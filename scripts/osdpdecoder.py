#!/usr/bin/python3

import io, struct, time, codecs, binascii

# printdecoder: given "bytes" which is a TLV batch from Axiom
# protocol, produce printable message descriptions to an output sink

def osdpdecoder(msg: bytes, out: io.IOBase):
    # Decode the one message here
    cmd = msg[0]
    if cmd == 0x60:
        #print("OSDP_POLL", file=out)
        pass
    elif cmd == 0x61:
        print("OSDP_ID", file=out)
    elif cmd == 0x62:
        print("OSDP_CAP", file=out)
    elif cmd == 0x63:
        print("OSDP_DIAG", file=out)
    elif cmd == 0x64:
        print("OSDP_LSTAT", file=out)
    elif cmd == 0x65:
        print("OSDP_ISTAT", file=out)
    elif cmd == 0x66:
        print("OSDP_OSTAT", file=out)
    elif cmd == 0x67:
        print("OSDP_RSTAT", file=out)
    elif cmd == 0x68:
        print("OSDP_OUT", file=out)
    elif cmd == 0x69:
        print("OSDP_LED", file=out)
    elif cmd == 0x6A:
        print("OSDP_BUZ", file=out)
    elif cmd == 0x6B:
        print("OSDP_TEXT", file=out)
    elif cmd == 0x6D:
        print("OSDP_TDSET", file=out)
    elif cmd == 0x6E:
        print("OSDP_COMSET", file=out)
    elif cmd == 0x6F:
        print("OSDP_DATA", file=out)
    elif cmd == 0x71:
        print("OSDP_PROMPT", file=out)
    elif cmd == 0x73:
        print("OSDP_BIOREAD", file=out)
    elif cmd == 0x74:
        print("OSDP_BIOMATCH", file=out)
    elif cmd == 0x75:
        print("OSDP_KEYSET", file=out)
    elif cmd == 0x76:
        print("OSDP_CHLNG", file=out)
    elif cmd == 0x77:
        print("OSDP_SCRYPT", file=out)
    elif cmd == 0x7A:
        action, delay, status, msgmax = struct.unpack_from("<BHhH", msg, 1)
        print("OSDP_FTSTAT act={0} delay={1} stat={2} msgmax={3}".format(action, delay, status, msgmax), file=out)
    elif cmd == 0x80:
        print("OSDP_MFG", file=out)
    elif cmd == 0x40:
        #print("OSDP_ACK", file=out)
        pass
    elif cmd == 0x41:
        print("OSDP_NAK", file=out)
    elif cmd == 0x45:
        vendor = msg[1] << 16 | msg[2] << 8 | msg[3]
        model = msg[4]
        hw_version = msg[5]
        serial = msg[6] | msg[7] << 8 | msg[8] << 16 | msg[9]
        major = msg[10]
        minor = msg[11]
        build = msg[12]
        print("OSDP_PDID Vendor={0:06X} model={1} "
              "hwver={2} serial={3} "
              "fwver={4}.{5}.{6}".format(vendor, model, hw_version, serial, major, minor, build))
    elif cmd == 0x46:
        print("OSDP_PDCAP", file=out)
    elif cmd == 0x48:
        print("OSDP_LSTATR", file=out)
    elif cmd == 0x49:
        print("OSDP_ISTATR", file=out)
    elif cmd == 0x4A:
        print("OSDP_OSTATR", file=out)
    elif cmd == 0x4B:
        print("OSDP_RSTATR", file=out)
    elif cmd == 0x50:
        # Major math...
        rdr = msg[1]; fmt = msg[2]
        bits = msg[3] | (msg[4]<<8)
        by = (bits+7)//8
        print("OSDP_RAW rdr={0} fmt={1} bits={2}".format(rdr, fmt, bits),
              file=out, end='')
        biginternal = 0
        for x in range(by):
            print(" {0:2X}".format(msg[5+x]), file=out, end='')
            biginternal = (biginternal << 8) | msg[5+x]
        # print in "undefined card" format
        # Align biginternal:
        align = by*8 - bits
        if align > 0:
            biginternal >>= align
        # CF 90 6B 89 79 6A 36 A4
        # 2b 3e 5a e2 5e 5a 8d a9
        print(" {0:X} {1}*{2}".format(biginternal,
                                      (biginternal >> 32) & 0xFFFFFFFF,
                                      biginternal & 0xFFFFFFFF), file=out)
    elif cmd == 0x51:
        print("OSDP_FMT", file=out)
    elif cmd == 0x53:
        print("OSDP_KEYPAD", file=out)
    elif cmd == 0x54:
        print("OSDP_COM", file=out)
    elif cmd == 0x57:
        print("OSDP_BIOREADR", file=out)
    elif cmd == 0x58:
        print("OSDP_BIOMATCHR", file=out)
    elif cmd == 0x76:
        print("OSDP_CCRYPT", file=out)
    elif cmd == 0x78:
        print("OSDP_RMAC_I", file=out)
    elif cmd == 0x79:
        print("OSDP_BUSY", file=out)
    elif cmd == 0x90:
        print("OSDP_MFGREP", file=out)

    return
