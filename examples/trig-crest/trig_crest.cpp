/*
 * trig_crest.cpp — show exact vs s16-safe sin/cos crest
 *
 * Built twice:
 *   trig_crest_exact     — default library (exact ±1.0 = ±65536)
 *   trig_crest_s16_safe  — -DFR_TRIG_S16_SAFE (crest 65534)
 *
 * Both still return s15.16. The difference is only the crest magnitude
 * so a blind (s16)(x >> 1) either wraps or stays in range.
 */

#include <stdio.h>
#include "FR_math.h"

static void show(const char *label, s32 v)
{
    s32 sh = v >> 1;
    s16 cast = (s16)sh;
    printf("  %-18s raw=%7ld  >>1=%7ld  (s16)(>>1)=%7d%s\n",
           label, (long)v, (long)sh, (int)cast,
           (cast != sh) ? "  <-- WRAP" : "");
}

int main(void)
{
#ifdef FR_TRIG_S16_SAFE
    const char *mode = "FR_TRIG_S16_SAFE (s16-safe crest)";
    const s32 expect = 65534;
#else
    const char *mode = "default (exact ±1.0 crest)";
    const s32 expect = FR_TRIG_ONE; /* 65536 */
#endif

    printf("FR_Math %s — trig crest demo\n", FR_MATH_VERSION);
    printf("Build: %s\n", mode);
    printf("Expected |crest| = %ld (still s15.16, not q1.15)\n\n",
           (long)expect);

    show("sin(0)",   fr_sin_bam(0));
    show("sin(90)",  fr_sin_bam(0x4000));
    show("sin(180)", fr_sin_bam(0x8000));
    show("sin(270)", fr_sin_bam(0xC000));
    show("cos(0)",   fr_cos_bam(0));
    show("cos(90)",  fr_cos_bam(0x4000));
    show("cos(180)", fr_cos_bam(0x8000));
    show("FR_CosI(0)", FR_CosI(0));
    show("FR_SinI(90)", FR_SinI(90));

    s32 peak = fr_sin_bam(0x4000);
    s16 stored = (s16)(peak >> 1);
    printf("\nBlind-cast summary: sin(90)=%ld → (s16)(>>1)=%d",
           (long)peak, (int)stored);
#ifdef FR_TRIG_S16_SAFE
    printf(" (safe: ±32767)\n");
    if (peak != expect || stored != 32767)
        return 1;
#else
    printf(" (wraps: 32768 → -32768)\n");
    if (peak != expect || stored != (s16)-32768)
        return 1;
#endif
    return 0;
}
