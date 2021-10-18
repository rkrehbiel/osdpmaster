#ifndef ODSP_DEF_H
#define OSDP_DEF_H

#if __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define chSOH 0x53				// OSDP message introducer

#define OSDP_POLL 0x60
#define OSDP_ID 0x61
#define OSDP_CAP 0x62
#define OSDP_DIAG 0x63
#define OSDP_LSTAT 0x64
#define OSDP_ISTAT 0x65
#define OSDP_OSTAT 0x66
#define OSDP_RSTAT 0x67
#define OSDP_OUT 0x68
#define OSDP_LED 0x69
#define OSDP_BUZ 0x6A
#define OSDP_TEXT 0x6B
#define OSDP_TDSET 0x6D
#define OSDP_COMSET 0x6E
#define OSDP_DATA 0x6F
#define OSDP_PROMPT 0x71
#define OSDP_BIOREAD 0x73
#define OSDP_BIOMATCH 0x74
#define OSDP_KEYSET 0x75
#define OSDP_CHLNG 0x76
#define OSDP_SCRYPT 0x77
#define OSDP_ABORT 0x7A
#define OSDP_MAXREPLY 0x7B
#define OSDP_MFG 0x80

#define OSDP_ACK 0x40
#define OSDP_NAK 0x41
#define OSDP_PDID 0x45
#define OSDP_PDCAP 0x46
#define OSDP_LSTATR 0x48
#define OSDP_ISTATR 0x49
#define OSDP_OSTATR 0x4A
#define OSDP_RSTATR 0x4B
#define OSDP_RAW 0x50
#define OSDP_FMT 0x51
#define OSDP_KEYPAD 0x53
#define OSDP_COM 0x54
#define OSDP_BIOREADR 0x57
#define OSDP_BIOMATCHR 0x58
#define OSDP_CCRYPT 0x76
#define OSDP_RMAC_I 0x78
#define OSDP_MFGREP 0x90
#define OSDP_BUSY 0x79

#pragma pack(1)
#ifndef T_OSDP_COMMON
#define T_OSDP_COMMON 1
struct osdp_common {
	uint8_t soh;
	uint8_t addr;
	uint8_t len[2];
	uint8_t ctrl;
};
#endif
#ifndef T_OSDP_COMMON_FLEX
#define T_OSDP_COMMON_FLEX 1
struct osdp_common_flex {
	struct osdp_common m;
	uint8_t data[];
};
#endif

#pragma pack()

#if __cplusplus
}
#endif

#endif // OSDP_DEF_H
