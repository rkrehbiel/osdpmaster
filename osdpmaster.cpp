// Can I do this?  Make a Linux app that polls OSDP?

#include <unistd.h>
#include <pthread.h>

#include <iostream>
#include <sstream>
#include <iomanip>

#include <vector>

#include <cassert>

#include <mosquitto.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "osdpslave.h"
#include "osdpmaster.h"

#include "split.h"

using namespace std;
using namespace boost;

property_tree::ptree g_config;		// The bus config document

std::string mqtt_host = "localhost";
int mqtt_port = 1883;
std::string mqtt_user;
std::string mqtt_password;

void mosq_errcheck(int mosqe, const char *context) {
	if(mosqe != MOSQ_ERR_SUCCESS) {
		log4cpp::Category &root = log4cpp::Category::getRoot();
		root.error("%s %d %s", context, mosqe, mosquitto_strerror(mosqe));
	}
}

void logprotocol::datalog(const char *prefix, const uint8_t *buffer, int len) {
	// Since log4cpp insists every log is a line,
	// build whole lines
	while(len > 0) {
		int somelen = len;
		if(somelen > 16)
			somelen = 16;
		std::ostringstream line;
		line.fill('0');
		line.setf(ios::hex|ios::uppercase,
				  ios::basefield|ios::uppercase);
		const char *space = "";
		for(int i = 0; i < somelen; i++) {
			line << space << setw(2) << (int)buffer[i];
			space = " ";
		}
				
		log4cpp::Category &root = log4cpp::Category::getRoot();
		root.debug("%s: %s", prefix, line.str().c_str());

		buffer += somelen;
		len -= somelen;
	}
}

osdpslave &busprotocol::add_slave(const char *addr) {
	// A static slave.
	uint32_t a = strtoul(addr, NULL, 10);
	m_slaves.push_back(osdpslave(this));
	osdpslave &s = m_slaves.back();
	s.addr(a);
	s.init();

	m_poll_slave = m_slaves.begin();
	return s;
}

void busprotocol::init(void) {
	// add_slave already took care of this
}

void *busprotocol_run(void *param) {
	busprotocol *proto = (busprotocol *)param;
	try {
		proto->run();
	}
	catch(std::exception &e) {
		cerr << e.what() << endl;
	}
	return 0;
}

void busprotocol::run() {
	log4cpp::Category &root = log4cpp::Category::getRoot();

	bool first = true;
	for(;;) {
		try {
			prepcom();			// Connect & prepare to run the port
			break;				// got it
		}
		catch(protocol_exception e) {
			if(first) {
				root.error("Failed to open %s, waiting...", m_config.port);
				first = false;
			}
		}
		if(m_port2) {
			// Switch ports
			const char *t = m_config.port;
			m_config.port = m_port2;
			m_port2 = t;
		}
		sleep(5);
	}
	root.error("Port %s opened", m_config.port);
	root.error("m_config.delay = %u", m_config.delay);
	root.error("m_config.timeout = %u", m_config.timeout);
	for(;;) {
		polled_t polled = DIDNT_POLL;

		/*
		 * There's a queue of outgoing messages per slave, and
		 * there's a bus-global queue.  The bus-global queue is for
		 * address-assignment messages and inquiries, and
		 * I give it precedence.
		 */

		try {
			polled = slave_poll();
		}
		catch(protocol_exception e) {
			log4cpp::Category &root = log4cpp::Category::getRoot();
			root.error("Error while polling, %s", e.what());

			close();			// I/O error (like, unplugged USB cable)

			for(;;) {
				try {
					prepcom();
					break;		// Reopened
				}
				catch(protocol_exception e) {
					// didn't reopen.
				}
				if(m_port2) {
					// Switch ports
					const char *t = m_config.port;
					m_config.port = m_port2;
					m_port2 = t;
				}
				sleep(5);		// wait a while
			}
			root.error("port %s reopened", m_config.port);
		}

	} // end forever
}

