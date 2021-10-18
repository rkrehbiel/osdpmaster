#!/usr/bin/python3

# Upload a bin file to an OSDP 2.2-compliant device

import sys, os, datetime, paho, binascii, codecs, struct
import time, datetime, threading

import paho.mqtt.client as mqtt

# args: <file name> <addr>

name = sys.argv[1]
addr = int(sys.argv[2])

offline = False

def on_connect(client, userdata, flags, rc):
    global addr

    client.subscribe("osdp/bus1/incoming/{0}/#".format(addr))

ftevent = threading.Event()
ftstat = None

def on_message(client, userdata, message):
    global addr, ftevent, ftstat
    #print(message.topic, message.payload[0], codecs.decode(binascii.b2a_hex(message.payload), 'ascii'))
    tx = message.topic.split('/')
    # Is this OSDP_FTSTAT?
    if tx[-1].isnumeric() and \
       int(tx[-1]) == addr:
        if message.payload[0] == 0x7A:
            # This is FTSTAT
            #print("OSDP_FTSTAT")
            ftstat = message.payload # Stash it
            ftevent.set()            # Got it
    else:
        if tx[-1] == "status" and int(tx[-2]) == addr:
            global offline
            # payload is "OFFLINE" or "ONLINE"
            print(codecs.decode(message.payload, 'ascii'))
            if message.payload == b'OFFLINE':
                offline = True
            elif message.payload == b'ONLINE':
                offline = False

class uploader(threading.Thread):

    def __init__(self):
        super(uploader, self).__init__()

    def run(self):
        global name, addr, ftevent, ftstat, offline
        # Upload to OSDP PD

        with open(name, "rb") as i:
            image = i.read()     # suck it all in

        offset = 0
        frag = 100              # Nano firmware doesn't handle frag > 100
        fin = False
        while offset < len(image) or not fin:
            retry = True
            saytry = " "
            while retry:
                if offline:
                    time.sleep(0.5)
                    retry = True
                    continue

                retry = False
                if offset + frag > len(image):
                    frag = len(image)-offset

                print("{0}{1}".format(offset, saytry),"of",len(image),'frag',frag)
                rec = struct.pack("<BBLLH", 0x7C, # OSDP_FILETRANSFER
                                  1,              # file type (PD-specific)
                                  len(image),     # total file size
                                  offset,         # Current fragment offset
                                  frag)           # Current fragment size
                rec += image[offset:offset+frag]  # Append upload data
                ftevent.clear()                   # Clear the wait event
                client.publish("osdp/bus1/outgoing/{0}".format(addr),
                               payload=rec, qos=1)

                if not ftevent.wait(1.0):
                    if offset == len(image):
                        # PD abandoned it's responsibility to send back
                        # FTSTAT status=2.  Maybe it succeeded, maybe not,
                        # can't tell.
                        raise BaseException("abandoned after the end")
                    retry = True
                    saytry = "*"

            offset += frag
            # Interpret reply
            code, action, delay, status, msgmax = struct.unpack("<BBHhH", ftstat)
            # code is OSDP_FTSTAT

            # OSDP doc says status can be:
            #    0 - okay to proceed
            #    1 - file contents processed
            #    2 - rebooting now
            #    3 - PD is finishing file transfer
            # or:
            #    -1 - abort file transfer
            #    -2 - unrecognized file contents
            #    -3 - file data unacceptable/malformed
            # For Wavelynx, PD responds 1..1..1..1..1..1..[3..3..]2 
            # For Nano, PD responds 0..0..0..0..0..0..0..0..2
            # So:  normal, okay to proceed is either 0 or 1
            # And: idling at end is 3
            # And: finished is 2.
            if status in ( 0, 1, 3 ):
                time.sleep(delay / 1000.0)
            elif status == 2:
                fin = True      # We're done.

            #print("status =", status)

        return                  # end of thread

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.username_pw_set("scripts", "scripts")

client.connect("localhost", 1883, 60)

upload = uploader()
upload.daemon = True
upload.start()

client.loop_forever()
