#ifndef OSDPPROTOCOL_H_
#define OSDPPROTOCOL_H_

#include "osdp_def.h"

#include <string>
#include <exception>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>

#include <cstdint>

#define PROTO_ERR_TIMEOUT -1
//#define PROTO_ERR_IO -2
#define PROTO_ERR_OVERFLOW -3
#define PROTO_ERR_NOEOD -4
#define PROTO_ERR_ESCAPE -5
#define PROTO_ERR_CRC -6
#define PROTO_ERR_FLYBY -7
#define PROTO_ERR_DELAYED -8

#define OSDPPROTO_BUFFER_SIZE 1024

class protocol_exception: public std::exception {
protected:
	std::string msg;
public:
	protocol_exception(const char *a_msg)
		: std::exception() {
		msg = a_msg;
	}

protocol_exception(const std::string &a_msg)
		: std::exception() {
		msg = a_msg;
	}

	virtual ~protocol_exception() throw() {
		// Nothing required
	}

	const char *what() const throw() {
		return msg.c_str();
	}
};

struct serial_config {
	const char *port;			// serial port device name
	int baud;
	char parity;
	char bits;
	char stop;
	char flow;
	char usb;
	char lead;					// How many leading sync chars
	char trail;					// How many trailing sync chars
	long timeout;		   // read timeout, MICROseconds
	long delay;			   // MICROseconds after receipt before rexmit
	long idle;			   // idle timeout (MICROseconds)
};

// An instance of "protocol" manages the serial protocol to a COM port.

class protocol {
protected:
	boost::mutex m_protolock;	// serialize access when needed

	int m_fd;			// COM port file descriptor
	
	// Serial port settings
	struct serial_config m_config; // serial params

#define SLAVE_BUFF_SIZE (256/*payload*/ + 5/*OSDP_MFGREP*/ + 5/*sizeof(osdp_common)*/ + 2/*CRC*/ + 1/*FF sync*/ + 2)
	unsigned char m_in_buffer[SLAVE_BUFF_SIZE];	// receive buffer... let's make it slightly larger than out.
	unsigned char m_out_buffer[SLAVE_BUFF_SIZE]; // transmit buffer
	uint16_t m_out_len;					// Length of last sent message

	uint8_t m_my_addr;					// My address.

	struct timespec m_next_write;	// when the next xmit can happen

	struct timespec m_deadline;	// The time at which the current read
								// will timeout

	void readstamp(void);		// compute m_next_write from NOW
	void delaywait(void);		// wait until m_next_write

	int read_some(int offset, int max);
	int readsoh();
	int readeof(int offset);	// after SOH, reads message
	int flyby(int offset); // fly past other device's msg

	int write(int len);

	virtual void xlog(const uint8_t *data, int len); // log transmitted
	virtual void rlog(const uint8_t *data, int len); // log received
	virtual void memo(const char *str, ...); // memo log

public:

	protocol(const struct serial_config *config);
	~protocol();

	int prepcom(void);			// Open & prep com port

	int readcook(void);		// read(), check timing, framing, CRC

	int writecook(int addr, int seq, int size, const uint8_t *payload);

	int write(const uint8_t *buffer, int size);
	int resend();

	int waitidle(void);			// Wait until inter-message gap

	void readreset(void);				// Reset receive path

	void flush_input(void);		// Discard any pending input

	void close(void);			// close port

	const unsigned char *in_msg() const { return m_in_buffer; }

	inline long timeout(void) const { return m_config.timeout; }
	inline long timeout(long t) { long old_t = m_config.timeout; m_config.timeout = t; return old_t; }

	inline long idle(void) const { return m_config.idle; }
	inline long idle(long t) { long old_t = m_config.idle; m_config.idle = t; return old_t; }

	inline uint8_t addr() const { return m_my_addr; }
	inline uint8_t addr(uint8_t addr) { uint8_t x = m_my_addr; m_my_addr = addr; return x; }

	inline uint8_t *out_buffer() { return m_out_buffer; }
	inline int out_buffer_size() const { return sizeof(m_out_buffer); }

	inline long delay() const { return m_config.delay; }
	inline int baud() const { return m_config.baud; }
};

#endif /* OSDPPROTOCOL_H_ */
