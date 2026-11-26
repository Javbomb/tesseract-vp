/* Wraps the stock Amlogic primary audio HAL; see README.md. */
#define LOG_TAG "TesseractAudio"
#include "hal_types.h"
#include "iec_dtshd.h"
#include "sniff.h"

#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/system_properties.h>
#include <unistd.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define TVP_VERSION "1.0"
#define STOCK_HAL "/vendor/lib/hw/audio.primary.amlogic.so"
#define AUD_CAP "/sys/class/amhdmitx/amhdmitx0/aud_cap"

static int g_bypass = 1, g_dsp = 1, g_dtshd = 1;

static void *g_stock_dso;
static struct hw_module_t *g_stock_hmi;
static uintptr_t g_stock_base;
static size_t g_stock_size;

static int in_stock(const void *fn)
{
    uintptr_t a = (uintptr_t)fn;
    return g_stock_base && a >= g_stock_base && a < g_stock_base + g_stock_size;
}


struct sink_caps { int bitstream; int dtshd_hbr; int mat; };

static struct sink_caps read_caps(void)
{
    struct sink_caps c = { 0, 0, 0 };
    char buf[2048];
    int fd = open(AUD_CAP, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return c;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return c;
    buf[n] = 0;
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (strchr(line, ',') && strncmp(line, "PCM,", 4) && strncmp(line, "CodingType", 10))
            c.bitstream = 1;
        if (!strncmp(line, "DTS-HD, 8 ch,", 13) && strstr(line, "/192 kHz")) c.dtshd_hbr = 1;
        if (!strncmp(line, "MAT,", 4)) c.mat = 1;
    }
    return c;
}


static int (*stock_set_parameters)(struct audio_hw_device *, const char *);
static atomic_int g_rewrites;
static atomic_int g_hdmi_format = -1;

static void note_hdmi_format(const char *kv)
{
    const char *p = kv ? strstr(kv, "hdmi_format=") : NULL;
    if (p && (p == kv || p[-1] == ';')) atomic_store(&g_hdmi_format, atoi(p + 12));
}

static int tvp_set_parameters(struct audio_hw_device *dev, const char *kv)
{
    const char *hit = NULL;
    if (g_bypass && kv) {
        for (const char *p = strstr(kv, "hdmi_format=5"); p; p = strstr(p + 1, "hdmi_format=5")) {
            char next = p[13];
            if ((p == kv || p[-1] == ';') && (next == 0 || next == ';')) { hit = p; break; }
        }
    }
    if (hit && read_caps().bitstream) {
        size_t len = strlen(kv);
        char *copy = malloc(len + 1);
        if (copy) {
            memcpy(copy, kv, len + 1);
            copy[(hit - kv) + 12] = '6';
            int n = atomic_fetch_add(&g_rewrites, 1);
            if (n < 3 || (n & 255) == 0) LOGI("bypass: '%s' -> '%s' (#%d)", kv, copy, n + 1);
            int rc = stock_set_parameters(dev, copy);
            note_hdmi_format(copy);
            free(copy);
            return rc;
        }
    }
    note_hdmi_format(kv);
    return stock_set_parameters(dev, kv);
}


struct pcm_config_head { unsigned int channels, rate; };
static void *(*real_pcm_open)(unsigned int, unsigned int, unsigned int, struct pcm_config_head *);
static int (*real_pcm_close)(void *);
static int (*real_pcm_write)(void *, const void *, unsigned int);
static int (*real_pcm_is_ready)(void *);
static int (*real_asp_command)(int, int, int, void *, void *, void *);

#define PCM_IN 0x10000000u
#define ASP_SET_REFERENCE 141
#define MAX_HDMI_PCM 4

static pthread_mutex_t g_dsp_lock = PTHREAD_MUTEX_INITIALIZER;
static struct { void *pcm; uint32_t rate, ch; int playing; } g_hdmi[MAX_HDMI_PCM];
static void *g_owner;
static int g_changing;
static uint32_t g_park[3] = { 0, 48000, 8 }; /* port 0 = TDM-A dummy bus */
static uint32_t g_applied[3];
static atomic_int g_dsp_events;

static int valid_ref(const uint32_t *v)
{
    static const uint32_t rates[] = { 32000, 44100, 48000, 88200, 96000, 176400, 192000 };
    int ok = 0;
    for (unsigned i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) ok |= v[1] == rates[i];
    return ok && (v[2] == 1 || v[2] == 2 || v[2] == 4 || v[2] == 6 || v[2] == 8);
}

static int hdmi_slot(void *pcm)
{
    for (int i = 0; i < MAX_HDMI_PCM; i++) if (pcm && g_hdmi[i].pcm == pcm) return i;
    return -1;
}

static void set_ref_locked(uint32_t port, uint32_t rate, uint32_t ch, const char *why)
{
    uint32_t v[3] = { port, rate, ch };
    if (!memcmp(v, g_applied, sizeof(v))) return;
    int rc = real_asp_command(0, ASP_SET_REFERENCE, 12, v, NULL, NULL);
    if (rc == 0) memcpy(g_applied, v, sizeof(v)); else g_applied[0] = 9;
    int n = atomic_fetch_add(&g_dsp_events, 1);
    if (n < 40 || (n & 63) == 0)
        LOGI("dsp reference %s: port %u %u Hz %u ch -> %d", why, port, rate, ch, rc);
}

