#include "bw_demo.h"

#include <stddef.h>

typedef struct {
    int8_t floor_dbm; /**< what the channel sits at when idle    */
    uint16_t duty_ppt; /**< share of the time something is on it  */
    int8_t burst_dbm; /**< how loud that something arrives       */
} BwDemoChan;

/* Rows are scenes, columns are the six channels in BwChan order:
 *   adv37   W1    adv38   W6    W11   adv39
 *   2402   2412   2426   2436  2462   2480  MHz                            */
static const BwDemoChan demo[BwDemoCount][BwChanCount] = {
    /* A bedroom at night. A watch, a pair of earbuds, a TV remote. Wi-Fi
     * idling on channel 6 because everything is asleep. */
    [BwDemoQuietRoom] =
        {
            {-96, 12, -78},
            {-96, 14, -74},
            {-95, 10, -80},
            {-95, 22, -70},
            {-96, 12, -76},
            {-96, 9, -80},
        },
    /* An open-plan office. Thirty laptops, forty phones, three access
     * points working hard. The advertising channels are genuinely busy -
     * and that is normal, which is exactly the point. */
    [BwDemoBusyOffice] =
        {
            {-94, 72, -70},
            {-92, 300, -56},
            {-94, 58, -72},
            {-91, 340, -54},
            {-93, 260, -58},
            {-94, 66, -71},
        },
    /* Someone streaming 4K over Wi-Fi from two metres away. The band looks
     * catastrophic and not one packet of it is Bluetooth. */
    [BwDemoWifiHeavy] =
        {
            {-92, 90, -66},
            {-84, 640, -42},
            {-90, 70, -68},
            {-86, 580, -45},
            {-88, 520, -47},
            {-92, 60, -70},
        },
    /* A microwave oven, one wall away, on the reheat cycle. A magnetron
     * smears energy across the top half of the band and touches nothing at
     * 2402 MHz at all. */
    [BwDemoMicrowave] =
        {
            {-96, 10, -80},
            {-95, 30, -72},
            {-92, 120, -60},
            {-80, 430, -44},
            {-76, 520, -38},
            {-84, 300, -50},
        },
    /* A spam board running in a bag on the far side of a cafe. All three
     * advertising channels, in step, at the range where the popups still
     * reach the tables in between. */
    [BwDemoSpamAcrossRoom] =
        {
            {-96, 215, -66},
            {-95, 40, -70},
            {-95, 230, -64},
            {-94, 45, -68},
            {-95, 38, -70},
            {-96, 200, -67},
        },
    /* The same board, in the pocket of the person standing next to you.
     * This is what the attack looks like when it is working. */
    [BwDemoSpamInPocket] =
        {
            {-96, 385, -40},
            {-95, 30, -72},
            {-96, 402, -38},
            {-95, 34, -70},
            {-95, 28, -73},
            {-96, 371, -42},
        },
    /* A shop that has bought into indoor navigation: sixty beacons in the
     * ceiling, all advertising as fast as their batteries allow. Nobody is
     * attacking anybody. Bulwark still calls it SPAM LIKELY, because from
     * the outside of the packets the two are the same thing. */
    [BwDemoBeaconFarm] =
        {
            {-95, 265, -58},
            {-93, 120, -62},
            {-95, 280, -56},
            {-93, 140, -60},
            {-94, 110, -63},
            {-95, 255, -59},
        },
};

static const char* const demo_names[BwDemoCount] = {
    [BwDemoQuietRoom] = "Quiet room",
    [BwDemoBusyOffice] = "Busy office",
    [BwDemoWifiHeavy] = "Heavy Wi-Fi",
    [BwDemoMicrowave] = "Microwave oven",
    [BwDemoSpamAcrossRoom] = "Spam across room",
    [BwDemoSpamInPocket] = "Spam in pocket",
    [BwDemoBeaconFarm] = "Beacon farm",
};

static const char* const demo_blurbs[BwDemoCount] = {
    [BwDemoQuietRoom] = "A bedroom at night. A watch and some earbuds.",
    [BwDemoBusyOffice] = "Open plan. Busy advertising channels are normal.",
    [BwDemoWifiHeavy] = "4K over Wi-Fi at two metres. None of it is BLE.",
    [BwDemoMicrowave] = "A magnetron one wall away. Nothing at 2402 MHz.",
    [BwDemoSpamAcrossRoom] = "A spam board in a bag on the far side.",
    [BwDemoSpamInPocket] = "The same board, one metre away, working.",
    [BwDemoBeaconFarm] = "Sixty shop beacons. Bulwark cannot tell. Honest.",
};

const char* bw_demo_name(BwDemoScene scene) {
    if(scene >= BwDemoCount) return "?";
    return demo_names[scene];
}

const char* bw_demo_blurb(BwDemoScene scene) {
    if(scene >= BwDemoCount) return "";
    return demo_blurbs[scene];
}

static uint32_t xorshift(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

void bw_demo_fill(
    BwDemoScene scene,
    uint32_t seed,
    uint32_t samples_per_chan,
    BwSweepAccum* accum) {
    if(scene >= BwDemoCount) scene = BwDemoQuietRoom;
    uint32_t state = seed ? seed : 0xB01D2402u;

    bw_stats_reset(accum);

    for(uint32_t i = 0; i < samples_per_chan; i++) {
        for(size_t c = 0; c < BwChanCount; c++) {
            const BwDemoChan* d = &demo[scene][c];
            uint32_t r = xorshift(&state);
            int dbm;
            if((r % 1000u) < d->duty_ppt) {
                /* Arrivals vary: distance, antenna angle, packet length. */
                dbm = d->burst_dbm + (int)((xorshift(&state) % 9u)) - 4;
            } else {
                dbm = d->floor_dbm + (int)((xorshift(&state) % 5u)) - 2;
            }
            bw_stats_add(accum, (BwChan)c, dbm);
        }
    }

    /* The real worker measures this; the scenes quote the rate a Flipper
     * actually achieves so the sample-rate line on screen is not a lie. */
    accum->duration_ms = (samples_per_chan * BwChanCount * 1000u) / 420u;
    if(accum->duration_ms == 0) accum->duration_ms = 1;
}
