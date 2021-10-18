#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <termios.h>
#include <sys/poll.h>
#include <pthread.h>

#include "log4cpp.h"

#include "crc16.h"

#include "osdpprotocol.h"

using namespace std;

bool operator<(const struct timespec &left, const struct timespec &right) {
	if(left.tv_sec < right.tv_sec)
		return true;
	if(left.tv_sec > right.tv_sec)
		return false;
	return left.tv_nsec < right.tv_nsec;
}

bool operator<=(const struct timespec &left, const struct timespec &right) {
	if(left.tv_sec < right.tv_sec)
		return true;
	if(left.tv_sec > right.tv_sec)
		return false;
	return left.tv_nsec <= right.tv_nsec;
}

std::ostream &operator<<(std::ostream &out, struct timespec &ts) {
	out << "{" <<
		ts.tv_sec << ":" <<
		ts.tv_nsec << "}";
	return out;
}

protocol::protocol(const struct serial_config *config) {
	m_my_addr = 0xFF;	// I'm a master, I see all addresses
	m_out_len = 0;
	memset(&m_next_write, 0, sizeof(m_next_write));
	m_config.baud = 115200;
	m_config.parity = 'N';
	m_config.bits = 8;
	m_config.stop = 1;
	m_config.flow = 'n';
	m_config.lead = m_config.trail = 0;
	m_config.timeout = 3000;	// 3000us default timeout
	m_config.delay = 600;		// 600us after receipt before xmit
	m_config.idle = 30;			// 3000us idle timer
	if(config)
		m_config = *config;
}