void xparse_config(struct serial_config &config, char **port2) {
	{
		auto portname = g_config.get_optional<string>("port.device");
		if(!portname) {
			throw protocol_exception("Config does not specify the device name");
		}
		// allocate a port name buffer that will persist
		auto port = (char *)malloc((*portname).size() + 1);
		strcpy(port, (*portname).c_str());
		config.port = port;
	}

	{
		auto portname = g_config.get_optional<string>("port.device2");
		if(portname) {
			// allocate a port name buffer that will persist
			auto port = (char *)malloc((*portname).size() + 1);
			strcpy(port, (*portname).c_str());
			*port2 = port;
		}
	}

	config.baud = g_config.get<int>("port.baud", 57600);
	{
		string parity = g_config.get<string>("port.parity", "N");
		config.parity = parity[0];
	}
	config.bits = g_config.get<int>("port.data", 8);
	config.stop = g_config.get<int>("port.stop", 1);
	{
		string flow = g_config.get<string>("port.flow", "N");
		config.flow = flow[0];
	}
	config.usb = g_config.get<bool>("port.usb");

	config.lead = g_config.get<int>("port.lead", 0);
	config.trail = g_config.get<int>("port.trail", 0);
	config.timeout = g_config.get<int>("port.timeout", 100000);
	config.delay = g_config.get<int>("port.delay", 300);
	config.idle = g_config.get<int>("port.idle", 300);
}

busprotocol::polled_t busprotocol::slave_poll(void) {
	log4cpp::Category &root = log4cpp::Category::getRoot();
	auto a = m_poll_slave;
	bool sendmsg = false;

	for(;;) {
		a++;					 // Next
		if(a == m_slaves.end()) // Past the end?
			a = m_slaves.begin(); // roll back to start
		if(a == m_poll_slave)
			break;				// I went all the way 'round
		if(a->defined() && a->enabled())
			break;				// Found the next defined slave
	}

	m_poll_slave = a;
	osdpslave &s = *a;

	if(!s.defined() || !s.enabled()) // None pollable.
		return DIDNT_POLL;

	if(s.offline()) {
		s.purge(); // clear out queued msgs (sorry dude)

		// How to re-acquire a stopped slave: send address-assign
		// every 5 seconds.

		// Is it time to try another poll?
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		//if(now < s.m_next_assign)
		if(now.tv_sec < s.m_next_assign.tv_sec ||
		   (now.tv_sec == s.m_next_assign.tv_sec &&
			now.tv_nsec < s.m_next_assign.tv_nsec)) {
			return DIDNT_POLL;		// Not time to tickle.
		}

		// It needs a normal poll.
		s.m_next_assign = now;
		s.m_next_assign.tv_sec += 5; // 5 seconds until the next tickle
		unsigned char pollmsg[1];
		pollmsg[0] = OSDP_POLL;
		flush_input();
		writecook(s.addr(), s.txseq(), sizeof(pollmsg), pollmsg);
	}
	else {
		// Module thought online, but it missed a poll
		if(s.m_retry > 0) {
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			if(now.tv_sec < s.m_next_poll.tv_sec ||
			   (now.tv_sec == s.m_next_poll.tv_sec &&
				now.tv_nsec < s.m_next_poll.tv_nsec)) {
				return DIDNT_POLL;
			}
		}
		// send it real stuff
		if(!s.empty()) {
			blob msg = s.front();
			flush_input();
			writecook(s.addr(), s.txseq(), msg.size(), (const uint8_t *)msg.pvoid());
			sendmsg = true;
		}
		else {
			// Nothing else to say, plain poll.
			unsigned char pollmsg[1];
			pollmsg[0] = OSDP_POLL;
			flush_input();
			writecook(s.addr(), s.txseq(), sizeof(pollmsg), pollmsg);
		}
	}

	int size = readcook();
	// prepare to work the reply
	if(size > 0) { // I rx okay, work seq
		struct osdp_common_flex *omsg =
			(struct osdp_common_flex *)m_in_buffer;
		if((omsg->m.addr & 0x7F) != s.addr()) {
			// Received from wrong address???
			// Ignore.  Gah.
			return DID_POLL;	// (but, I did poll.)
		}
		uint8_t seq = omsg->m.ctrl & 0x03;	// Get proto recvd sequence #
		if(s.m_rxseq == 0 || seq == 0) {
			s.m_rxseq = next_seq(seq);	// Determine next seq
			// receipt okay, new sequence
		}
		else {
			// OSDP document is not clear about the master's ("CP")
			// behavior regarding the slave's ("PD") sequence numbers.
			// If the CP does not hear a reply correctly, it asks again,
			// with the same sequence number, which signals the PD to repeat
			// the last message.  If the PD does not hear a poll correctly,
			// the CP will poll it again (with the same seq).
			// It does not explain behavior re the PD's receipt of CP
			// sequence numbers.
			// I'm going to behave as if they don't matter.
		}
		if(omsg->data[0] == OSDP_NAK ||
			omsg->data[0] == OSDP_BUSY) {

			if(omsg->data[0] == OSDP_NAK){
				s.m_txseq = 0;
			}

			s.m_rxseq = next_seq(seq);
			return DID_POLL;		// not an ACK, but I polled
		}

		s.ack();		// I can xmit the next seq next time.
		if(sendmsg)
			s.pop();			// No rexmit

		// Handle received messages here
		if(omsg->data[0] != OSDP_ACK) { // (not the mundane stuff)
			ostringstream topic;
			topic << "osdp/bus1/incoming/" << (omsg->m.addr & 0x7F);
			// The messages start from the func byte.
			// SOH addr LSM MSB flag func
			// [0] [1]  [2] [3] [4]  [5]
			int mosqe =
				mosquitto_publish(mq(), NULL, topic.str().c_str(),
								  size-5, m_in_buffer + 5, 1, false);
			mosq_errcheck(mosqe, "mosquitto_publish");
			// Off it goes.
		}
	}
	else {
		root.debug("** %d", size);
		//root.info("readcook error %d", (int)size);
		s.nak();				// Sorry, didn't succeed
		// Handle errors, like timeout, for declaring modules STOPPED
		if(size == PROTO_ERR_CRC)
			katomic_inc(&m_crc_count);
		else if(size == PROTO_ERR_TIMEOUT)
			katomic_inc(&m_timeout_count);
	}
	return DID_POLL;
}

