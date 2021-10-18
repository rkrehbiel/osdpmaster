CC = gcc
C++ = g++

C++STD = -std=c++11

OPTIMIZE := -O2

MOSQUITTO_INCLUDE = $(shell pkg-config --cflags libmosquitto)

MOSQUITTO_LIB = $(shell pkg-config --libs libmosquitto)

INCLUDES = $(shell pkg-config --cflags log4cpp) \
	$(shell pkg-config --cflags uuid) \
	$(shell pkg-config --cflags openssl) \
	$(MOSQUITTO_INCLUDE)

DEFINES =

CFLAGS = -pthread -g $(OPTIMIZE) $(DEFINES) $(INCLUDES)
CCFLAGS = -pthread -g $(OPTIMIZE) -Wno-psabi $(C++STD) $(DEFINES) $(INCLUDES)

LIBS = 	-lz \
	-lpthread \
	-Wl,-Bstatic \
	$(shell pkg-config --libs log4cpp --static) \
	$(shell pkg-config --libs uuid) \
	$(MOSQUITTO_LIB_STATIC) \
	-Wl,-Bdynamic \
	$(MOSQUITTO_LIB) \
	$(shell pkg-config --libs openssl) \
	-lboost_thread -lboost_system -lrt -ldl

CSRCS = crc16.c
CCSRCS = osdpmaster.cpp osdpslave.cpp osdpprotocol.cpp blob.cpp
OBJS = $(CSRCS:.c=.o) $(CCSRCS:.cpp=.o)

osdpmaster: $(OBJS) makefile
	$(C++) $(CCFLAGS) -o osdpmaster $(OBJS) $(LIBS)

clean:
	/bin/rm -vf osdpmaster $(OBJS)

depend:
	$(CC) $(CFLAGS) -MM $(CSRCS) >depends
	$(C++) $(CCFLAGS) -MM $(CCSRCS) >>depends

.cpp.o:
	$(C++) $(CCFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<

include depends
