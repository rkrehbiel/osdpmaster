#include "osdpslave.h"
#include "osdpmaster.h"

#include "timespec.h"

using namespace std;

// utility: optimized test-memory-all-zero
bool mem_zero(const uint8_t *p, int size) {
	// Cover unaligned leading bytes...
	while(((intptr_t)p & 0x07) != 0 && size > 0) {
		if(*p++ != 0)
			return false;
		size--;
	}
	// Speed over aligned interim bytes...
	uint32_t *lp = (uint32_t *)p; // Work via long words
	while(size >= 4) {
		if(*lp++ != 0)
			return false;
		size -= 4;
	}
	// Finish up trailing few bytes
	p = (uint8_t *)lp;
	while(size > 0) {
		if(*p++ != 0)
			return false;
		size--;
	}
	return true;
}

osdpslave::osdpslave(class busprotocol *bus)
	: m_bus(bus), m_retry(OSDPSLAVE_RETRY_MAX+1),
	  m_next_assign{0,0},
	  m_txseq(0), m_rxseq(0), m_addr(0xFF),
	  m_setrtc(false), m_enabled(true) {
	memset(m_uuid, 0, sizeof(m_uuid));
}

osdpslave::~osdpslave() {
	retire();
}

bool osdpslave::defined(void) const {
	return(m_addr != 0xFF || !mem_zero(m_uuid, sizeof(m_uuid)));
}

void osdpslave::purge(void) {
	while(!m_msglist.empty())
		m_msglist.pop();
}

void osdpslave::retire(void) {
	// return queued msg blocks to free memory
	purge();
	memset(m_uuid, 0, sizeof(m_uuid)); // bye bye
	m_addr = 0xFF;
	m_retry = OSDPSLAVE_RETRY_MAX+1;
}

void osdpslave::init(void) {
	purge();
	m_retry = OSDPSLAVE_RETRY_MAX + 1; // Initially, failed
}

// sequence numbers:

// The slave replies with the same sequence number it was given.
// The slave has two replies ready to send: either a new reply,
// or the last reply sent.

// If the slave sees a poll with a repeat of the last sequence,
// it re-sends the last reply, otherwise it sends the new reply.
// In either case, it sends the same sequence number (old or new)
// contained in the poll.

// If the slave sees a poll with a zero sequence number, then
// the master has restarted.  It can't tell if the last message
// was received or not.  To be safe, re-send that last msg (its
// sequence number needs to be zeroed, and CRC recalculated).

// So, how could the master see a sequence other than the one in
// the poll?  Some downstream module failure (dodgy software),
// communications error (should have been a CRC error), UART FIFO
// failure (master's own fault).

void osdpslave::ack(void) {
	m_txseq = next_seq(m_txseq);
	if(offline()) {				// I thought I was offline?
		declare_online(true);
	}
	m_retry = 0;				// Retry count zero
}

void osdpslave::nak(void) {
	if(offline())
		return;					// not changed, still offline
	m_retry++;					// needs a retry
	if(offline()) {				// state changed?
		log4cpp::Category &root = log4cpp::Category::getRoot();
		root.info("slave %d offline", (int)addr());
		declare_online(false);	// publish.
	}
	else {
		clock_gettime(CLOCK_MONOTONIC, &m_next_poll);
		m_next_poll.tv_nsec += 10000000;
		if(m_next_poll.tv_nsec >= 1000000000) {
			m_next_poll.tv_nsec -= 1000000000;
			m_next_poll.tv_sec++;
		}
	}
}

bool osdpslave::welcome(void) {
	// I don't know what the sequence numbers would be...
	m_txseq = 0; // As an indicator, make this 0
	// Slave just talked...
	m_retry = 0;
	declare_online(true);
	return true;
}

void osdpslave::declare_online(bool tf) {
	ostringstream topic;
	if(mem_zero(uuid(), 16)) {
		topic << "osdpiom/bus1/incoming/" << (int)addr() << "/status";
	}
	else {
		char sayuuid[36];
		uuid_unparse(uuid(), sayuuid);
		topic << "osdpiom/bus1/incoming/" << sayuuid << "/status";
	}
	const char *val = "OFFLINE";
	if(tf)
		val = "ONLINE";
	if(!enabled())
		val = "DISABLED";
	// Set final arg "true" to make this the "first message" of the
	// topic.  The MQTT broker will keep a copy, and any app that
	// subscribes will get this message first, and then any other
	// messages, including new "firsts".
	log4cpp::Category &root = log4cpp::Category::getRoot();
	root.info("Publishing status %s = %s",
			  topic.str().c_str(), val);
	int mosqe =
		mosquitto_publish(bus()->mq(), NULL, topic.str().c_str(),
						  strlen(val), val, 1, true);
	mosq_errcheck(mosqe, "mosquitto_publish");
}