void message_for_slave(osdpslave &s, const std::vector<string> &components,
					   const struct mosquitto_message *message) {
	log4cpp::Category &root = log4cpp::Category::getRoot();
	// What name to call this slave
	char name[40];
	snprintf(name, sizeof(name), "%d", s.addr());
		
	if(components.size() == 5 && components[4] == "control") {
		// The message is a command, right now only DISABLE and ENABLE supported
		string cmd((const char *)message->payload, message->payloadlen);
		root.info("%s %d", cmd.c_str(), name);

		bool o = s.offline();	// What online state
		if(cmd == "DISABLE")
			s.enabled(false);
		else if(cmd == "ENABLE") {
			s.enabled(true);
			o = true;			// When I'm enabling a slave, it's
								// prior state was "offline"
		}
		s.declare_online(o);
	}
	else {
		if(!s.enabled()) {
			root.info("Message for disabled slave %s ignored", name);
		}
		else {
			root.info("Delivered msg %ld to %s", (long)message->mid, name);
			// This messages queues to this slave.
			s.push(blob(message->payload, message->payloadlen));
		}
	}
}

void message_callback(struct mosquitto *mosq, void *obj,
					  const struct mosquitto_message *message) {
	// Pick apart the topic
	auto proto = *(busprotocol **)obj;
	vector<string> components;
	split(string(message->topic), components, '/');
	// [0] = osdp
	// [1] = bus%d (should be 1)
	// [2] = outgoing
	// [3] = OSDP address
	// [4] = "control", if components.size() == 5

	assert(components[0] == "osdp");
	assert(components[1] == "bus1");
	assert(components[2] == "outgoing");
	unsigned long addr = strtoul(components[3].c_str(), NULL, 10);
	for(auto i = proto->m_slaves.begin(); i != proto->m_slaves.end(); i++) {
		osdpslave &s = *i;
		if(s.addr() == addr) {
			message_for_slave(s, components, message);
			return;
		}
	}
}

void log_callback(struct mosquitto *mq, void *user, int level, const char *msg) {
	log4cpp::Category &root = log4cpp::Category::getRoot();

	switch(level) {
	case MOSQ_LOG_INFO:
		root.info(msg);
		break;
	case MOSQ_LOG_WARNING:
	case MOSQ_LOG_NOTICE:
		root.warn(msg);
		break;
	case MOSQ_LOG_ERR:
		root.error(msg);
		break;
	case MOSQ_LOG_DEBUG:
		root.debug(msg);
		break;
	}
}

