#ifndef OSDPMASTER_H
#define OSDPMASTER_H

#include <iomanip>
#include <sstream>
#include <mosquitto.h>
#include "log4cpp.h"

#include "osdpprotocol.h"

class logprotocol: public protocol {
public:
	logprotocol(struct serial_config *config)
		: protocol(config) {
	}

	virtual ~logprotocol() {
	}

protected:
	static void datalog(const char *prefix, const uint8_t *buffer, int len);

	virtual void xlog(const uint8_t *buffer, int len) {
		datalog("TX", buffer, len);
	}

	virtual void rlog(const uint8_t *buffer, int len) {
		datalog("RX", buffer, len);
	}

};

class busprotocol: public logprotocol {
public:
	typedef std::list<osdpslave> slavelist_t;
	slavelist_t m_slaves; 	// here I instantiate the slaves I address
	slavelist_t::iterator m_poll_slave;

	// The queue for BUS messages
	blobqueue_t m_msglist;

	katomic_t m_crc_count;
	katomic_t m_timeout_count;

	struct mosquitto *m_mq;		// where outgoing messages can be
								// posted

	const char *m_port2;		// Alternate port name

public:
	busprotocol(struct serial_config *config)
		: logprotocol(config),
		m_crc_count(0), m_timeout_count(0) {
		m_poll_slave = m_slaves.end();
		m_port2 = NULL;
	}

	virtual ~busprotocol() {
	}

	osdpslave &add_slave(const char *addr);

	inline bool empty() {
		return m_msglist.empty();
	}
	inline blob front() {
		return m_msglist.front();
	}
	inline void pop() {
		m_msglist.pop();
	}

	inline void push(blob msg) {
		m_msglist.push(msg);
	}

	void init();
	void run();

	typedef enum {
		DID_POLL, DIDNT_POLL
	} polled_t;
	polled_t slave_poll(void);

	bool id_slave(const uuid_t uuid);

	inline struct mosquitto *mq(void) { return m_mq; }

	inline const char *port2(void) const { return m_port2; }
	inline const char *port2(const char *n) { const char *t = m_port2; m_port2 = n; return t; }
};

void mosq_errcheck(int mosqe, const char *context);


#endif // OSDPMASTER_H
