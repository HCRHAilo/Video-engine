/*
 * verify_format.c - prove the FullScale USB pixel format over EVERY frame
 *
 * Hypothesis under test (from frame 5 analysis):
 *   - 2 bytes per pixel, big-endian (high byte first)
 *   - 14-bit sensor value in bits 15:2, bits 1:0 always 1 (padding)
 *   - value14 = ((b0 << 8) | b1) >> 2
 *
 * Build:  gcc -O2 -Wall -o verify_format verify_format.c -lm
 * Usage:  ./verify_format raw.bin [width=640] [height=480]
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s raw.bin [width] [height]\n", argv[0]);
        return 1;
    }
    int W = argc > 2 ? atoi(argv[2]) : 640;
    int H = argc > 3 ? atoi(argv[3]) : 480;
    size_t npix = (size_t)W * H, fbytes = npix * 2;

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }

    uint8_t *buf = malloc(fbytes);
    uint16_t *cur = malloc(npix * 2), *prev = malloc(npix * 2);
    long frame = 0, pad_violations_total = 0;

    printf("frame | pad!=11 | min14  max14  mean14   | diff vs prev: mean|d|  max|d|  identical\n");
    while (fread(buf, 1, fbytes, f) == fbytes) {
        long pad_bad = 0;
        uint16_t mn = 0xFFFF, mx = 0;
        double sum = 0;

        for (size_t i = 0; i < npix; i++) {
            uint16_t w = (uint16_t)((buf[2 * i] << 8) | buf[2 * i + 1]);  /* big-endian */
            if ((w & 0x3) != 0x3) pad_bad++;                             /* padding check */
            uint16_t v = w >> 2;                                         /* 14-bit value */
            cur[i] = v;
            if (v < mn) mn = v;
            if (v > mx) mx = v;
            sum += v;
        }
        pad_violations_total += pad_bad;

        printf("%5ld | %7ld | %5u  %5u  %8.1f |", frame, pad_bad, mn, mx, sum / npix);
        if (frame > 0) {
            double sd = 0;
            int maxd = 0;
            size_t same = 0;
            for (size_t i = 0; i < npix; i++) {
                int d = abs((int)cur[i] - (int)prev[i]);
                sd += d;
                if (d > maxd) maxd = d;
                if (d == 0) same++;
            }
            printf("   %7.2f  %6d  %s", sd / npix, maxd,
                   same == npix ? "YES (duplicate frame!)" : "no");
        }
        printf("\n");

        uint16_t *t = prev; prev = cur; cur = t;
        frame++;
    }
    fclose(f);

    printf("\nframes checked: %ld\n", frame);
    printf("pixels with bits1:0 != 11 : %ld  -> %s\n", pad_violations_total,
           pad_violations_total == 0 ? "padding hypothesis HOLDS for every pixel of every frame"
                                     : "padding hypothesis FAILS: bits 1:0 carry data somewhere");
    free(buf); free(cur); free(prev);
    return 0;
}
