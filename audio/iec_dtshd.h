/* DTS-HD access units -> IEC 61937-5 type IV (0x11) bursts on a 768 kHz (8ch 192k) link. */
#pragma once
#include <stdint.h>

#define DTSHD_RAW_CAP (96u * 1024u)
#define DTSHD_BURST_MAX (16384u * 4u)

struct dtshd_packer {
    uint32_t used;
    uint32_t frames;
    uint32_t resyncs;
    uint32_t last_period;
    uint8_t raw[DTSHD_RAW_CAP];
};

void dtshd_reset(struct dtshd_packer *p);

int dtshd_push(struct dtshd_packer *p, const uint8_t *in, uint32_t n);

/* >0 burst bytes, 0 = need more input, <0 = unit dropped */
int dtshd_next_burst(struct dtshd_packer *p, uint8_t *out, uint32_t cap);
