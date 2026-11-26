#include "iec_dtshd.h"
#include <string.h>

#define SYNC_CORE 0x7FFE8001u
#define SYNC_EXT  0x64582025u

static uint32_t be32(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

void dtshd_reset(struct dtshd_packer *p)
{
    p->used = 0;
}

int dtshd_push(struct dtshd_packer *p, const uint8_t *in, uint32_t n)
{
    if (n > DTSHD_RAW_CAP - p->used) {
        p->used = 0;
        p->resyncs++;
        if (n > DTSHD_RAW_CAP) return -1;
    }
    memcpy(p->raw + p->used, in, n);
    p->used += n;
    return 0;
}

static uint32_t core_size(const uint8_t *h)
{
    return ((((uint32_t)h[5] & 3) << 12) | ((uint32_t)h[6] << 4) | (h[7] >> 4)) + 1;
}

static uint32_t ext_size(const uint8_t *h)
{
    if (h[5] & 0x20) /* bHeaderSizeType */
        return ((((uint32_t)h[6] & 1) << 19) | ((uint32_t)h[7] << 11) | ((uint32_t)h[8] << 3) | (h[9] >> 5)) + 1;
    return ((((uint32_t)h[6] & 31) << 11) | ((uint32_t)h[7] << 3) | (h[8] >> 5)) + 1;
}

static void drop(struct dtshd_packer *p, uint32_t n)
{
    if (n >= p->used) { p->used = 0; return; }
    memmove(p->raw, p->raw + n, p->used - n);
    p->used -= n;
}

static void resync(struct dtshd_packer *p)
{
    uint32_t i;
    for (i = 1; i + 4 <= p->used; i++)
        if (be32(p->raw + i) == SYNC_CORE) break;
    drop(p, i + 4 <= p->used ? i : (p->used > 3 ? p->used - 3 : 0));
    p->resyncs++;
}

static const uint32_t core_rates[16] = {
    0, 8000, 16000, 32000, 0, 0, 11025, 22050, 44100, 0, 0, 12000, 24000, 48000, 96000, 192000 };

int dtshd_next_burst(struct dtshd_packer *p, uint8_t *out, uint32_t cap)
{
    for (int guard = 0; guard < 64; guard++) {
        const uint8_t *h = p->raw;
        uint32_t avail = p->used, end, rate, samples, period, subtype, burst, pd, i;
        int complete = 0;

        if (avail < 16) return 0;
        if (be32(h) != SYNC_CORE) { resync(p); continue; }
        end = core_size(h);
        if (end < 96 || end > 16384) { resync(p); continue; }
        if (end > avail) return 0;
        for (;;) {
            uint32_t sync, ext;
            if (avail - end < 4) break;
            sync = be32(h + end);
            if (sync == SYNC_CORE) { complete = 1; break; }
            if (sync != SYNC_EXT) { end = 0; break; }
            if (avail - end < 10) break;
            ext = ext_size(h + end);
            if (ext < 10 || ext > DTSHD_RAW_CAP) { end = 0; break; }
            if (ext > avail - end) break;
            end += ext;
        }
        if (!end) { resync(p); continue; }
        if (!complete) return 0;

        rate = core_rates[(h[8] >> 2) & 15];
        samples = ((((uint32_t)h[4] & 1) << 6) | (h[5] >> 2)) + 1;
        samples *= 32;
        if (!rate || (768000u * samples) % rate) { drop(p, end); return -3; }
        period = 768000u * samples / rate;
        for (subtype = 0; subtype < 6 && (512u << subtype) != period; subtype++) {}
        if (subtype == 6) { drop(p, end); return -3; }
        burst = period * 4;
        pd = ((12 + end + 8 + 15) & ~15u) - 8;
        if (end > 65535 || pd > burst - 8 || burst > cap) { drop(p, end); return -4; }

        memset(out, 0, burst);
        /* Pa Pb Pc Pd */
        out[0] = 0x72; out[1] = 0xF8;
        out[2] = 0x1F; out[3] = 0x4E;
        out[4] = 0x11; out[5] = (uint8_t)subtype;
        out[6] = pd & 0xFF; out[7] = (pd >> 8) & 0xFF;
        /* DTS-HD header + unit, byte-swapped into 16-bit words */
        {
            uint8_t hdr[12] = { 0x01, 0, 0, 0, 0, 0, 0, 0, 0xFE, 0xFE,
                                (uint8_t)(end >> 8), (uint8_t)end };
            for (i = 0; i < 12; i++) out[8 + (i ^ 1u)] = hdr[i];
            for (i = 0; i < end; i++) out[20 + (i ^ 1u)] = h[i];
        }
        drop(p, end);
        p->frames++;
        p->last_period = period;
        return (int)burst;
    }
    return 0;
}
