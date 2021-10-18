#!/usr/bin/python3

# send factory-reset command to Nano/Wavelynx

import sys, os, datetime, paho, binascii, codecs, struct
import time, datetime, threading

import paho.mqtt.client as mqtt

# args: addr

addr = sys.argv[1]

def on_connect(client, userdata, flags, rc):
    print("Connected.")

client = mqtt.Client()
client.on_connect = on_connect

client.username_pw_set("scripts", "scripts")

client.connect("localhost", 1883, 60)

rec = bytes([0x80, 0x7C, 0x55, 0xA7, 0x0A])

client.publish("osdp/bus1/outgoing/{0}".format(addr),
               payload=rec, qos=1)
