/**
 * Bulwark - synthetic bands.
 *
 * Six places, described the way the radio would hear them: a per-channel
 * noise floor, a duty cycle, and how loud the arrivals are. Feeding these
 * through the real accumulator and the real scorer is how the tests check
 * the verdicts, how the README mockups are drawn, and how demo mode works
 * without hardware. There is no second implementation of anything.
 *
 * One of the six is a shop full of Bluetooth beacons, and Bulwark calls it
 * SPAM LIKELY. That is not a bug in the demo data - it is the honest limit
 * of measuring a band you cannot decode, and demo mode says so on the screen.
 *
 * No furi here.
 */
#pragma once

#include "bw_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BwDemoQuietRoom = 0,
    BwDemoBusyOffice,
    BwDemoWifiHeavy,
    BwDemoMicrowave,
    BwDemoSpamAcrossRoom,
    BwDemoSpamInPocket,
    BwDemoBeaconFarm,
    BwDemoCount,
} BwDemoScene;

/** Short name for the menu. */
const char* bw_demo_name(BwDemoScene scene);

/** One line about what the place is. */
const char* bw_demo_blurb(BwDemoScene scene);

/**
 * Fill an accumulator as if the radio had listened to @p scene.
 *
 * @param seed  any value; the same seed gives the same band, every time.
 * @param samples_per_chan  how many RSSI reads per channel to simulate.
 */
void bw_demo_fill(BwDemoScene scene, uint32_t seed, uint32_t samples_per_chan, BwSweepAccum* accum);

#ifdef __cplusplus
}
#endif
