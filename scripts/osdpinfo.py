#!/usr/bin/python3

import sys, os, datetime, paho, binascii, codecs, struct
import time, datetime

import paho.mqtt.client as mqtt

addr = sys.argv[1]

def on_connect(client, userdata, flags, rc):
    global addr
    print('MQTT connected, flags', flags, 'rc', mqtt.connack_string(rc))

    client.subscribe("osdp/bus1/incoming/{0}".format(addr))

def on_message(client, userdata, message):
    print(message.topic, codecs.decode(binascii.b2a_hex(message.payload), 'ascii'))
    # Decode the info
    # 45 
    if message.payload[0] == 0x45: # OSDP_PDID
        vendor = message.payload[1] << 16 | \
            message.payload[2] << 8 | \
            message.payload[3]
        model = message.payload[4]
        hw_version = message.payload[5]
        serial = message.payload[6] | \
            message.payload[7] << 8 | \
            message.payload[8] << 16 | \
            message.payload[9]
        major = message.payload[10]
        minor = message.payload[11]
        build = message.payload[12]

        print("Vendor={0:06X} model={1} "
              "hwver={2} serial={3} "
              "fwver={4}.{5}.{6}".format(vendor, model, hw_version, serial, major, minor, build))

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.username_pw_set("scripts", "scripts")

client.connect("localhost", 1883, 60)

time.sleep(1)

query = bytes([ 0x61 ])         # OSDP_ID

msg = client.publish("osdp/bus1/outgoing/{0}".format(addr),
                     payload=query, qos=1)

client.loop_forever()
