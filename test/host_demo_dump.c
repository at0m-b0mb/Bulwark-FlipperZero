/**
 * Bulwark - what the engine really says about each demo scene.
 *
 * tools_gen_mockups.py draws the README screenshots from this file, so the
 * numbers on those screenshots are numbers the engine produced, not numbers
 * somebody thought looked convincing.
 *
 *   make -C test dump
 */
#include "../helpers/bw_score.h"
#include "../helpers/bw_demo.h"

#include <stdio.h>
#include <string.h>

#define DUMP_SWEEPS 10
#define DUMP_SAMPLES 600
#define DUMP_SEED 0x5EEDu

static void print_escaped(const char* s) {
    for(; *s; s++) {
        if(*s == '"' || *s == '\\') putchar('\\');
        putchar(*s);
    }
}

int main(void) {
    printf("{\n");
    printf("  \"ceiling\": %u,\n", BW_SCORE_CEILING);
    printf("  \"gate_ppt\": %u,\n", BW_GATE_PPT);
    printf("  \"channels\": [\n");
    for(size_t c = 0; c < BwChanCount; c++) {
        const BwChanInfo* ci = &bw_chan_info[c];
        printf(
            "    {\"label\": \"%s\", \"mhz\": %u, \"rf\": %u, \"adv\": %s}%s\n",
            ci->label,
            ci->mhz,
            ci->rf_index,
            bw_chan_is_adv((BwChan)c) ? "true" : "false",
            (c + 1 < BwChanCount) ? "," : "");
    }
    printf("  ],\n");
    printf("  \"scenes\": [\n");

    for(size_t s = 0; s < BwDemoCount; s++) {
        BwWatch w;
        BwScore sc;
        BwSweep sweep;
        memset(&w, 0, sizeof(w));
        memset(&sc, 0, sizeof(sc));
        memset(&sweep, 0, sizeof(sweep));

        for(int i = 0; i < DUMP_SWEEPS; i++) {
            BwSweepAccum acc;
            bw_demo_fill((BwDemoScene)s, DUMP_SEED + (uint32_t)i * 7919u, DUMP_SAMPLES, &acc);
            bw_stats_finalize(&acc, &sweep);
            bw_watch_feed(&w, &sweep, &sc);
        }

        printf("    {\n");
        printf("      \"name\": \"");
        print_escaped(bw_demo_name((BwDemoScene)s));
        printf("\",\n      \"blurb\": \"");
        print_escaped(bw_demo_blurb((BwDemoScene)s));
        printf("\",\n");
        printf("      \"score\": %u,\n", sc.score);
        printf("      \"raw_score\": %u,\n", sc.raw_score);
        printf("      \"verdict\": \"%s\",\n", bw_verdict_name(sc.verdict));
        printf("      \"verdict_blurb\": \"");
        print_escaped(bw_verdict_blurb(sc.verdict));
        printf("\",\n");
        printf("      \"interference\": \"%s\",\n", bw_interference_name(sc.interference));
        printf("      \"streak\": %u,\n", w.streak);
        printf("      \"peak_score\": %u,\n", w.peak_score);
        printf("      \"band_floor_dbm\": %d,\n", sweep.band_floor_dbm);
        printf("      \"adv_peak_dbm\": %d,\n", sweep.adv_peak_dbm);
        printf("      \"adv_swing_db\": %u,\n", sweep.adv_swing_db);
        printf("      \"adv_busy_ppt\": %u,\n", sweep.adv_busy_ppt);
        printf("      \"ref_busy_ppt\": %u,\n", sweep.ref_busy_ppt);
        printf("      \"rate_hz\": %u,\n", sweep.rate_hz);
        printf("      \"binding_cap\": \"%s\",\n", bw_cap_name(sc.binding_cap));
        printf("      \"binding_cap_reason\": \"");
        print_escaped(bw_cap_reason(sc.binding_cap));
        printf("\",\n");

        printf("      \"caps\": [");
        bool first = true;
        for(size_t c = 1; c < BwCapCount; c++) {
            if(!(sc.caps_applied & (1u << c))) continue;
            printf("%s\"%s\"", first ? "" : ", ", bw_cap_name((BwCap)c));
            first = false;
        }
        printf("],\n");

        printf("      \"chan\": [\n");
        for(size_t c = 0; c < BwChanCount; c++) {
            printf(
                "        {\"label\": \"%s\", \"busy_ppt\": %u, \"hot_ppt\": %u, "
                "\"floor_dbm\": %d, \"peak_dbm\": %d}%s\n",
                bw_chan_info[c].label,
                sweep.chan[c].busy_ppt,
                sweep.chan[c].hot_ppt,
                sweep.chan[c].floor_dbm,
                sweep.chan[c].peak_dbm,
                (c + 1 < BwChanCount) ? "," : "");
        }
        printf("      ],\n");

        printf("      \"families\": [\n");
        for(size_t f = 0; f < BwFamilyCount; f++) {
            printf(
                "        {\"name\": \"%s\", \"points\": %u, \"max\": %u}%s\n",
                bw_family_name((BwFamily)f),
                sc.family_points[f],
                sc.family_max[f],
                (f + 1 < BwFamilyCount) ? "," : "");
        }
        printf("      ],\n");

        printf("      \"signals\": [\n");
        for(size_t i = 0; i < BwSigCount; i++) {
            printf("        {\"name\": \"");
            print_escaped(sc.sig[i].name);
            printf("\", \"detail\": \"");
            print_escaped(sc.sig[i].detail);
            printf(
                "\", \"points\": %u, \"max\": %u, \"family\": \"%s\"}%s\n",
                sc.sig[i].points,
                sc.sig[i].max_points,
                bw_family_name(sc.sig[i].family),
                (i + 1 < BwSigCount) ? "," : "");
        }
        printf("      ]\n");
        printf("    }%s\n", (s + 1 < BwDemoCount) ? "," : "");
    }

    printf("  ]\n}\n");
    return 0;
}