int protocol::prepcom(void) {
	m_fd = open(m_config.port, O_RDWR|O_NONBLOCK);
	if(m_fd < 0)
		throw protocol_exception("Failed to open COM port");

	struct termios dcb = { 0, };
	int i;

	// pre-populate the dcb with the current settings
	i = tcgetattr(m_fd, &dcb);
	if(i < 0)
		throw protocol_exception("Port settings can't be read");

	// Since I'm designed for device-control-style COM port, set the
	// port to "raw" mode
	cfmakeraw(&dcb);

	dcb.c_cflag |= CREAD|CLOCAL; // Enable receiver; ignore modem controls
	dcb.c_iflag &= ~(BRKINT|IGNBRK); // Observe and report BRK
									 // receipt, but no interrupt
	dcb.c_oflag &= ~CRTSCTS;	// Manual control of modem lines

	// I'm told that these support non-canonical mode & lowest latency
	dcb.c_cc[VMIN] = 1;			// minimum number of characters
								// buffered before read returns
	dcb.c_cc[VTIME] = 0;		// Wait 0 tenths of a second after
								// char arrival before returning data

	switch(m_config.flow)
	{
	case 'n': case 'N':
		break;					// Already set up
	case 'r': case 'R':
		dcb.c_cflag |= CRTSCTS;	// Do CTS/RTS flow control
		break;
	case 'x': case 'X':			// XON/XOFF flow control unsupported
	default:
		throw protocol_exception("Unknown flow control");
	}

	// Set "parity":
	switch(m_config.parity)
	{
	case 'n': case 'N':
		dcb.c_iflag |= IGNPAR;	// Ignore parity errors
		dcb.c_iflag &= ~INPCK;	// No parity checking
		dcb.c_cflag &= ~PARENB;	// Disable parity
		break;
	case 'E': case 'e':
		dcb.c_iflag |= INPCK|IGNPAR;	 // Input Parity checking
		dcb.c_iflag &= ~PARMRK; // without parity markers
		dcb.c_cflag |= PARENB;	// Enable parity
		dcb.c_cflag &= ~PARODD;	// Not odd parity
		break;
	case 'O': case 'o':
		dcb.c_iflag |= INPCK|IGNPAR;	// Input Parity checking
		dcb.c_iflag &= ~PARMRK; // without parity markers
		dcb.c_cflag |= PARENB;	// Enable parity
		dcb.c_cflag |= PARODD;	// Odd parity
		break;
	case 'm': case 'M':
	case 's': case 'S':
		// (* old comment *)
		// Sadly, Unix device drivers don't support Mark or Space
		// parity.
		// (* newer Linux drivers do.  But I don't yet have a reason
		// to support this. *)
	default:
		throw protocol_exception("Unsupported parity setting");
	}

	switch(m_config.bits)
	{
	case 7:
		dcb.c_cflag = (dcb.c_cflag & ~CSIZE) | CS7;
		dcb.c_iflag |= ISTRIP;	// Help me out; Strip received bytes
								// to the 7 data bits
		break;
	case 8:
		dcb.c_cflag = (dcb.c_cflag & ~CSIZE) | CS8;
		dcb.c_iflag &= ~ISTRIP; // Don't do 7 bit strip
		break;
	default:
		throw protocol_exception("Unsupported data bits size");
	}

	switch(m_config.stop)
	{
	case 1:
		dcb.c_cflag &= ~CSTOPB;
		break;
	case 2:
		dcb.c_cflag |= CSTOPB;
		break;
	default:
		throw protocol_exception("Unsupported stop bits");
	}

	int cbaud = -1;
	switch(m_config.baud)
	{
#define BAUDCASE(n) case n: cbaud = B##n; break
		BAUDCASE(50);
		BAUDCASE(75);
		BAUDCASE(110);
		BAUDCASE(134);
		BAUDCASE(150);
		BAUDCASE(200);
		BAUDCASE(300);
		BAUDCASE(600);
		BAUDCASE(1200);
		BAUDCASE(1800);
		BAUDCASE(2400);
		BAUDCASE(4800);
		BAUDCASE(9600);
		BAUDCASE(19200);
		BAUDCASE(38400);
		BAUDCASE(57600);
		BAUDCASE(115200);
		BAUDCASE(230400);
	default:
	{
		// How to set a custom baud rate:
		struct serial_struct nuts;
		i = ioctl(m_fd, TIOCGSERIAL, &nuts);
		if(i < 0)
			throw protocol_exception("Failed to read com port settings");
		nuts.custom_divisor = nuts.baud_base / m_config.baud;
		nuts.flags &= ~ASYNC_SPD_MASK;
		nuts.flags |= ASYNC_SPD_CUST;
		i = ioctl(m_fd, TIOCSSERIAL, &nuts);
		if(i < 0)
			throw protocol_exception("Failed to set com port settings (baud rate)");
		dcb.c_cflag |= B38400;
		break;
	}
	}
#undef BAUDCASE

	if(cbaud != -1)
	{
		// Set baud rate
		i = cfsetispeed(&dcb, cbaud);
		if(i < 0)
			throw protocol_exception("Failed to set com port baud rate");
		i = cfsetospeed(&dcb, cbaud);
		if(i < 0)
			throw protocol_exception("Failed to set com port baud rate");
	}

	// Now that my config block is prepared, set the tty...
	i = tcsetattr(m_fd, TCSANOW, &dcb);
	if(i < 0)
		throw protocol_exception("Failed to set com port serial parameters");

	if(!m_config.usb) { // FTDI RS485 wire doesn't accept RS485 ioctls
		struct serial_rs485 rs485;
		i = ioctl(m_fd, TIOCGRS485, &rs485); // whats' the current setup?
		rs485.flags |= SER_RS485_ENABLED; // Enable RS485 mode
		rs485.flags &= ~(SER_RS485_RTS_AFTER_SEND|SER_RS485_RTS_ON_SEND);
		//rs485.flags |= SER_RS485_RTS_AFTER_SEND;
		rs485.flags |= SER_RS485_RTS_ON_SEND;
		//rs485.flags |= (SER_RS485_RTS_AFTER_SEND|SER_RS485_RTS_ON_SEND);
		rs485.delay_rts_before_send = 0; // delay in ms from RTS-assert before xmit
		rs485.delay_rts_after_send = 0;	// delay after xmit until RTS-negate

		i = ioctl(m_fd, TIOCSRS485, &rs485); // establish
		if(i < 0)
			throw protocol_exception("Failed to set RS485 parameters");
	}

	i = tcflush(m_fd, TCIOFLUSH);	// flush out any buffered bytes
	if(i < 0)
		throw protocol_exception("Failed to flush FIFO buffers");

	memset(&m_next_write, 0, sizeof(m_next_write));

    return 0;	// Done
}

protocol::~protocol() {
	close();					// call self-closer.
}

void protocol::close(void) {
	if(m_fd != -1)
		::close(m_fd);
	m_fd = -1;
}

