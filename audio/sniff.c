#include "sniff.h"

#include <string.h>

struct bits { const uint8_t *p; uint32_t pos, end; };

static uint32_t get(struct bits *b, int n)
{
    uint32_t v = 0;
    while (n--) {
        uint32_t i = b->pos++;
        v = v << 1 | (i < b->end ? (uint32_t)(b->p[i >> 3] >> (7 - (i & 7))) & 1 : 0);
    }
    return v;
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

/* E-AC-3 independent frame: the addbsi flag that marks Atmos (flag_ec3_extension_type_a); -1 if unreadable */
static int eac3_ext_a(const uint8_t *f, uint32_t len)
{
    struct bits b = { f, 16, len * 8 };
    static const int blocks[4] = { 1, 2, 3, 6 };
    uint32_t strmtyp = get(&b, 2);
    get(&b, 3 + 11);
    uint32_t fscod = get(&b, 2), nblk = 6;
    if (fscod == 3) get(&b, 2);
    else nblk = blocks[get(&b, 2)];
    uint32_t acmod = get(&b, 3), lfe = get(&b, 1);
    get(&b, 5);
    for (int i = acmod ? 0 : 1; i >= 0; i--) {
        get(&b, 5);
        if (get(&b, 1)) get(&b, 8);
    }
    if (strmtyp == 1 && get(&b, 1)) get(&b, 16);
    if (get(&b, 1)) {
        if (acmod > 2) get(&b, 2);
        if (acmod > 2 && (acmod & 1)) get(&b, 6);
        if (acmod > 2 && (acmod & 4)) get(&b, 6);
        if (lfe && get(&b, 1)) get(&b, 5);
        if (strmtyp == 0) {
            for (int i = acmod ? 0 : 1; i >= 0; i--)
                if (get(&b, 1)) get(&b, 6);
            if (get(&b, 1)) get(&b, 6);
            switch (get(&b, 2)) {
            case 1: get(&b, 5); break;
            case 2: get(&b, 12); break;
            case 3: b.pos += (get(&b, 5) + 2) * 8; break;
            }
            if (acmod < 2)
                for (int i = acmod ? 0 : 1; i >= 0; i--)
                    if (get(&b, 1)) get(&b, 14);
            if (get(&b, 1))
                for (uint32_t k = 0; k < nblk; k++)
                    if (nblk == 1 || get(&b, 1)) get(&b, 5);
        }
    }
    if (get(&b, 1)) {
        get(&b, 3 + 2);
        if (acmod == 2) get(&b, 4);
        if (acmod >= 6) get(&b, 2);
        for (int i = acmod ? 0 : 1; i >= 0; i--)
            if (get(&b, 1)) get(&b, 8);
        if (fscod != 3) get(&b, 1);
    }
    if (strmtyp == 0 && nblk != 6) get(&b, 1);
    if (strmtyp == 2 && (nblk == 6 || get(&b, 1))) get(&b, 6);
    if (b.pos >= b.end) return -1;
    if (!get(&b, 1)) return 0;
    get(&b, 6 + 7);
    return b.pos < b.end ? (int)get(&b, 1) : -1;
}

static void vote(struct sniff *s, int v)
{
    if (v < 0) return;
    s->votes = s->votes && v == s->last ? s->votes + 1 : 1;
    s->last = v;
    if (s->votes < 2) return;
    if (s->codec == SN_DTS || s->codec == SN_DTSHD) { s->dtsx = v; s->atmos = 0; }
    else { s->atmos = v; s->dtsx = 0; }
    s->done = 1;
}

static const uint8_t *find4(const uint8_t *p, const uint8_t *end, uint32_t w)
{
    for (; p + 4 <= end; p++)
        if (p[0] == w >> 24 && be32(p) == w) return p;
    return NULL;
}

/* returns bytes consumed from k, 0 when more data is needed */
static uint32_t step(struct sniff *s, uint32_t k)
{
    const uint8_t *b = s->buf + k;
    uint32_t left = s->n - k;
    if (left < 8) return 0;
    if (s->iec && be32(b) == 0xF8724E1F) {
        int t = b[5] & 0x7f;
        int c = t == 0x01 ? SN_AC3 : t == 0x15 ? SN_EAC3 : t == 0x16 ? SN_TRUEHD : t == 0x11 ? SN_DTSHD
              : t >= 0x0b && t <= 0x0d ? SN_DTS : 0;
        if (c && s->codec != c) { s->codec = c; s->votes = 0; }
        return 8;
    }
    switch (s->codec) {
    case SN_TRUEHD:
        if (be32(b) != 0xF8726FBA) return 1;
        if (left < 18) return 0;
        vote(s, (b[16] >> 4) == 4 && (b[17] & 0x80));
        return 18;
    case SN_AC3:
    case SN_EAC3: {
        if (b[0] != 0x0B || b[1] != 0x77) return 1;
        uint32_t bsid = b[5] >> 3;
        if (bsid <= 10) {
            if (s->codec == SN_AC3) vote(s, 0);
            return 2;
        }
        if (bsid > 16) return 1;
        uint32_t fs = (((uint32_t)(b[2] & 7) << 8 | b[3]) + 1) * 2;
        if (fs + 66 > left) return fs + 66 > SNIFF_BUF / 2 ? 1 : 0;
        /* IEC 61937 pads after each frame; tunneled (HW A/V sync) writes put a header between frames */
        int next = 0;
        for (uint32_t g = 0; g <= 64 && !next; g += 2) next = b[fs + g] == 0x0B && b[fs + g + 1] == 0x77;
        if (!next && !(s->iec && !b[fs] && !b[fs + 1])) return 1;
        if ((b[2] >> 3) == 0) vote(s, eac3_ext_a(b, fs));
        return fs;
    }
    case SN_DTS:
    case SN_DTSHD: {
        if (be32(b) != 0x7FFE8001) return 1;
        const uint8_t *end = s->buf + s->n, *j = find4(b + 4, end, 0x7FFE8001);
        if (!j) return left > SNIFF_BUF / 2 ? 4 : 0;
        const uint8_t *x = find4(b + 4, j, 0x41A29547);
        int v = 0;
        if (x)
            for (const uint8_t *p = x + 4; p + 4 <= j && !v; p += 4) {
                uint32_t w = be32(p);
                v = w == 0x02000850u || (w >> 1) == (0xF14000D0u >> 1);
            }
        vote(s, v);
        return (uint32_t)(j - b);
    }
    default:
        return 1;
    }
}

void sniff_init(struct sniff *s, int iec, int codec)
{
    memset(s, 0, sizeof(*s));
    s->iec = iec;
    s->codec = codec;
    s->atmos = s->dtsx = -1;
}

int sniff_feed(struct sniff *s, const void *data, uint32_t len)
{
    const uint8_t *d = data;
    s->fed += len;
    while (!s->done && len) {
        while (len && s->n < SNIFF_BUF - 1) {
            if (!s->iec) s->buf[s->n++] = *d;
            else if (!s->odd) s->carry = *d;
            else { s->buf[s->n++] = *d; s->buf[s->n++] = (uint8_t)s->carry; }
            s->odd ^= s->iec;
            d++;
            len--;
        }
        uint32_t k = 0, c;
        while (!s->done && (c = step(s, k))) k += c;
        if (k > s->n) k = s->n;
        memmove(s->buf, s->buf + k, s->n - k);
        s->n -= k;
        if (s->n >= SNIFF_BUF - 1) s->n = 0;
    }
    if (s->fed >= SNIFF_LIMIT) s->done = 1;
    return s->done;
}

const char *sniff_name(int codec)
{
    static const char *n[] = { "unknown", "ac3", "eac3", "truehd", "dts", "dtshd" };
    return codec >= 0 && codec <= SN_DTSHD ? n[codec] : "unknown";
}