static void park_locked(const char *why)
{
    g_owner = NULL;
    set_ref_locked(g_park[0], g_park[1], g_park[2], why);
}

static int is_hdmi_out(unsigned card, unsigned dev, unsigned flags, const struct pcm_config_head *c)
{
    if (card != 0 || dev != 1 || (flags & PCM_IN) || !c) return 0;
    uint32_t v[3] = { 1, c->rate, c->channels };
    return valid_ref(v);
}

/* vendor.media.audio.tvp.cap = "<skip bytes> <bytes> <rate>" arms one capture of the next HDMI pcm at that rate */
#define CAP_PROP "vendor.media.audio.tvp.cap"
#define CAP_PATH "/data/vendor/audiohal/tvp_cap.bin"
static void *g_cap_pcm;
static int g_cap_fd = -1;
static uint32_t g_cap_skip, g_cap_left;

static void cap_arm(void *pcm, uint32_t rate)
{
    char v[PROP_VALUE_MAX] = "";
    unsigned skip = 0, len = 0, want = 0;
    if (g_cap_fd >= 0 || __system_property_get(CAP_PROP, v) <= 0) return;
    if (sscanf(v, "%u %u %u", &skip, &len, &want) != 3 || !len || want != rate) return;
    g_cap_fd = open(CAP_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    LOGI("capture: %u bytes after %u at %u Hz -> fd %d (%s)", len, skip, rate, g_cap_fd, g_cap_fd < 0 ? strerror(errno) : "ok");
    __system_property_set(CAP_PROP, "0");
    if (g_cap_fd < 0) return;
    g_cap_pcm = pcm; g_cap_skip = skip; g_cap_left = len;
}

static void cap_write(const uint8_t *d, uint32_t n)
{
    if (g_cap_skip) { uint32_t k = n < g_cap_skip ? n : g_cap_skip; g_cap_skip -= k; d += k; n -= k; }
    if (n > g_cap_left) n = g_cap_left;
    if (n && write(g_cap_fd, d, n) != (ssize_t)n) g_cap_left = 0; else g_cap_left -= n;
    if (!g_cap_left) { close(g_cap_fd); g_cap_fd = -1; g_cap_pcm = NULL; LOGI("capture: done"); }
}

#define WIRE_PROP "vendor.media.audio.tvp.wire"
static pthread_mutex_t g_wire_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int g_wire_armed;
static struct sniff g_wire;
static void *_Atomic g_wire_pcm;
static void *g_wire_bits_pcm;
static atomic_int g_out_bits;

static void wire_arm(void) { atomic_store(&g_wire_armed, 1); }

static int all_zero(const uint8_t *d, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) if (d[i]) return 0;
    return 1;
}

static void wire_feed(void *pcm, const void *data, uint32_t n)
{
    if (atomic_load(&g_wire_armed) == 1 && all_zero(data, n)) return;
    pthread_mutex_lock(&g_wire_lock);
    int st = atomic_load(&g_wire_armed);
    if (st == 1) { sniff_init(&g_wire, 1, SN_UNKNOWN); atomic_store(&g_wire_armed, 2); }
    if (st && (sniff_feed(&g_wire, data, n) || (g_wire.codec == SN_UNKNOWN && g_wire.fed > (1u << 20)))) {
        const char *c = g_wire.codec == SN_UNKNOWN ? "pcm" : g_wire.codec == SN_TRUEHD && g_wire.atmos < 0 ? "matpcm"
                        : sniff_name(g_wire.codec);
        char v[PROP_VALUE_MAX];
        snprintf(v, sizeof(v), "codec=%s atmos=%d dtsx=%d t=%ld", c, g_wire.atmos, g_wire.dtsx, (long)time(NULL));
        /* a PCM port opened beside a bitstream one (optical and HDMI) must not hide it */
        if (g_wire.codec != SN_UNKNOWN || ((!g_wire_bits_pcm || g_wire_bits_pcm == pcm) && !atomic_load(&g_out_bits))) {
            g_wire_bits_pcm = g_wire.codec != SN_UNKNOWN ? pcm : NULL;
            __system_property_set(WIRE_PROP, v);
            LOGI("hdmi out: %s", v);
        }
        if (atomic_load(&g_wire_armed) == 2) atomic_store(&g_wire_armed, 0);
    }
    pthread_mutex_unlock(&g_wire_lock);
}

/* card0 dev4/5: the SPDIF ports that carry DD+, DD and DTS to HDMI */
static int is_spdif_out(unsigned card, unsigned dev, unsigned flags)
{
    return card == 0 && (dev == 4 || dev == 5) && !(flags & PCM_IN);
}

