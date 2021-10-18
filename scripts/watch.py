#!/usr/bin/python3

import sys, os, datetime, paho, binascii, codecs, io

import paho.mqtt.client as mqtt

from printdecoder import *
from osdpdecoder import *

def on_connect(client, userdata, flags, rc):
    print('MQTT connected, flags', flags, 'rc', mqtt.connack_string(rc))

    client.subscribe("osdp/bus1/incoming/#")

# 907c55a7070c01010972616e67653d333500
# OSDP_MFGREP
#   0x7C 0x55 0xA7
#         07 = text log
#           0c = msg id
#             01 = segment count
#               01 = segment number
#                 09 = this segment length
#                   72... = text

def on_message(client, userdata, message):
    now = datetime.datetime.now()
    xtopic = message.topic.split("/")
    if xtopic[-1] == "status":
        print(now.strftime("%Y-%m-%d %H:%M:%S"), message.topic, codecs.decode(message.payload, 'ascii'))
    else:
        print(now.strftime("%Y-%m-%d %H:%M:%S"), message.topic, codecs.decode(binascii.b2a_hex(message.payload), 'ascii'))
        if message.payload[0:5] == bytes([ 0x90, 0x7C, 0x55, 0xA7, 0x02 ]):
            print(now.strftime("%Y-%m-%d %H:%M:%S"), end=' ')
            printdecoder(message.payload[5:], sys.stdout)
        elif message.payload[0:5] == bytes([ 0x90, 0x7C, 0x55, 0xA7, 0x07 ]):
            if message.payload[-1] == 0:
                text = message.payload[9:-1]
            else:
                text = message.payload[9:]
            text = codecs.decode(text, 'ascii')
            print(text)
        else:
            print(now.strftime("%Y-%m-%d %H:%M:%S"), end=' ')
            osdpdecoder(message.payload, sys.stdout)

client = mqtt.Client(clean_session=True)
client.on_connect = on_connect
client.on_message = on_message

client.username_pw_set("scripts", "scripts")

client.connect("localhost", 1883, 60)

client.loop_forever()