int protocol::read_some(int offset, int count) {
	for(;;) {
		memo("read...");
		errno = 0;
		int size = 
			::read(m_fd, m_in_buffer+offset, count); // Read up to that many chars
		if(size < 0) {
			int e = errno;
			memo("size=%d errno=%d", size, e);
			if(e == EINTR)
				continue;		// Just go again
			if(e == EWOULDBLOCK || e == EAGAIN) {
				// No data to read.
				// Wait until the fd is readable...
				struct pollfd fds[1];
				fds[0].events = POLLIN;
				fds[0].fd = m_fd;
				fds[0].revents = 0;

				struct timespec tick, delta;
				clock_gettime(CLOCK_MONOTONIC, &tick);
				delta.tv_sec = m_deadline.tv_sec - tick.tv_sec;
				delta.tv_nsec = m_deadline.tv_nsec - tick.tv_nsec;
				if(delta.tv_nsec < 0) {
					delta.tv_sec -= 1;
					delta.tv_nsec += 1000000000;
				}
				long itime; // nanoseconds to milliseconds
				itime = delta.tv_nsec / 1000000;
				itime += delta.tv_sec * 1000;
				if(itime < 0)
					return PROTO_ERR_TIMEOUT; // Too late, baby
				if(itime > 1000)
					itime = 1000; // capped at one second
				if(itime == 0)
					itime++;	// wait at least 1 ms

				memo("poll timer=%d", itime);
				int i = poll(fds, 1, itime);
				// (I don't even care about i)
				continue;			// try to read again
			}

			throw protocol_exception("read I/O error");
		}
		rlog(m_in_buffer+offset, size);
		return offset+size;		// How much buffer is occupied now
	}
	/*NOTREACHED*/

	throw protocol_exception("read I/O error");
}

int protocol::readsoh() {
	// Skip received characters until SOH
	// read four leading message bytes (SOH addr len-LSB len-MSB)
	for(;;) {
		memset(m_in_buffer, 0, sizeof(struct osdp_common));
		int32_t offset = read_some(0, sizeof(struct osdp_common));
		if(offset < 0)
			return offset;		// some error
		unsigned char *soh = (unsigned char *)
			memchr(m_in_buffer, chSOH, offset);
		if(soh != NULL) {
			// I received SOH.  Move it to the start of the buffer,
			// then read the rest of the header
			if(soh > m_in_buffer) {
				int shift = soh - m_in_buffer;
				int size = offset - shift;
				memmove(m_in_buffer, soh, size);
				offset -= shift;
			}
			while(offset < (int)sizeof(struct osdp_common)) {
				offset = read_some(offset,
						sizeof(struct osdp_common)-offset);
				if(offset < 0)
					return offset;	// Read error
			}
			return offset; // That's it, I've got 5.
		}
		// (if no SOH, then discard everything I read.)
	}
	/*NOTREACHED*/
	return 0;
}

int protocol::flyby(int offset) {
	// The OSDP protocol spec says that slaves must read AND ERROR-CHECK
	// all bus messages, even if they are not addressed to themselves.
	// Furthermore, it says any bus occupant must permit the fly-by
	// of messages addressed to other slaves of up to 1440 bytes -
	// larger than my own buffer.
	int total, size = offset;
	struct osdp_common *hdr = (struct osdp_common *)m_in_buffer;
	total = hdr->len[0] | (hdr->len[1] << 8);
	uint8_t ctrl = hdr->ctrl;
	uint16_t crc;
	uint8_t checksum;
	if(ctrl & 0x04) {
		crc16_prepare(crc);
		crc = crc16_add(crc, m_in_buffer, offset);
		total -= 2;		// Don't fly past the CRC
	}
	else {
		checksum = 0;
		for(int i = 0; i < offset; i++)
			checksum += m_in_buffer[i];
		total--;		// Dont fly past the checksum byte
	}
	while(size < total) {
		// bytes come in, we error-check them, we
		// don't keep them.
		size_t count = total - size;
		if(count > sizeof(m_in_buffer))
			count = sizeof(m_in_buffer); // don't read too much.
		offset = read_some(0, count);
		if(offset < 0)
			return offset;	// error
		if(ctrl & 0x04)
			crc = crc16_add(crc, m_in_buffer, offset);
		else {
			for(int i = 0; i < offset; i++)
				checksum += m_in_buffer[i];
		}
		size += offset;
	}
	// Now read the checksum/CRC
	if(ctrl & 0x04) {
		crc = crc16_digest(crc);
		offset = 0;
		while(offset < 2) {
			offset = read_some(offset, 2-offset);
			if(offset < 0)
				return offset; // Oh!  So close...
		}
		uint16_t msgcrc = m_in_buffer[0] |
			(m_in_buffer[1] << 8);
		if(crc != msgcrc)
			return PROTO_ERR_CRC;
	}
	else {
		offset = read_some(0, 1);
		if(offset < 0)
			return offset; // Oh!  So close...
		if(m_in_buffer[0] != checksum)
			return PROTO_ERR_CRC;
	}
	return PROTO_ERR_FLYBY;	// flyby success.
}