static void *hook_pcm_open(unsigned card, unsigned dev, unsigned flags, struct pcm_config_head *c)
{
    if (is_spdif_out(card, dev, flags)) {
        void *p = real_pcm_open(card, dev, flags, c);
        if (p) { atomic_store(&g_wire_pcm, p); wire_arm(); }
        return p;
    }
    if (!is_hdmi_out(card, dev, flags, c)) return real_pcm_open(card, dev, flags, c);
    uint32_t rate = c->rate, ch = c->channels;
    pthread_mutex_lock(&g_dsp_lock);
    g_changing++;
    park_locked("before-open");
    pthread_mutex_unlock(&g_dsp_lock);

    void *pcm = real_pcm_open(card, dev, flags, c);

    pthread_mutex_lock(&g_dsp_lock);
    g_changing--;
    if (pcm && (!real_pcm_is_ready || real_pcm_is_ready(pcm))) {
        for (int i = 0; i < MAX_HDMI_PCM; i++) if (!g_hdmi[i].pcm) {
            g_hdmi[i].pcm = pcm; g_hdmi[i].rate = rate; g_hdmi[i].ch = ch; g_hdmi[i].playing = 0;
            break;
        }
        cap_arm(pcm, rate);
        atomic_store(&g_wire_pcm, pcm);
        wire_arm();
    }
    pthread_mutex_unlock(&g_dsp_lock);
    return pcm;
}

static int hook_pcm_write(void *pcm, const void *data, unsigned int count)
{
    if (pcm != g_owner || !count) {
        pthread_mutex_lock(&g_dsp_lock);
        int i = hdmi_slot(pcm);
        if (i >= 0 && count && !g_changing) {
            g_hdmi[i].playing = 1;
            g_owner = pcm;
            set_ref_locked(1, g_hdmi[i].rate, g_hdmi[i].ch, "first-write");
        }
        pthread_mutex_unlock(&g_dsp_lock);
    }
    if (pcm == g_cap_pcm && g_cap_fd >= 0) cap_write(data, count);
    if (atomic_load(&g_wire_armed) && pcm == atomic_load(&g_wire_pcm)) wire_feed(pcm, data, count);
    int rc = real_pcm_write(pcm, data, count);
    if (rc != 0 && pcm == g_owner) {
        pthread_mutex_lock(&g_dsp_lock);
        int i = hdmi_slot(pcm);
        if (i >= 0) g_hdmi[i].playing = 0;
        if (g_owner == pcm) park_locked("write-error");
        pthread_mutex_unlock(&g_dsp_lock);
    }
    return rc;
}

static int hook_pcm_close(void *pcm)
{
    pthread_mutex_lock(&g_wire_lock);
    if (g_wire_bits_pcm == pcm) g_wire_bits_pcm = NULL;
    pthread_mutex_unlock(&g_wire_lock);
    if (pcm == atomic_load(&g_wire_pcm)) {
        void *next = NULL;
        pthread_mutex_lock(&g_dsp_lock);
        for (int k = 0; k < MAX_HDMI_PCM && !next; k++) if (g_hdmi[k].pcm != pcm) next = g_hdmi[k].pcm;
        pthread_mutex_unlock(&g_dsp_lock);
        atomic_store(&g_wire_pcm, next);
        if (next) wire_arm();
    }
    pthread_mutex_lock(&g_dsp_lock);
    int i = hdmi_slot(pcm);
    if (i >= 0) {
        g_changing++;
        if (g_owner == pcm || !g_owner) park_locked("before-close");
        if (pcm == g_cap_pcm && g_cap_fd >= 0) { close(g_cap_fd); g_cap_fd = -1; g_cap_pcm = NULL; }
    }
    pthread_mutex_unlock(&g_dsp_lock);

    int rc = real_pcm_close(pcm);

    if (i >= 0) {
        pthread_mutex_lock(&g_dsp_lock);
        if (g_hdmi[i].pcm == pcm) g_hdmi[i].pcm = NULL;
        g_changing--;
        pthread_mutex_unlock(&g_dsp_lock);
    }
    return rc;
}

static int hook_asp_command(int a0, int cmd, int len, void *data, void *b, void *c)
{
    if (a0 != 0 || cmd != ASP_SET_REFERENCE || len != 12 || !data)
        return real_asp_command(a0, cmd, len, data, b, c);
    uint32_t v[3];
    memcpy(v, data, sizeof(v));
    pthread_mutex_lock(&g_dsp_lock);
    if (v[0] == 0 && valid_ref(v)) memcpy(g_park, v, sizeof(v));
    if (v[0] == 1) {
        int i = hdmi_slot(g_owner);
        if (i >= 0 && g_hdmi[i].playing && !g_changing) {
            v[1] = g_hdmi[i].rate; v[2] = g_hdmi[i].ch;
        } else {
            memcpy(v, g_park, sizeof(v));
        }
    }
    int rc = real_asp_command(a0, cmd, len, v, b, c);
    if (rc == 0) memcpy(g_applied, v, sizeof(v)); else g_applied[0] = 9;
    if (memcmp(v, data, sizeof(v))) {
        int n = atomic_fetch_add(&g_dsp_events, 1);
        if (n < 40 || (n & 63) == 0)
            LOGI("dsp reference stock request %u/%u/%u steered to %u/%u/%u -> %d",
                 ((uint32_t *)data)[0], ((uint32_t *)data)[1], ((uint32_t *)data)[2], v[0], v[1], v[2], rc);
    }
    pthread_mutex_unlock(&g_dsp_lock);
    return rc;
}

