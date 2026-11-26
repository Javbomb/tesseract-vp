/* Host check: sniff_test <iec 0|1> <codec hint> <file> */
#include "sniff.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    static struct sniff s;
    static uint8_t chunk[40000];
    if (argc < 4) return 2;
    FILE *f = fopen(argv[3], "rb");
    if (!f) return 2;
    sniff_init(&s, atoi(argv[1]), atoi(argv[2]));
    srand(7);
    size_t n;
    while ((n = fread(chunk, 1, 1 + rand() % sizeof(chunk), f)) > 0)
        if (sniff_feed(&s, chunk, (uint32_t)n)) break;
    printf("%s codec=%s atmos=%d dtsx=%d after %llu bytes\n", argv[3], sniff_name(s.codec), s.atmos, s.dtsx,
           (unsigned long long)s.fed);
    return 0;
}