int main(int argc, char *argv[]) {
	struct serial_config config;
	busprotocol *proto;
	int frombaud = -1;
	memset(&config, 0, sizeof(config));

	{
		int inarg, outarg;
		for(inarg = outarg = 1; inarg < argc;) {
			if(strcmp(argv[inarg], "-b") == 0) {
				inarg++;
				frombaud = strtol(argv[inarg++], NULL, 10);
			}
			else
				argv[outarg++] = argv[inarg++];
		}
		argv[outarg] = NULL;
		argc = outarg;
	}

	// Grab program settings
	boost::property_tree::ini_parser::read_ini("osdpmaster.ini", g_config);

	// Get MQTT settings
	mqtt_host = g_config.get<std::string>("mqtt.host", "localhost");
	mqtt_port = g_config.get<int>("mqtt.port", 1883);
	mqtt_user = g_config.get<std::string>("mqtt.login", "");
	mqtt_password = g_config.get<std::string>("mqtt.password", "");

	log4cpp::Category &root=log4cpp::Category::getRoot();
	{
		// Find the [logging] settings
		int loglevel = g_config.get<int>("logging.level", 0);
		string logconfig = g_config.get("logging.config", "");

		string layout = "%d{%m-%d %H:%M:%S.%l} %m%n";
		auto l = new log4cpp::PatternLayout();
		l->setConversionPattern(layout);

		{
			log4cpp::Priority::Value pri = log4cpp::Priority::ERROR;
			switch(loglevel) {
			case 0: break;
			case 1: pri = log4cpp::Priority::WARN; break;
			case 2: pri = log4cpp::Priority::INFO; break;
			case 3: pri = log4cpp::Priority::DEBUG; break;
			}
			root.setPriority(pri);
		}

		vector<string> logging;
		split(logconfig.c_str(), logging, ':');
		for(vector<string>::const_iterator i_l = logging.begin();
			i_l != logging.end(); i_l++) {
			vector<string> kv;
			log4cpp::Appender *appender;
			split(*i_l, kv, '=', 2);
			if(kv[0] == "console") {
				appender = new log4cpp::OstreamAppender("console", &std::cout);
				appender->setLayout(l);
				appender->setThreshold(log4cpp::Priority::INFO);
				root.addAppender(appender);
			}
			else if(kv[0] == "file") {
				appender = new log4cpp::RollingFileAppender("file",
															kv[1].c_str(),
															16777216,
															30);
				appender->setLayout(l);
				appender->setThreshold(log4cpp::Priority::DEBUG);
				root.addAppender(appender);
			}
		}

		root.info("osdpmaster startup");
	}

	// ALSO - we pick up outgoing messages from an MQTT subscription, and
	// we send replies to MQTT topic subscribers!  Wow, IOT!

	int mosqe = mosquitto_lib_init();
	mosq_errcheck(mosqe, "mosquitto_lib_init");
	struct mosquitto *mosq =
		mosquitto_new("osdpmaster", false, (void *)&proto);

	// Give me a call when a message arrives
	mosquitto_message_callback_set(mosq, message_callback);

	if(mqtt_user != "")
		mosquitto_username_pw_set(mosq, mqtt_user.c_str(), mqtt_password.c_str());

	mosqe = mosquitto_connect(mosq, mqtt_host.c_str(), mqtt_port, 60);
	mosq_errcheck(mosqe, "mosquitto_connect");
	mosqe = mosquitto_subscribe(mosq, NULL, "osdp/bus1/outgoing/#", 0);
	mosq_errcheck(mosqe, "mosquitto_subscribe");

	{
		// Find the [port] settings
		memset(&config, 0, sizeof(config));
		config.port = "/dev/ttyO2";
		config.baud = 115200;
		config.parity = 'N';
		config.bits = 8;
		config.stop = 1;
		config.flow = 'n';
		config.usb = 0;
		config.lead = 1;
		config.trail = 0;
		config.timeout = 50000; // Microseconds?
		config.delay = 20000;	// Yeah, microseconds.
		config.idle = 500;
		char *port2 = NULL;
		xparse_config(config, &port2);
		proto = new busprotocol(&config);
		proto->port2(port2);
	}

	proto->m_mq = mosq;

	{
		// Find the <slave> elements
		for (auto i_sect : g_config) {
			if (strncmp(i_sect.first.c_str(), "slave", 5) == 0) {
				auto xaddr =
					g_config.get_optional<string>(i_sect.first + ".addr");
				const char* addr = NULL;
				if (xaddr)
					addr = (*xaddr).c_str();

				osdpslave &s = proto->add_slave(addr);
				s.declare_online(false); // slaves are born offline
			}
		}
	}

	pthread_t th;
	pthread_create(&th, NULL, busprotocol_run, (void *)proto);

	mosqe = mosquitto_loop_forever(mosq, -1, 1); // This is where main() lives
	mosq_errcheck(mosqe, "mosquitto_loop_forever");

	return 0;
}