struct got_job { const char *lib; const char *sym; void *hook; void **orig; int patched; };

static int patch_object(struct dl_phdr_info *info, size_t sz, void *arg)
{
    struct got_job *job = arg;
    (void)sz;
    if (!info->dlpi_name || !strstr(info->dlpi_name, job->lib)) return 0;
    const ElfW(Dyn) *dyn = NULL;
    uintptr_t relro_lo = 0, relro_hi = 0;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
        if (ph->p_type == PT_DYNAMIC) dyn = (const ElfW(Dyn) *)(info->dlpi_addr + ph->p_vaddr);
        if (ph->p_type == PT_GNU_RELRO) {
            relro_lo = info->dlpi_addr + ph->p_vaddr;
            relro_hi = relro_lo + ph->p_memsz;
        }
    }
    if (!dyn) return 0;
    const ElfW(Sym) *symtab = NULL;
    const char *strtab = NULL;
    const ElfW(Rel) *rel[2] = { NULL, NULL };
    size_t relsz[2] = { 0, 0 };
    for (const ElfW(Dyn) *d = dyn; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
        case DT_SYMTAB: symtab = (const ElfW(Sym) *)(info->dlpi_addr + d->d_un.d_ptr); break;
        case DT_STRTAB: strtab = (const char *)(info->dlpi_addr + d->d_un.d_ptr); break;
        case DT_JMPREL: rel[0] = (const ElfW(Rel) *)(info->dlpi_addr + d->d_un.d_ptr); break;
        case DT_PLTRELSZ: relsz[0] = d->d_un.d_val; break;
        case DT_REL: rel[1] = (const ElfW(Rel) *)(info->dlpi_addr + d->d_un.d_ptr); break;
        case DT_RELSZ: relsz[1] = d->d_un.d_val; break;
        }
    }
    if (!symtab || !strtab) return 0;
    long page = sysconf(_SC_PAGESIZE);
    for (int t = 0; t < 2; t++) {
        if (!rel[t]) continue;
        for (size_t k = 0; k < relsz[t] / sizeof(ElfW(Rel)); k++) {
            const ElfW(Rel) *r = &rel[t][k];
            unsigned type = ELF32_R_TYPE(r->r_info), si = ELF32_R_SYM(r->r_info);
            if ((type != R_ARM_JUMP_SLOT && type != R_ARM_GLOB_DAT) || !si) continue;
            if (strcmp(strtab + symtab[si].st_name, job->sym)) continue;
            void **slot = (void **)(info->dlpi_addr + r->r_offset);
            if (*slot == job->hook) continue;
            if (!*job->orig) *job->orig = *slot;
            else if (*slot != *job->orig) {
                LOGW("got %s in %s points elsewhere; left alone", job->sym, info->dlpi_name);
                continue;
            }
            uintptr_t pg = (uintptr_t)slot & ~(uintptr_t)(page - 1);
            int ro = (uintptr_t)slot >= relro_lo && (uintptr_t)slot < relro_hi;
            if (ro && mprotect((void *)pg, page, PROT_READ | PROT_WRITE)) {
                LOGE("mprotect %s: %s", info->dlpi_name, strerror(errno));
                continue;
            }
            __atomic_store_n(slot, job->hook, __ATOMIC_SEQ_CST);
            if (ro) mprotect((void *)pg, page, PROT_READ);
            job->patched++;
        }
    }
    return 0;
}

static int got_hook(const char *lib, const char *sym, void *hook, void **orig)
{
    struct got_job job = { lib, sym, hook, orig, 0 };
    dl_iterate_phdr(patch_object, &job);
    return job.patched;
}

static pthread_mutex_t g_got_lock = PTHREAD_MUTEX_INITIALIZER;

static void install_dsp_hooks(void)
{
    static int hal_done, voice_done;
    if (!g_dsp || (hal_done && voice_done)) return;
    pthread_mutex_lock(&g_got_lock);
    int n = 0;
    if (!hal_done) {
        const char *hal = "/audio.primary.amlogic.so";
        int k = got_hook(hal, "asp_command", (void *)hook_asp_command, (void **)&real_asp_command);
        k += got_hook(hal, "pcm_open", (void *)hook_pcm_open, (void **)&real_pcm_open);
        k += got_hook(hal, "pcm_close", (void *)hook_pcm_close, (void **)&real_pcm_close);
        k += got_hook(hal, "pcm_write", (void *)hook_pcm_write, (void **)&real_pcm_write);
        hal_done = k > 0;
        n += k;
    }
    /* the voice library's pcm_* resolve to a different tinyalsa; only its reference requests matter */
    if (!voice_done && real_asp_command) {
        int k = got_hook("/audio.voice.default.so", "asp_command", (void *)hook_asp_command,
                         (void **)&real_asp_command);
        voice_done = k > 0;
        n += k;
    }
    if (!real_pcm_is_ready) {
        void *alsa = dlopen("libamltinyalsa.so", RTLD_NOW | RTLD_NOLOAD);
        if (alsa) real_pcm_is_ready = (int (*)(void *))dlsym(alsa, "pcm_is_ready");
    }
    pthread_mutex_unlock(&g_got_lock);
    if (n) LOGI("dsp reference hooks: %d import slots", n);
}


