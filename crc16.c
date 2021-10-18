#include "crc16.h"

// Modified from something on stackoverflow.com... purports to be CCITT CRC-16

// First prepare...
void crc16_prepare(uint16_t *crc) {
	*crc = 0x1D0F;
}

// Then, accumulate...
uint16_t crc16_add(uint16_t crc, const uint8_t* data_p, int length) {
    uint8_t x;

    while (--length >= 0) {
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) ^ ((uint16_t)(x <<5)) ^ ((uint16_t)x);
    }
    return crc;
}

// Then, complete the digest.
uint16_t crc16_digest(uint16_t *crc) {
	return *crc;
}

