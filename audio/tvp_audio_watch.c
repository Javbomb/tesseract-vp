/* Sleeps on kernel uevents; when the receiver's audio formats change, has Android read them again. */
#define LOG_TAG "TesseractAudio"
#include <android/log.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define AUD_CAP "/sys/class/amhdmitx/amhdmitx0/aud_cap"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static int read_caps(char *b, size_t cap)
{
    FILE *f = fopen(AUD_CAP, "r");
    if (!f) return 0;
    size_t n = fread(b, 1, cap - 1, f);
    fclose(f);
    b[n] = 0;
    return (int)n;
}

static int refresh(const char *dex)
{
    pid_t p = fork();
    if (p < 0) return -1;
    if (!p) {
        setenv("CLASSPATH", dex, 1);
        execl("/system/bin/app_process", "app_process", "/system/bin", "HdmiRefresh", (char *)NULL);
        _exit(127);
    }
    int st = 0;
    waitpid(p, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static int hdmi_event(const char *m, ssize_t n)
{
    for (const char *p = m; p < m + n; p += strlen(p) + 1)
        if (!strcmp(p, "SWITCH_NAME=hdmi") || !strcmp(p, "SWITCH_NAME=hdmi_audio") || strstr(p, "amhdmitx"))
            return 1;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    pid_t d = fork();
    if (d) return d < 0;
    setsid();
    int nul = open("/dev/null", O_RDWR);
    if (nul >= 0) { dup2(nul, 0); dup2(nul, 1); dup2(nul, 2); if (nul > 2) close(nul); }
    struct sockaddr_un one = { .sun_family = AF_UNIX, .sun_path = "\0tvp_audio_watch" };
    int g = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (g < 0 || bind(g, (struct sockaddr *)&one, sizeof(sa_family_t) + 17) < 0) return 0;
    struct sockaddr_nl sa = { .nl_family = AF_NETLINK, .nl_groups = 1 };
    int s = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
    if (s < 0 || bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) return 1;
    static char last[4096], now[4096], again[4096], msg[8192];
    int due = 1, tries = 0;
    for (;;) {
        struct pollfd pf = { s, POLLIN, 0 };
        int r = poll(&pf, 1, due ? (tries ? 30000 : 3000) : -1);
        if (r > 0) {
            ssize_t n = recv(s, msg, sizeof(msg) - 1, 0);
            if (n > 0) { msg[n] = 0; if (hdmi_event(msg, n)) { due = 1; tries = 0; } }
            continue;
        }
        if (!due) continue;
        if (!read_caps(now, sizeof(now)) || !strstr(now, "PCM")) { due = 0; continue; }
        sleep(1);
        read_caps(again, sizeof(again));
        if (strcmp(now, again)) continue;
        if (!strcmp(now, last)) { due = 0; continue; }
        int rc = refresh(argv[1]);
        LOGI("hdmi formats changed; Android refresh -> %d", rc);
        if (rc == 0 || rc == 2 || ++tries > 20) { memcpy(last, now, sizeof(last)); due = 0; tries = 0; }
    }
}