struct stock_out_ops {
    uint32_t (*get_sample_rate)(const struct audio_stream *);
    size_t (*get_buffer_size)(const struct audio_stream *);
    audio_channel_mask_t (*get_channels)(const struct audio_stream *);
    audio_format_t (*get_format)(const struct audio_stream *);
    int (*standby)(struct audio_stream *);
    ssize_t (*write)(struct audio_stream_out *, const void *, size_t);
    int (*get_render_position)(const struct audio_stream_out *, uint32_t *);
    int (*flush)(struct audio_stream_out *);
    int (*get_presentation_position)(const struct audio_stream_out *, uint64_t *, struct timespec *);
};

struct wrapped_out {
    struct audio_stream_out *out;
    struct stock_out_ops ops;
    uint32_t rate;
    audio_channel_mask_t mask;
    uint32_t ratio;
    struct dtshd_packer pk;
    uint8_t burst[DTSHD_BURST_MAX];
    uint64_t in_bytes, out_bursts;
    uint32_t errors;
    pthread_mutex_t lock;
    struct wrapped_out *next;
};

static pthread_mutex_t g_out_lock = PTHREAD_MUTEX_INITIALIZER;
static struct wrapped_out *g_outs;

static struct wrapped_out *find_out(const void *s)
{
    pthread_mutex_lock(&g_out_lock);
    struct wrapped_out *w = g_outs;
    while (w && (const void *)w->out != s) w = w->next;
    pthread_mutex_unlock(&g_out_lock);
    return w;
}

static uint32_t w_get_sample_rate(const struct audio_stream *s)
{
    struct wrapped_out *w = find_out(s);
    return w ? w->rate : 0;
}

static size_t w_get_buffer_size(const struct audio_stream *s)
{
    struct wrapped_out *w = find_out(s);
    return w ? w->ops.get_buffer_size(s) : 0;
}

static audio_channel_mask_t w_get_channels(const struct audio_stream *s)
{
    struct wrapped_out *w = find_out(s);
    return w ? w->mask : 0;
}

static audio_format_t w_get_format(const struct audio_stream *s)
{
    return find_out(s) ? AUDIO_FORMAT_DTS_HD : 0;
}

static int w_standby(struct audio_stream *s)
{
    struct wrapped_out *w = find_out(s);
    if (!w) return -ENODEV;
    pthread_mutex_lock(&w->lock);
    dtshd_reset(&w->pk);
    pthread_mutex_unlock(&w->lock);
    return w->ops.standby(s);
}

static int w_flush(struct audio_stream_out *s)
{
    struct wrapped_out *w = find_out(s);
    if (!w) return -ENODEV;
    pthread_mutex_lock(&w->lock);
    dtshd_reset(&w->pk);
    pthread_mutex_unlock(&w->lock);
    return w->ops.flush ? w->ops.flush(s) : -ENOSYS;
}

static ssize_t w_write(struct audio_stream_out *s, const void *buf, size_t bytes)
{
    struct wrapped_out *w = find_out(s);
    if (!w) return -ENODEV;
    pthread_mutex_lock(&w->lock);
    w->in_bytes += bytes;
    if (dtshd_push(&w->pk, buf, (uint32_t)bytes) < 0) w->errors++;
    for (;;) {
        int n = dtshd_next_burst(&w->pk, w->burst, sizeof(w->burst));
        if (n == 0) break;
        if (n < 0) {
            if (w->errors++ < 8) LOGW("dtshd: skipped an access unit (%d)", n);
            continue;
        }
        size_t off = 0;
        while (off < (size_t)n) {
            ssize_t wr = w->ops.write(s, w->burst + off, n - off);
            if (wr <= 0) { if (w->errors++ < 8) LOGW("dtshd: stock write %zd", wr); break; }
            off += (size_t)wr;
        }
        if (w->out_bursts++ == 0)
            LOGI("dtshd: first burst, period %u (IEC frames at 768 kHz), %d bytes", w->pk.last_period, n);
    }
    pthread_mutex_unlock(&w->lock);
    return (ssize_t)bytes;
}

static int w_get_render_position(const struct audio_stream_out *s, uint32_t *frames)
{
    struct wrapped_out *w = find_out(s);
    if (!w || !w->ops.get_render_position) return -ENOSYS;
    int rc = w->ops.get_render_position(s, frames);
    if (!rc && frames) *frames /= w->ratio;
    return rc;
}

static int w_get_presentation_position(const struct audio_stream_out *s, uint64_t *frames, struct timespec *ts)
{
    struct wrapped_out *w = find_out(s);
    if (!w || !w->ops.get_presentation_position) return -ENOSYS;
    int rc = w->ops.get_presentation_position(s, frames, ts);
    if (!rc && frames) *frames /= w->ratio;
    return rc;
}

static int (*stock_open_output)(struct audio_hw_device *, audio_io_handle_t, audio_devices_t,
                                audio_output_flags_t, struct audio_config *, struct audio_stream_out **,
                                const char *);
