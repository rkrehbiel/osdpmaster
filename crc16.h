#ifndef CRC16_H
#define CRC16_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
void crc16_prepare(uint16_t *crc);
uint16_t crc16_add(uint16_t crc, const uint8_t* data_p, int length);
uint16_t crc16_digest(uint16_t *crc);

#ifdef __cplusplus
}
inline void crc16_prepare(uint16_t &crc) { crc16_prepare(&crc); }
inline uint16_t crc16_digest(uint16_t &crc) { return crc16_digest(&crc); }
#endif
#endif // CRC16_H