int protocol::readeof(int offset) {
	// readsoh provided the first 4 bytes: SOH, addr, len.
	// Now read the full message indicated by count, or timeout.

	int total;
	struct osdp_common *hdr = (struct osdp_common *)m_in_buffer;
	total = hdr->len[0] | (hdr->len[1] << 8);
	if(total <= 5 || total > (int)sizeof(m_in_buffer) - offset)
		return PROTO_ERR_OVERFLOW;
	for(;;) {
		offset = read_some(offset, total - offset);
		if(offset < 0)
			return offset;		// Read error
		if(offset >= total) {
			return offset;		// Read complete.
		}
	}					// iterate, accumulate msg bytes
	/*NOTREACHED*/
	return -1;
}

void protocol::readstamp(void) {
	clock_gettime(CLOCK_MONOTONIC, &m_next_write);
	// Assume m_delay.tv_sec is zero - when would delay be more than a
	// second?)
	if((m_next_write.tv_nsec += (m_config.delay * 1000)) >= 1000000000) {
		m_next_write.tv_nsec -= 1000000000;
		m_next_write.tv_sec++;
	}
}

void protocol::delaywait(void) {
	if(m_next_write.tv_sec == 0 && m_next_write.tv_nsec == 0)
		readstamp();			// Don't know when the last read was

	// one simple API sleeps until the monotonic time is reached.
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &m_next_write, NULL);
}

int protocol::readcook(void) {
	log4cpp::Category &root=log4cpp::Category::getRoot();
	clock_gettime(CLOCK_MONOTONIC, &m_deadline);
	auto qr = div((long)m_config.timeout, (long)1000000);
	m_deadline.tv_sec += qr.quot;
	m_deadline.tv_nsec += qr.rem * 1000;
	if(m_deadline.tv_nsec >= 1000000000) {
		m_deadline.tv_nsec -= 1000000000;
		m_deadline.tv_sec += 1;
	}

	int size2 = 0, size = readsoh();
	if(size < 0) {
		goto leave;
	}
	if(m_my_addr != 0xFF) { // If I'm a master, I see all
		struct osdp_common *hdr = (struct osdp_common *)m_in_buffer;
		if(hdr->addr != m_my_addr &&
			hdr->addr != 0x7D &&
			hdr->addr != 0x7F) {
			size = flyby(size); // let it pass me by
			goto leave;
		}
	}

	size2 = readeof(size);
	if(size2 < 0) {
		size = size2;			// Tell caller of I/O error
		goto leave;
	}
	size = size2;

	{
		// integrity check...
		// OSDP ctrl flag says whether checksum or CRC16 was supplied.
		struct osdp_common *hdr =
				(struct osdp_common *)m_in_buffer;

		if(hdr->ctrl & 0x04) {
			uint16_t crc, mycrc;
			crc16_prepare(crc);
			crc = crc16_add(crc, m_in_buffer, size-2);
			mycrc = crc16_digest(crc);
			uint16_t msgcrc = m_in_buffer[size-2] |
				(m_in_buffer[size-1] << 8);
			if(msgcrc != mycrc) {
				size = PROTO_ERR_CRC;
				goto leave;
			}
			size = size-2;			// size minus checksum
		}
		else {
			uint8_t csum = 0, *mptr = m_in_buffer, *end=m_in_buffer + size - 1;
			while(mptr < end)
				csum += *mptr++;
			if(csum != m_in_buffer[size-1]) {
				size = PROTO_ERR_CRC;
				goto leave;
			}
			size -= 1;
		}
	}

leave:
	readstamp();				// Set m_next_time
	return size;				// size minus checksum
}

