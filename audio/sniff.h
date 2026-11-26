#pragma once
#include <stdint.h>

/* Reads the object-audio flags (TrueHD Atmos, DD+ Atmos, DTS:X) from the first bytes of a bitstream. */
enum { SN_UNKNOWN, SN_AC3, SN_EAC3, SN_TRUEHD, SN_DTS, SN_DTSHD };

#define SNIFF_BUF 65536
#define SNIFF_LIMIT (4u << 20)

struct sniff {
    uint8_t buf[SNIFF_BUF];
    uint32_t n;
    uint64_t fed;
    int iec, odd, carry;
    int codec;
    int atmos, dtsx;
    int votes, last;
    int done;
};

void sniff_init(struct sniff *s, int iec, int codec);
/* 1 once codec/atmos/dtsx are settled (or the limit passed) */
int sniff_feed(struct sniff *s, const void *data, uint32_t len);
const char *sniff_name(int codec);
