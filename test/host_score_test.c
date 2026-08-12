/**
 * Bulwark - the engine, on the host, taken apart.
 *
 * Everything Bulwark puts on the screen is a rendering of what bw_stats and
 * bw_score decided. A screenshot cannot vouch for any of it, so the
 * histogram maths, the thresholds, the family ceilings, the caps and the
 * promises the README makes are all checked here, on every push.
 *
 *   make -C test
 */
#include "../helpers/bw_score.h"
#include "../helpers/bw_demo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long checks = 0;
static unsigned long failures = 0;

#define CHECK(cond, ...)                                    \
    do {                                                    \
        checks++;                                           \
        if(!(cond)) {                                       \
            failures++;                                     \
            printf("  FAIL %s:%d  ", __FILE__, __LINE__);   \
            printf(__VA_ARGS__);                            \
            printf("\n    condition: %s\n", #cond);         \
            if(failures > 30) {                             \
                printf("  ... too many failures, giving up\n"); \
                exit(1);                                    \
            }                                               \
        }                                                   \
    } while(0)

static void section(const char* name) {
    printf("\n== %s\n", name);
}

static uint32_t rng_state = 0xC0FFEE11u;
static uint32_t rnd(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}
static uint32_t rnd_range(uint32_t lo, uint32_t hi) {
    return lo + (rnd() % (hi - lo + 1));
}

/* ------------------------------------------------------------ channel plan */

static void test_channel_plan(void) {
    section("channel plan");

    /* The whole app rests on these six frequencies being the right ones. */
    CHECK(bw_chan_info[BwChanAdv37].mhz == 2402, "adv37 must be 2402 MHz");
    CHECK(bw_chan_info[BwChanAdv38].mhz == 2426, "adv38 must be 2426 MHz");
    CHECK(bw_chan_info[BwChanAdv39].mhz == 2480, "adv39 must be 2480 MHz");

    for(size_t i = 0; i < BwChanCount; i++) {
        const BwChanInfo* c = &bw_chan_info[i];
        /* The radio numbers channels 0..39 as 2402 + 2*index. */
        CHECK(c->rf_index <= 39, "rf index %u out of range", c->rf_index);
        CHECK(
            c->mhz == 2402u + 2u * c->rf_index,
            "channel %zu: %u MHz does not match rf index %u",
            i,
            c->mhz,
            c->rf_index);
        CHECK(c->label != NULL && c->label[0] != '\0', "channel %zu needs a label", i);
    }

    size_t adv = 0, ref = 0;
    for(size_t i = 0; i < BwChanCount; i++) {
        if(bw_chan_is_adv((BwChan)i))
            adv++;
        else
            ref++;
    }
    CHECK(adv == BW_ADV_COUNT, "expected %u advertising channels, found %zu", BW_ADV_COUNT, adv);
    CHECK(ref == BW_REF_COUNT, "expected %u reference channels, found %zu", BW_REF_COUNT, ref);

    for(size_t i = 0; i < BW_ADV_COUNT; i++) {
        CHECK(bw_chan_is_adv(bw_chan_adv[i]), "bw_chan_adv[%zu] is not an adv channel", i);
    }
    for(size_t i = 0; i < BW_REF_COUNT; i++) {
        CHECK(!bw_chan_is_adv(bw_chan_ref[i]), "bw_chan_ref[%zu] is an adv channel", i);
    }

    /* Every reference channel must sit between advertising channels in
     * frequency - that is what makes the guard-contrast signal mean
     * anything. */
    for(size_t i = 0; i < BW_REF_COUNT; i++) {
        uint16_t f = bw_chan_info[bw_chan_ref[i]].mhz;
        CHECK(
            f > bw_chan_info[BwChanAdv37].mhz && f < bw_chan_info[BwChanAdv39].mhz,
            "reference channel at %u MHz is outside the advertising span",
            f);
    }
}

/* ------------------------------------------------------------- percentiles */

static int cmp_int(const void* a, const void* b) {
    int x = *(const int*)a, y = *(const int*)b;
    return (x > y) - (x < y);
}

static void test_percentile(void) {
    section("percentiles vs brute force");

    for(int trial = 0; trial < 300; trial++) {
        BwChanAccum acc;
        memset(&acc, 0, sizeof(acc));
        acc.peak_dbm = -127;

        int n = (int)rnd_range(1, 400);
        int* vals = malloc(sizeof(int) * (size_t)n);
        for(int i = 0; i < n; i++) {
            int dbm = -(int)rnd_range(30, 110);
            vals[i] = dbm;
            acc.hist[-dbm]++;
            acc.n++;
            if(dbm > acc.peak_dbm) acc.peak_dbm = (int8_t)dbm;
        }
        qsort(vals, (size_t)n, sizeof(int), cmp_int);

        const uint32_t pcts[] = {0, 1, 20, 50, 90, 99, 100};
        for(size_t p = 0; p < sizeof(pcts) / sizeof(pcts[0]); p++) {
            uint32_t want = ((uint32_t)n * pcts[p] + 99) / 100;
            if(want == 0) want = 1;
            int expect = vals[want - 1];
            int got = bw_stats_percentile(&acc, pcts[p]);
            CHECK(
                got == expect,
                "n=%d pct=%u: got %d dBm, brute force says %d dBm",
                n,
                pcts[p],
                got,
                expect);
        }
        free(vals);
    }

    /* An empty channel has no percentile to give. */
    BwChanAccum empty;
    memset(&empty, 0, sizeof(empty));
    empty.peak_dbm = -127;
    CHECK(bw_stats_percentile(&empty, 50) == -127, "empty channel should read as the floor");
}

/* --------------------------------------------------------------- sampling */

static void test_sample_hygiene(void) {
    section("sample hygiene");

    BwSweepAccum a;
    bw_stats_reset(&a);

    /* The stack returns exactly 0 dBm when the RSSI read failed. That must
     * never become the loudest sample of the sweep. */
    for(int i = 0; i < 100; i++) bw_stats_add(&a, BwChanAdv37, 0);
    CHECK(a.acc[BwChanAdv37].n == 0, "failed reads (0 dBm) must be dropped");
    CHECK(a.acc[BwChanAdv37].peak_dbm == -127, "failed reads must not set the peak");

    for(int i = 0; i < 10; i++) bw_stats_add(&a, BwChanAdv37, 12);
    CHECK(a.acc[BwChanAdv37].n == 0, "positive dBm must be dropped too");

    /* Out-of-range values clamp rather than running off the histogram. */
    bw_stats_add(&a, BwChanAdv37, -400);
    CHECK(a.acc[BwChanAdv37].n == 1, "a clamped sample still counts");
    CHECK(a.acc[BwChanAdv37].hist[127] == 1, "-400 dBm should land in the last bin");

    /* An out-of-range channel index must not write anywhere. */
    BwSweepAccum b;
    bw_stats_reset(&b);
    bw_stats_add(&b, (BwChan)BwChanCount, -60);
    bw_stats_add(&b, (BwChan)99, -60);
    uint32_t total = 0;
    for(size_t i = 0; i < BwChanCount; i++) total += b.acc[i].n;
    CHECK(total == 0, "out-of-range channels must be ignored, got %u samples", total);
}

static void test_occupancy_maths(void) {
    section("occupancy maths");

    BwSweepAccum a;
    bw_stats_reset(&a);

    /* Every channel flat at -90 except adv38, which is busy exactly 25% of
     * the time at -50. The band floor should be -90 and adv38 should read
     * 250 parts per thousand. */
    for(int i = 0; i < 400; i++) {
        for(size_t c = 0; c < BwChanCount; c++) {
            if(c == BwChanAdv38 && (i % 4) == 0)
                bw_stats_add(&a, (BwChan)c, -50);
            else
                bw_stats_add(&a, (BwChan)c, -90);
        }
    }
    a.duration_ms = 1000;

    BwSweep s;
    bw_stats_finalize(&a, &s);
    CHECK(s.valid, "sweep should be valid with 400 samples per channel");
    CHECK(s.band_floor_dbm == -90, "band floor should be -90, got %d", s.band_floor_dbm);
    CHECK(
        s.chan[BwChanAdv38].busy_ppt == 250,
        "adv38 should read 250 ppt busy, got %u",
        s.chan[BwChanAdv38].busy_ppt);
    CHECK(
        s.chan[BwChanAdv37].busy_ppt == 0,
        "a flat channel should read 0 ppt busy, got %u",
        s.chan[BwChanAdv37].busy_ppt);
    CHECK(
        s.chan[BwChanAdv38].hot_ppt == 250,
        "-50 is 40 dB over the floor, so hot should also be 250, got %u",
        s.chan[BwChanAdv38].hot_ppt);
    CHECK(s.adv_busy_max_ppt == 250, "max adv busy should be 250, got %u", s.adv_busy_max_ppt);
    CHECK(s.adv_busy_min_ppt == 0, "min adv busy should be 0, got %u", s.adv_busy_min_ppt);
    CHECK(s.adv_peak_dbm == -50, "adv peak should be -50, got %d", s.adv_peak_dbm);
    CHECK(s.adv_swing_db == 40, "swing should be 40 dB, got %u", s.adv_swing_db);
    CHECK(s.rate_hz == BwChanCount * 400u, "rate should be reported, got %u Hz", s.rate_hz);

    /* A transmitter that never stops lifts its own channel's floor. Measuring
     * against that floor would hide it completely; measuring against the
     * quietest corner of the band is what catches it. */
    BwSweepAccum cont;
    bw_stats_reset(&cont);
    for(int i = 0; i < 400; i++) {
        for(size_t c = 0; c < BwChanCount; c++) {
            bw_stats_add(&cont, (BwChan)c, (c == BwChanAdv39) ? -55 : -95);
        }
    }
    cont.duration_ms = 1000;
    BwSweep cs;
    bw_stats_finalize(&cont, &cs);
    CHECK(
        cs.chan[BwChanAdv39].busy_ppt == 1000,
        "an unbroken carrier must read as busy all the time, got %u",
        cs.chan[BwChanAdv39].busy_ppt);

    /* Too few samples is not a quiet room, it is no data. */
    BwSweepAccum thin;
    bw_stats_reset(&thin);
    for(uint32_t i = 0; i < BW_MIN_SAMPLES_PER_CHAN - 1; i++) {
        for(size_t c = 0; c < BwChanCount; c++) bw_stats_add(&thin, (BwChan)c, -90);
    }
    BwSweep ts;
    bw_stats_finalize(&thin, &ts);
    CHECK(!ts.valid, "a sweep with too few samples must not be valid");
    for(size_t c = 0; c < BwChanCount; c++) {
        CHECK(!ts.chan[c].valid, "channel %zu should be invalid with too few samples", c);
    }
}

/* -------------------------------------------------------------- demo scenes */

static void score_scene(BwDemoScene scene, uint32_t seed, int sweeps, BwScore* out) {
    BwWatch w;
    memset(&w, 0, sizeof(w));
    for(int i = 0; i < sweeps; i++) {
        BwSweepAccum acc;
        BwSweep s;
        bw_demo_fill(scene, seed + (uint32_t)i * 7919u, 600, &acc);
        bw_stats_finalize(&acc, &s);
        bw_watch_feed(&w, &s, out);
    }
}

static void test_demo_verdicts(void) {
    section("demo scenes reach the verdicts they are supposed to");

    struct {
        BwDemoScene scene;
        BwVerdict want;
    } expect[] = {
        {BwDemoQuietRoom, BwVerdictClear},
        {BwDemoBusyOffice, BwVerdictBusy},
        {BwDemoWifiHeavy, BwVerdictBusy},
        {BwDemoMicrowave, BwVerdictBusy},
        {BwDemoSpamAcrossRoom, BwVerdictSpam},
        {BwDemoSpamInPocket, BwVerdictSpam},
        {BwDemoBeaconFarm, BwVerdictSpam},
    };

    for(size_t i = 0; i < sizeof(expect) / sizeof(expect[0]); i++) {
        /* Every scene has to land in the same band from any seed. A verdict
         * that depends on which second you pressed the button is not a
         * verdict. */
        for(uint32_t seed = 1; seed <= 40; seed++) {
            BwScore sc;
            score_scene(expect[i].scene, seed * 104729u, 10, &sc);
            CHECK(
                sc.verdict == expect[i].want,
                "%s (seed %u): got %s (%u), expected %s",
                bw_demo_name(expect[i].scene),
                seed,
                bw_verdict_name(sc.verdict),
                sc.score,
                bw_verdict_name(expect[i].want));
        }
    }

    /* Heavy Wi-Fi must be *called* heavy Wi-Fi, not just scored low. */
    BwScore wifi;
    score_scene(BwDemoWifiHeavy, 5, 10, &wifi);
    CHECK(
        wifi.interference == BwInterferenceHeavy,
        "heavy Wi-Fi should report heavy interference, got %s",
        bw_interference_name(wifi.interference));
    CHECK(
        (wifi.caps_applied & (1u << BwCapWifiDominant)) != 0,
        "heavy Wi-Fi should trip the Wi-Fi dominance cap");

    BwScore quiet;
    score_scene(BwDemoQuietRoom, 5, 10, &quiet);
    CHECK(
        quiet.interference == BwInterferenceNone,
        "a quiet room should report low interference, got %s",
        bw_interference_name(quiet.interference));

    /* The microwave is the interesting negative: it is genuinely loud and
     * genuinely busy, and it must never look like advertising, because it
     * cannot touch 2402 MHz. */
    BwScore oven;
    score_scene(BwDemoMicrowave, 5, 10, &oven);
    CHECK(
        oven.sig[BwSigTriBalance].points == 0,
        "a microwave must not produce the tri-channel signature (got %u)",
        oven.sig[BwSigTriBalance].points);
    CHECK(
        oven.family_points[BwFamilyShape] == 0,
        "a microwave must produce no SHAPE evidence at all (got %u)",
        oven.family_points[BwFamilyShape]);

    /* A spam board in a place with a busy access point: the tri-channel
     * signature still fires, but Wi-Fi reaches those frequencies too, so the
     * most Bulwark may say is SUSPECT. */
    BwScoreInput mixed;
    memset(&mixed, 0, sizeof(mixed));
    mixed.sweep.valid = true;
    mixed.sweep.adv_busy_ppt = 300;
    mixed.sweep.adv_busy_min_ppt = 280;
    mixed.sweep.adv_busy_max_ppt = 320;
    mixed.sweep.adv_hot_ppt = 200;
    mixed.sweep.ref_busy_ppt = 400;
    mixed.sweep.adv_peak_dbm = -45;
    mixed.sweep.adv_swing_db = 45;
    mixed.streak = 10;
    mixed.sweeps_seen = 10;
    mixed.baseline.taken = true;
    BwScore mixed_score;
    bw_score_eval(&mixed, &mixed_score);
    CHECK(
        (mixed_score.caps_applied & (1u << BwCapWifiComparable)) != 0,
        "comparable Wi-Fi should trip the comparability cap, not the dominance cap");
    CHECK(
        mixed_score.verdict == BwVerdictSuspect,
        "spam under comparable Wi-Fi should read SUSPECT, got %s (%u)",
        bw_verdict_name(mixed_score.verdict),
        mixed_score.score);

    /* And the honest failure, stated out loud in the demo blurb. */
    BwScore farm;
    score_scene(BwDemoBeaconFarm, 5, 10, &farm);
    CHECK(
        farm.verdict == BwVerdictSpam,
        "the beacon farm is a known false positive and the app says so");
    CHECK(
        farm.score <= BW_SCORE_CEILING,
        "even the beacon farm must respect the ceiling, got %u",
        farm.score);
}

static void test_demo_metadata(void) {
    section("demo metadata");
    for(size_t i = 0; i < BwDemoCount; i++) {
        const char* n = bw_demo_name((BwDemoScene)i);
        const char* b = bw_demo_blurb((BwDemoScene)i);
        CHECK(n && n[0], "scene %zu needs a name", i);
        CHECK(b && b[0], "scene %zu needs a blurb", i);
        CHECK(strlen(n) <= 18, "scene name '%s' is too long for the menu", n);
        CHECK(strlen(b) <= 50, "scene blurb '%s' is too long for the screen", b);
    }
    CHECK(bw_demo_name((BwDemoScene)BwDemoCount)[0] != '\0', "out of range name must be safe");
}

/* ------------------------------------------------------- the promises made */

static void random_input(BwScoreInput* in) {
    memset(in, 0, sizeof(*in));
    in->sweep.valid = true;

    uint16_t lo = (uint16_t)rnd_range(0, 500);
    uint16_t hi = (uint16_t)rnd_range(lo, 1000);
    in->sweep.adv_busy_min_ppt = lo;
    in->sweep.adv_busy_max_ppt = hi;
    in->sweep.adv_busy_ppt = (uint16_t)((lo + hi) / 2);
    in->sweep.adv_hot_ppt = (uint16_t)rnd_range(0, in->sweep.adv_busy_ppt + 1);
    in->sweep.ref_busy_ppt = (uint16_t)rnd_range(0, 1000);
    in->sweep.adv_peak_dbm = (int8_t)(-(int)rnd_range(20, 110));
    in->sweep.adv_swing_db = (uint8_t)rnd_range(0, 70);
    in->sweep.band_floor_dbm = (int8_t)(-(int)rnd_range(70, 110));
    in->streak = (uint16_t)rnd_range(0, 20);
    in->sweeps_seen = (uint16_t)rnd_range(0, 50);
    if(rnd() & 1) {
        in->baseline.taken = true;
        in->baseline.adv_busy_ppt = (uint16_t)rnd_range(0, 600);
        in->baseline.ref_busy_ppt = (uint16_t)rnd_range(0, 600);
    }
}

static void test_invariants(void) {
    section("the promises the README makes");

    for(int trial = 0; trial < 200000; trial++) {
        BwScoreInput in;
        BwScore sc;
        random_input(&in);
        bw_score_eval(&in, &sc);

        /* Bulwark cannot read a packet, so it never reaches certainty. */
        CHECK(sc.score <= BW_SCORE_CEILING, "score %u exceeds the ceiling", sc.score);
        CHECK(sc.raw_score <= 100, "raw score %u exceeds 100", sc.raw_score);
        CHECK(sc.score <= sc.raw_score, "a cap raised the score: %u > %u", sc.score, sc.raw_score);

        /* No family may outgrow its own ceiling, so no single kind of
         * evidence can carry a verdict on its own. */
        uint32_t sum = 0;
        for(size_t f = 0; f < BwFamilyCount; f++) {
            CHECK(
                sc.family_points[f] <= sc.family_max[f],
                "family %s scored %u of %u",
                bw_family_name((BwFamily)f),
                sc.family_points[f],
                sc.family_max[f]);
            sum += sc.family_points[f];
        }
        CHECK(sum == sc.raw_score, "family points %u do not add to raw score %u", sum, sc.raw_score);

        /* SPAM LIKELY requires the tri-channel signature specifically. */
        if(sc.verdict == BwVerdictSpam) {
            CHECK(
                sc.sig[BwSigTriBalance].points == 16,
                "SPAM LIKELY without the tri-channel signature (score %u)",
                sc.score);
            CHECK(in.sweeps_seen >= 2, "SPAM LIKELY on a single sweep (score %u)", sc.score);
            CHECK(
                sc.family_points[BwFamilyOccupancy] > 0,
                "SPAM LIKELY on an idle band (score %u)",
                sc.score);
            CHECK(
                in.sweep.ref_busy_ppt < BW_GATE_PPT ||
                    in.sweep.ref_busy_ppt < in.sweep.adv_busy_ppt,
                "SPAM LIKELY while Wi-Fi was busier than advertising");
        }

        /* Each cap does what it says on the tin. */
        if(sc.family_points[BwFamilyOccupancy] == 0) CHECK(sc.score <= 20, "quiet band cap");
        if(sc.family_points[BwFamilyShape] == 0) CHECK(sc.score <= 44, "no-shape cap");
        if(sc.sig[BwSigTriBalance].points < 16) CHECK(sc.score <= 69, "no-balance cap");
        if(in.sweeps_seen < 2) CHECK(sc.score <= 64, "first-look cap");
        if(!in.baseline.taken) CHECK(sc.score <= 88, "no-baseline cap");
        if(in.sweep.ref_busy_ppt >= BW_GATE_PPT) {
            if(in.sweep.ref_busy_ppt >= (uint32_t)in.sweep.adv_busy_ppt * 2u) {
                CHECK(sc.score <= 30, "Wi-Fi dominance cap");
            } else if(in.sweep.ref_busy_ppt >= in.sweep.adv_busy_ppt) {
                CHECK(sc.score <= 65, "Wi-Fi comparability cap");
            }
        }

        /* The score band and the word on the screen never disagree. */
        BwVerdict want = sc.score <= 24  ? BwVerdictClear :
                         sc.score <= 44  ? BwVerdictBusy :
                         sc.score <= 69  ? BwVerdictSuspect :
                                           BwVerdictSpam;
        CHECK(sc.verdict == want, "score %u does not match verdict %s", sc.score, bw_verdict_name(sc.verdict));

        /* A cap is only blamed when it actually held the score down. */
        if(sc.binding_cap != BwCapNone) {
            CHECK(sc.score < sc.raw_score, "cap %s blamed but nothing was lost", bw_cap_name(sc.binding_cap));
        } else {
            CHECK(sc.score == sc.raw_score, "no cap blamed but %u was lost", sc.raw_score - sc.score);
        }

        /* Every signal must be filled in, every time, or the detail screen
         * shows a blank row. */
        for(size_t i = 0; i < BwSigCount; i++) {
            CHECK(sc.sig[i].id == (BwSignal)i, "signal %zu is out of order", i);
            CHECK(sc.sig[i].name && sc.sig[i].name[0], "signal %zu has no name", i);
            CHECK(sc.sig[i].detail && sc.sig[i].detail[0], "signal %zu has no detail", i);
            CHECK(
                sc.sig[i].points <= sc.sig[i].max_points,
                "signal %s scored %u of %u",
                sc.sig[i].name,
                sc.sig[i].points,
                sc.sig[i].max_points);
        }
    }
}

static void test_no_data(void) {
    section("no data is not a clean bill of health");

    BwScoreInput in;
    BwScore sc;
    memset(&in, 0, sizeof(in));
    in.sweep.valid = false;
    bw_score_eval(&in, &sc);

    CHECK(!sc.have_data, "an invalid sweep must not claim to have data");
    CHECK(sc.verdict == BwVerdictNoData, "an invalid sweep must not produce a verdict");
    CHECK(sc.score == 0, "an invalid sweep must not produce a score");
    for(size_t i = 0; i < BwSigCount; i++) {
        CHECK(sc.sig[i].name && sc.sig[i].name[0], "signal %zu must still be listed", i);
        CHECK(sc.sig[i].points == 0, "signal %zu must score nothing without data", i);
    }
}

static void test_monotonic(void) {
    section("more evidence never means a lower score");

    for(int trial = 0; trial < 4000; trial++) {
        BwScoreInput base;
        random_input(&base);

        /* Busier advertising channels, everything else held still. */
        uint8_t prev = 0;
        for(uint32_t busy = 0; busy <= 1000; busy += 25) {
            BwScoreInput in = base;
            in.sweep.adv_busy_ppt = (uint16_t)busy;
            in.sweep.adv_busy_min_ppt = (uint16_t)(busy * 4u / 5u);
            in.sweep.adv_busy_max_ppt = (uint16_t)busy;
            BwScore sc;
            bw_score_eval(&in, &sc);
            CHECK(
                sc.score >= prev,
                "score fell from %u to %u as occupancy rose to %u ppt",
                prev,
                sc.score,
                busy);
            prev = sc.score;
        }

        /* Longer streaks, everything else held still. */
        prev = 0;
        for(uint32_t streak = 0; streak <= 16; streak++) {
            BwScoreInput in = base;
            in.streak = (uint16_t)streak;
            in.sweeps_seen = 50;
            BwScore sc;
            bw_score_eval(&in, &sc);
            CHECK(sc.score >= prev, "score fell from %u to %u as the streak grew", prev, sc.score);
            prev = sc.score;
        }

        /* A louder signal, everything else held still. */
        prev = 0;
        for(uint32_t swing = 0; swing <= 60; swing += 5) {
            BwScoreInput in = base;
            in.sweep.adv_swing_db = (uint8_t)swing;
            BwScore sc;
            bw_score_eval(&in, &sc);
            CHECK(sc.score >= prev, "score fell from %u to %u as the swing grew", prev, sc.score);
            prev = sc.score;
        }
    }
}

static void test_watch_state(void) {
    section("rolling state");

    BwWatch w;
    memset(&w, 0, sizeof(w));
    BwScore sc;

    /* A busy sweep, then a quiet one, must break the streak. */
    BwSweepAccum acc;
    BwSweep busy, calm;
    bw_demo_fill(BwDemoSpamInPocket, 11, 400, &acc);
    bw_stats_finalize(&acc, &busy);
    bw_demo_fill(BwDemoQuietRoom, 11, 400, &acc);
    bw_stats_finalize(&acc, &calm);

    for(int i = 0; i < 5; i++) bw_watch_feed(&w, &busy, &sc);
    CHECK(w.streak == 5, "streak should be 5, got %u", w.streak);
    CHECK(sc.verdict == BwVerdictSpam, "five sweeps of pocket spam should be SPAM LIKELY");
    uint16_t peak = w.peak_score;

    bw_watch_feed(&w, &calm, &sc);
    CHECK(w.streak == 0, "a quiet sweep must break the streak, got %u", w.streak);
    CHECK(sc.verdict == BwVerdictClear, "a quiet sweep should read CLEAR");
    CHECK(w.peak_score == peak, "the peak score must survive a quiet sweep");

    /* An invalid sweep must not advance anything. */
    BwSweep bad;
    memset(&bad, 0, sizeof(bad));
    uint16_t seen = w.sweeps_seen;
    bw_watch_feed(&w, &bad, &sc);
    CHECK(w.sweeps_seen == seen, "an invalid sweep must not count as a sweep");

    /* A baseline taken in a spammed room is the user's mistake, but it must
     * not be silently ignored: the cap fires and says why. */
    BwWatch b;
    memset(&b, 0, sizeof(b));
    bw_watch_set_baseline(&b, &busy);
    CHECK(b.baseline.taken, "baseline should be stored");
    for(int i = 0; i < 5; i++) bw_watch_feed(&b, &busy, &sc);
    CHECK(
        (sc.caps_applied & (1u << BwCapAtBaseline)) != 0,
        "a sweep matching the baseline must trip the baseline cap");
    CHECK(sc.score <= 30, "a sweep matching the baseline must be capped at 30, got %u", sc.score);

    /* An invalid sweep must not be storable as a baseline. */
    BwWatch c;
    memset(&c, 0, sizeof(c));
    bw_watch_set_baseline(&c, &bad);
    CHECK(!c.baseline.taken, "an invalid sweep must not become a baseline");

    /* Reset keeps the baseline - it describes the place, not the session. */
    BwWatch d;
    memset(&d, 0, sizeof(d));
    bw_watch_set_baseline(&d, &busy);
    d.streak = 9;
    d.sweeps_seen = 12;
    bw_watch_reset(&d);
    CHECK(d.streak == 0 && d.sweeps_seen == 0, "reset must clear the session");
    CHECK(d.baseline.taken, "reset must keep the baseline");

    /* A baseline lifts the confidence ceiling from 88 to 94, and that is the
     * only thing it can do to the top of the range. */
    BwScoreInput in;
    memset(&in, 0, sizeof(in));
    in.sweep.valid = true;
    in.sweep.adv_busy_ppt = 900;
    in.sweep.adv_busy_min_ppt = 880;
    in.sweep.adv_busy_max_ppt = 920;
    in.sweep.adv_hot_ppt = 800;
    in.sweep.ref_busy_ppt = 10;
    in.sweep.adv_peak_dbm = -40;
    in.sweep.adv_swing_db = 55;
    in.streak = 12;
    in.sweeps_seen = 12;
    BwScore without, with;
    bw_score_eval(&in, &without);
    in.baseline.taken = true;
    in.baseline.adv_busy_ppt = 20;
    bw_score_eval(&in, &with);
    CHECK(without.score == 88, "without a baseline the best case is 88, got %u", without.score);
    CHECK(with.score == BW_SCORE_CEILING, "with a baseline the best case is 94, got %u", with.score);
}

static void test_names(void) {
    section("nothing on screen is ever NULL");

    for(int v = 0; v <= BwVerdictCount; v++) {
        CHECK(bw_verdict_name((BwVerdict)v) != NULL, "verdict %d name", v);
        CHECK(bw_verdict_blurb((BwVerdict)v) != NULL, "verdict %d blurb", v);
        CHECK(strlen(bw_verdict_name((BwVerdict)v)) <= 11, "verdict %d name too wide", v);
        CHECK(strlen(bw_verdict_blurb((BwVerdict)v)) <= 30, "verdict %d blurb too wide", v);
    }
    for(int f = 0; f <= BwFamilyCount; f++) {
        CHECK(bw_family_name((BwFamily)f) != NULL, "family %d name", f);
    }
    for(int c = 0; c <= BwCapCount; c++) {
        CHECK(bw_cap_name((BwCap)c) != NULL, "cap %d name", c);
        CHECK(bw_cap_reason((BwCap)c) != NULL, "cap %d reason", c);
        CHECK(strlen(bw_cap_name((BwCap)c)) <= 20, "cap %d name too wide", c);
    }
    for(int i = 0; i <= 3; i++) {
        CHECK(bw_interference_name((BwInterference)i) != NULL, "interference %d name", i);
    }

    /* The signal names sit in a fixed column on the detail screen. */
    BwScoreInput in;
    BwScore sc;
    memset(&in, 0, sizeof(in));
    in.sweep.valid = true;
    bw_score_eval(&in, &sc);
    for(size_t i = 0; i < BwSigCount; i++) {
        CHECK(strlen(sc.sig[i].name) <= 18, "signal name '%s' is too wide", sc.sig[i].name);
    }
}

int main(void) {
    printf("Bulwark engine tests\n");

    test_channel_plan();
    test_percentile();
    test_sample_hygiene();
    test_occupancy_maths();
    test_demo_verdicts();
    test_demo_metadata();
    test_no_data();
    test_invariants();
    test_monotonic();
    test_watch_state();
    test_names();

    printf("\n%lu checks, %lu failures\n", checks, failures);
    return failures ? 1 : 0;
}