void protocol::flush_input(void) {
	int i = tcflush(m_fd, TCIFLUSH);	// flush out any waiting input
	if(i < 0)
		throw protocol_exception("Failed to flush input buffer");
	
}

int protocol::waitidle(void) {
	// Wait until the bus is idle, as specified by OSDP 2.8
	// "Synchronization". The spec says "The nominal value of the
	// inter-character timeout shall be 20 milliseconds".  But at 115K
	// baud, that's about 230 character-times!  So it's configurable.
	for(;;) {
		clock_gettime(CLOCK_MONOTONIC, &m_deadline);
		{
			auto qr = div((long)m_config.idle, (long)100000); // idle is microseconds
			m_deadline.tv_sec += qr.quot;
			m_deadline.tv_nsec += qr.rem * 1000;
			if(m_deadline.tv_nsec >= 1000000000) {
				m_deadline.tv_sec++;
				m_deadline.tv_nsec -= 1000000000;
			}
		}
		// Get & discard some bytes

		int i = read_some(0, 4); // Read & discard some chars
		if(i == PROTO_ERR_TIMEOUT) {
			return i;			// Heard nothing.
		}

		// Iterate, recalculate timeout.  :-(
	}

	return 0;
}

void protocol::readreset(void) {
}

int protocol::writecook(int addr, int seq, int size, const uint8_t *payload) {
	// copy to m_buffer with proper envelope
	unsigned char *out = m_out_buffer, *postpad;
	int fullsize = size + 7;	// add wrapping bytes
	for(int i = 0; i < m_config.lead; i++)
		*out++ = 0xFF;				// OSDP requirement (2.7, "Timing")

	postpad = out;
	*out++ = chSOH;
	*out++ = addr & 0x7F;
	*out++ = fullsize & 0xff;
	*out++ = (fullsize >> 8) & 0xff;
	*out++ = seq | 0x04;		// seq#, CRC indicator

	if(size > 0) {
		memmove(out, payload, size);
		out += size;
	}
//#define DEBUG_PROTOCOL
#ifdef DEBUG_PROTOCOL
	if(fullsize > 100){
		PRINTF("WC size (%d)\n", fullsize);
	}
#endif
	uint16_t crc, icrc;
	crc16_prepare(crc);
	crc = crc16_add(crc, postpad, out-postpad);
	icrc = crc16_digest(crc);
	*out++ = icrc & 0xFF;
	*out++ = (icrc >> 8) & 0xFF;

	for(int i = 0; i < m_config.trail; i++)
		*out++ = 0xFF;

	delaywait();				// Wait until it's okay to send

	m_out_len = out - m_out_buffer;	// size of written msg (for rexmit)

	return protocol::write(m_out_buffer, m_out_len);
}

int protocol::write(int size) {
	m_out_len = size;
	return write(m_out_buffer, size);
}

int protocol::write(const uint8_t *buffer, int size) {
	// Transmit!
	int offset = 0;

	xlog(buffer, size);

	while(offset < size) {
		int i = ::write(m_fd, m_out_buffer + offset, size - offset);

		if(i == 0) {
			errno = EAGAIN;		// Turn 0-length into "wait"
			i = -1;
		}
		if(i < 0) {
			switch(errno) {
			case EINTR:
				continue;		// interrupt signal, just re-issue the
								// same write
			case EAGAIN:
#if EAGAIN != EWOULDBLOCK
			case EWOULDBLOCK:
#endif
			{
				// Wait until the fd is writable.
				struct pollfd fds[1];
				fds[0].events = POLLOUT;
				fds[0].fd = m_fd;
				fds[0].revents = 0;

				i = poll(fds, 1, 1000);
				continue;			// Go try again
			}
			default:
				throw protocol_exception("write I/O error");
			}
		}
		offset += i;
	}

	return size;
}

int protocol::resend() {
	if(m_out_len <= 0) {
		memo("resend() but nothing to resend!");
		return 0;
	}

	delaywait();				// Wait until it's okay to send
	return protocol::write(m_out_len);
}

void protocol::xlog(const uint8_t *buffer, int size) {
	// unimplemented
}

void protocol::rlog(const uint8_t *buffer, int size) {
	// unimplemented
}

void protocol::memo(const char *str, ...) {
	// unimplemented
}
