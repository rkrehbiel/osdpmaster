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

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.username_pw_set("scripts", "scripts")

mqtthost = "localhost"
if "MQTT_SERVER" in os.environ:
    mqtthost = os.getenv("MQTT_SERVER")

client.connect(mqtthost, 1883, 60)

time.sleep(1)

query = bytes([ 0x80, 0x7C, 0x55, 0xA7, 0x0A ]) # OSDP_MFG "inquire public key"

msg = client.publish("osdp/bus1/outgoing/{0}".format(addr),
                     payload=query, qos=1)

client.loop_forever()
