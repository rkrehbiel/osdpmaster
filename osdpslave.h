#ifndef OSDPSLAVE_H
#define OSDPSLAVE_H

#include <boost/optional.hpp>

#include <cstring>

#include "uuid.h"

#include "blob.h"
#include "sync_queue.h"

typedef sync_queue < blob > blobqueue_t;

#define OSDPSLAVE_RETRY_MAX 10

static inline uint8_t next_seq(uint8_t seq) { return (seq == 3) ? 1 : ++seq; }

class osdpslave {
public:
	class busprotocol *m_bus;	// I'm a slave on this bus
	int16_t m_retry;			// What retry attempt; >0 means do rexmit

	struct timespec m_next_assign; // When to try another assignment
	struct timespec m_next_poll; // When to try another poll (after a miss)

	blobqueue_t m_msglist;		// Messages for this slave

	uuid_t m_uuid;				// factory-assigned UUID
	uint8_t m_addr;				// statically- or dynamically-assigned addr
	uint8_t m_txseq, m_rxseq;
	bool m_enabled;		  // whether it's enabled (polled, etc)
	bool m_setrtc;		  // Rather than dequeue from msglist, set RTC

public:
	osdpslave(busprotocol *bus);
	~osdpslave();

	void init(void);

	inline void push(blob msg) {
		m_msglist.push(msg);
	}

	inline bool empty() {
		return m_msglist.empty();
	}
	inline blob front() {
		blob empty;
		if(m_msglist.empty())
		   return empty;
		return m_msglist.front();
	}
	inline void pop() {
		m_msglist.pop();
	}

	void set_uuid(uuid_t u) { memcpy(m_uuid, u, sizeof(m_uuid)); }
	const uuid_t &uuid() const { return m_uuid; }
	uint8_t addr() const { return m_addr; }
	uint8_t addr(uint8_t a) { m_addr = a; return a; }

	bool defined() const;

	bool offline(void) const { return m_retry >= OSDPSLAVE_RETRY_MAX; }
	void ack(void);				// poll success (slave responded)
	void nak(void);				// poll failed

	inline uint8_t txseq() const { return m_txseq; }
	inline uint8_t rxseq() const { return m_rxseq; }
	inline class busprotocol *bus() { return m_bus; }

	inline bool enabled(void) const { return m_enabled; }
	inline bool enabled(bool yn) { bool t = m_enabled; m_enabled = yn; return t; }

	bool welcome(void);

	void retire(void);

	void purge(void);			// purge queued outgoing

	void declare_online(bool tf); // Report to the world whether I'm
								  // offline or online
};

bool mem_zero(const uint8_t *p, int size);

#endif // OSDPSLAVE_H