static void (*stock_close_output)(struct audio_hw_device *, struct audio_stream_out *);

static int stream_fns_in_stock(struct audio_stream_out *o)
{
    return in_stock(o->common.get_sample_rate) && in_stock(o->common.get_buffer_size) &&
           in_stock(o->common.get_channels) && in_stock(o->common.get_format) &&
           in_stock(o->common.standby) && in_stock(o->write) &&
           (!o->get_presentation_position || in_stock(o->get_presentation_position)) &&
           (!o->get_render_position || in_stock(o->get_render_position)) &&
           (!o->flush || in_stock(o->flush));
}

static int open_output(struct audio_hw_device *dev, audio_io_handle_t handle, audio_devices_t devices,
                       audio_output_flags_t flags, struct audio_config *config,
                       struct audio_stream_out **out, const char *address)
{
    int want = g_dtshd && config && (config->format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_DTS_HD &&
               (flags & AUDIO_OUTPUT_FLAG_DIRECT) &&
               !(flags & (AUDIO_OUTPUT_FLAG_HW_AV_SYNC | AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD |
                          AUDIO_OUTPUT_FLAG_MMAP_NOIRQ)) &&
               config->sample_rate && 192000 % config->sample_rate == 0 && config->sample_rate <= 96000;
    if (want && !read_caps().dtshd_hbr) {
        LOGI("dtshd: sink has no DTS-HD 8ch/192k; stock path");
        want = 0;
    }
    if (!want) return stock_open_output(dev, handle, devices, flags, config, out, address);

    struct audio_config inner = *config;
    inner.format = AUDIO_FORMAT_IEC61937;
    inner.sample_rate = 192000;
    inner.channel_mask = AUDIO_CHANNEL_OUT_7POINT1;
    int rc = stock_open_output(dev, handle, devices, flags, &inner, out, address);
    if (rc || !*out || !stream_fns_in_stock(*out)) {
        LOGW("dtshd: IEC open rc %d (%p); stock DTS-HD path", rc, out ? *out : NULL);
        if (!rc && *out) stock_close_output(dev, *out);
        return stock_open_output(dev, handle, devices, flags, config, out, address);
    }
    struct wrapped_out *w = calloc(1, sizeof(*w));
    if (!w) { stock_close_output(dev, *out); return stock_open_output(dev, handle, devices, flags, config, out, address); }
    struct audio_stream_out *o = *out;
    w->out = o;
    w->rate = config->sample_rate;
    w->mask = config->channel_mask ? config->channel_mask : AUDIO_CHANNEL_OUT_7POINT1;
    w->ratio = 192000 / config->sample_rate;
    pthread_mutex_init(&w->lock, NULL);
    w->ops.get_sample_rate = o->common.get_sample_rate;
    w->ops.get_buffer_size = o->common.get_buffer_size;
    w->ops.get_channels = o->common.get_channels;
    w->ops.get_format = o->common.get_format;
    w->ops.standby = o->common.standby;
    w->ops.write = o->write;
    w->ops.get_render_position = o->get_render_position;
    w->ops.flush = o->flush;
    w->ops.get_presentation_position = o->get_presentation_position;
    pthread_mutex_lock(&g_out_lock);
    w->next = g_outs;
    g_outs = w;
    pthread_mutex_unlock(&g_out_lock);
    o->common.get_sample_rate = w_get_sample_rate;
    o->common.get_buffer_size = w_get_buffer_size;
    o->common.get_channels = w_get_channels;
    o->common.get_format = w_get_format;
    o->common.standby = w_standby;
    o->write = w_write;
    if (o->get_render_position) o->get_render_position = w_get_render_position;
    if (o->flush) o->flush = w_flush;
    if (o->get_presentation_position) o->get_presentation_position = w_get_presentation_position;
    LOGI("dtshd: stream %p opened as IEC 61937 192k 7.1 (content %u Hz, mask %#x, flags %#x)",
         (void *)o, w->rate, config->channel_mask, flags);
    return 0;
}

#define OUT_PROP "vendor.media.audio.tvp.out"
#define MAX_PEEK 4

static pthread_mutex_t g_pub_lock = PTHREAD_MUTEX_INITIALIZER;
static const void *g_shown;
static struct peek {
    struct audio_stream_out *o;
    ssize_t (*write)(struct audio_stream_out *, const void *, size_t);
    struct sniff *sn;
    const char *codec, *src;
    int bitstream;
    uint32_t rate, ch;
} g_peek[MAX_PEEK];

static void publish(const void *stream, const char *codec, int bitstream, uint32_t rate, uint32_t ch,
                    int atmos, int dtsx, const char *src)
{
    char v[PROP_VALUE_MAX];
    snprintf(v, sizeof(v), "codec=%s bitstream=%d rate=%u ch=%u atmos=%d dtsx=%d t=%ld%s%s", codec, bitstream,
             rate, ch, atmos, dtsx, (long)time(NULL), src ? " src=" : "", src ? src : "");
    pthread_mutex_lock(&g_pub_lock);
    g_shown = stream;
    atomic_store(&g_out_bits, bitstream);
    __system_property_set(OUT_PROP, v);
    pthread_mutex_unlock(&g_pub_lock);
}

static const char *iec_name(int codec)
{
    switch (codec) {
    case SN_AC3: return "ac3";
    case SN_EAC3: return "eac3";
    case SN_TRUEHD: return "mat";
    case SN_DTS: return "dts";
    case SN_DTSHD: return "dtshd";
    default: return NULL;
    }
}

static ssize_t peek_write(struct audio_stream_out *o, const void *buf, size_t bytes)
{
    ssize_t (*orig)(struct audio_stream_out *, const void *, size_t) = NULL;
    struct peek done = { 0 };
    pthread_mutex_lock(&g_pub_lock);
    for (int i = 0; i < MAX_PEEK; i++) {
        struct peek *p = &g_peek[i];
        if (p->o != o) continue;
        orig = p->write;
        if (sniff_feed(p->sn, buf, (uint32_t)bytes)) {
            done = *p;
            if (p->sn->iec) done.codec = iec_name(p->sn->codec);
            o->write = orig;
            p->o = NULL;
        }
        break;
    }
    pthread_mutex_unlock(&g_pub_lock);
    if (done.sn) {
        if (done.codec)
            publish(o, done.codec, done.bitstream, done.rate, done.ch, done.sn->atmos, done.sn->dtsx, done.src);
        LOGI("stream %p: %s atmos %d dtsx %d", (void *)o, sniff_name(done.sn->codec), done.sn->atmos, done.sn->dtsx);
        wire_arm();
        free(done.sn);
    }
    return orig ? orig(o, buf, bytes) : -ENODEV;
}

static void peek_start(struct audio_stream_out *o, int iec, int sn_codec, const char *codec, int bitstream,
                       uint32_t rate, uint32_t ch, const char *src)
{
    struct sniff *sn = malloc(sizeof(*sn));
    if (!sn) return;
    sniff_init(sn, iec, sn_codec);
    pthread_mutex_lock(&g_pub_lock);
    for (int i = 0; i < MAX_PEEK; i++) if (!g_peek[i].o) {
        g_peek[i] = (struct peek){ o, o->write, sn, codec, src, bitstream, rate, ch };
        o->write = peek_write;
        sn = NULL;
        break;
    }
    pthread_mutex_unlock(&g_pub_lock);
    free(sn);
}

static void describe_open(struct audio_stream_out *o, audio_output_flags_t flags, const struct audio_config *c)
{
    if (!(flags & AUDIO_OUTPUT_FLAG_DIRECT) || !c) return;
    uint32_t fmt = c->format, main = fmt & AUDIO_FORMAT_MAIN_MASK, ch = (uint32_t)__builtin_popcount(c->channel_mask);
    int bypass = atomic_load(&g_hdmi_format) == 6;
    int joc = fmt == AUDIO_FORMAT_E_AC3_JOC ? 1 : -1;
    if (main == AUDIO_FORMAT_IEC61937) {
        peek_start(o, 1, SN_UNKNOWN, NULL, 1, c->sample_rate, ch, NULL);
    } else if (main == AUDIO_FORMAT_E_AC3) {
        publish(o, "eac3", 1, 192000, 2, joc, 0, "eac3");
        peek_start(o, 0, SN_EAC3, "eac3", 1, 192000, 2, "eac3");
    } else if (main == AUDIO_FORMAT_AC3) {
        publish(o, "ac3", 1, 48000, 2, 0, 0, "ac3");
    } else if (main == AUDIO_FORMAT_DTS) {
        publish(o, "dts", 1, 48000, 2, 0, 0, "dts");
    } else if (main == AUDIO_FORMAT_DTS_HD) {
        const char *k = bypass ? "dtshd" : "dts";
        publish(o, k, 1, bypass ? 192000 : 48000, bypass ? 8 : 2, 0, -1, "dtshd");
        peek_start(o, 0, SN_DTSHD, k, 1, bypass ? 192000 : 48000, bypass ? 8 : 2, "dtshd");
    } else if (main == AUDIO_FORMAT_DOLBY_TRUEHD) {
        const char *k = bypass ? "mat" : "pcm";
        publish(o, k, bypass, bypass ? 192000 : 48000, bypass ? 8 : ch, -1, 0, "truehd");
        peek_start(o, 0, SN_TRUEHD, k, bypass, bypass ? 192000 : 48000, bypass ? 8 : ch, "truehd");
    } else if (main == 0) {
        publish(o, "pcm", 0, c->sample_rate, ch, 0, 0, NULL);
    }
}

static int tvp_open_output(struct audio_hw_device *dev, audio_io_handle_t handle, audio_devices_t devices,
                           audio_output_flags_t flags, struct audio_config *config,
                           struct audio_stream_out **out, const char *address)
{
    install_dsp_hooks();
    struct audio_config asked = config ? *config : (struct audio_config){ 0 };
    int rc = open_output(dev, handle, devices, flags, config, out, address);
    if (!rc && out && *out && config) describe_open(*out, flags, &asked);
    if (!rc) wire_arm();
    return rc;
}

static void tvp_close_output(struct audio_hw_device *dev, struct audio_stream_out *o)
{
    pthread_mutex_lock(&g_pub_lock);
    for (int i = 0; i < MAX_PEEK; i++) if (g_peek[i].o == o) {
        o->write = g_peek[i].write;
        g_peek[i].o = NULL;
        free(g_peek[i].sn);
    }
    int shown = g_shown == o;
    pthread_mutex_unlock(&g_pub_lock);
    if (shown) publish(NULL, "none", 0, 0, 0, -1, -1, NULL);
    pthread_mutex_lock(&g_out_lock);
    struct wrapped_out **pp = &g_outs, *w = NULL;
    while (*pp && (*pp)->out != o) pp = &(*pp)->next;
    if (*pp) { w = *pp; *pp = w->next; }
    pthread_mutex_unlock(&g_out_lock);
    if (w) {
        o->common.get_sample_rate = w->ops.get_sample_rate;
        o->common.get_buffer_size = w->ops.get_buffer_size;
        o->common.get_channels = w->ops.get_channels;
        o->common.get_format = w->ops.get_format;
        o->common.standby = w->ops.standby;
        o->write = w->ops.write;
        o->get_render_position = w->ops.get_render_position;
        o->flush = w->ops.flush;
        o->get_presentation_position = w->ops.get_presentation_position;
        LOGI("dtshd: stream %p closed: %llu bytes in, %llu bursts, %u resyncs, %u errors", (void *)o,
             (unsigned long long)w->in_bytes, (unsigned long long)w->out_bursts, w->pk.resyncs, w->errors);
    }
    stock_close_output(dev, o);
    if (w) { pthread_mutex_destroy(&w->lock); free(w); }
}


static void read_switches(void)
{
    char v[PROP_VALUE_MAX] = "";
    __system_property_get("persist.vendor.tvp.audio", v);
    if (strstr(v, "nobypass")) g_bypass = 0;
    if (strstr(v, "nodsp")) g_dsp = 0;
    if (strstr(v, "nodtshd")) g_dtshd = 0;
    LOGI("Tesseract VP audio %s: bypass %d, dsp %d, dtshd %d ('%s')", TVP_VERSION, g_bypass, g_dsp, g_dtshd, v);
}

static int find_stock(struct dl_phdr_info *info, size_t sz, void *arg)
{
    (void)sz; (void)arg;
    if (!info->dlpi_name || !strstr(info->dlpi_name, "/audio.primary.amlogic.so")) return 0;
    uintptr_t hi = 0;
    for (int i = 0; i < info->dlpi_phnum; i++)
        if (info->dlpi_phdr[i].p_type == PT_LOAD) {
            uintptr_t end = info->dlpi_phdr[i].p_vaddr + info->dlpi_phdr[i].p_memsz;
            if (end > hi) hi = end;
        }
    g_stock_base = info->dlpi_addr;
    g_stock_size = hi;
    return 1;
}

static int tvp_open(const struct hw_module_t *module, const char *id, struct hw_device_t **device)
{
    (void)module;
    if (!g_stock_dso) {
        g_stock_dso = dlopen(STOCK_HAL, RTLD_NOW);
        if (!g_stock_dso) { LOGE("dlopen %s: %s", STOCK_HAL, dlerror()); return -ENOENT; }
        g_stock_hmi = (struct hw_module_t *)dlsym(g_stock_dso, "HMI");
        if (!g_stock_hmi || !g_stock_hmi->methods || !g_stock_hmi->methods->open) {
            LOGE("stock HAL has no HMI");
            return -ENOENT;
        }
        dl_iterate_phdr(find_stock, NULL);
        read_switches();
    }
    int rc = g_stock_hmi->methods->open(g_stock_hmi, id, device);
    if (rc || !*device) return rc;
    struct audio_hw_device *dev = (struct audio_hw_device *)*device;
    if (!in_stock(dev->set_parameters) || !in_stock(dev->open_output_stream) ||
        !in_stock(dev->close_output_stream)) {
        LOGE("unexpected device layout; running stock HAL untouched");
        return 0;
    }
    stock_set_parameters = dev->set_parameters;
    stock_open_output = dev->open_output_stream;
    stock_close_output = dev->close_output_stream;
    dev->set_parameters = tvp_set_parameters;
    dev->open_output_stream = tvp_open_output;
    dev->close_output_stream = tvp_close_output;
    install_dsp_hooks();
    struct sink_caps c = read_caps();
    LOGI("device %p wrapped (stock base %#lx); sink bitstream %d, dts-hd hbr %d, mat %d", (void *)dev,
         (unsigned long)g_stock_base, c.bitstream, c.dtshd_hbr, c.mat);
    return 0;
}

static struct hw_module_methods_t tvp_methods = { .open = tvp_open };

__attribute__((visibility("default")))
struct hw_module_t HMI = {
    .tag = HARDWARE_MODULE_TAG,
    .module_api_version = 1,
    .hal_api_version = 0x0100,
    .id = "audio",
    .name = "Tesseract VP audio passthrough (wraps Amlogic primary)",
    .author = "Tesseract VP",
    .methods = &tvp_methods,
};
