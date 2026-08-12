/**
 * Bulwark - deciding what the band is doing, and refusing to overclaim it.
 *
 * A Bluetooth LE advertising-spam attack - the popup floods that make an
 * iPhone ask about AirPods twenty times a minute, or a Windows laptop offer
 * to pair with a mouse that does not exist - is, at the radio layer, one
 * device hammering the three advertising channels far harder than any honest
 * device does, from close range, without stopping. That is the whole thing
 * Bulwark can see, and it is quite a lot.
 *
 * Four independent families of evidence:
 *
 *   OCCUPANCY   how much of the time the advertising channels are busy.
 *   SHAPE       whether the busyness is *shaped like Bluetooth* - all three
 *               advertising channels equally, and not the Wi-Fi centres in
 *               between them.
 *   LEVEL       how far above the band floor the loudest arrivals are, which
 *               is proximity plus transmit power.
 *   PERSISTENCE whether it is still there sweep after sweep.
 *
 * The caps are the design, not a safety net bolted on afterwards:
 *
 *   - A quiet band cannot be an attack, whatever its shape, so if OCCUPANCY
 *     scores nothing the total stops at 20.
 *   - A busy band that is not Bluetooth-shaped stops at 44, because a
 *     microwave oven, a video sender and a crowded Wi-Fi channel all make the
 *     band busy and none of them is advertising at your phone.
 *   - SPAM LIKELY requires the tri-channel balance signal *specifically*.
 *     Every other signal here has an innocent explanation on its own; nothing
 *     else in 2.4 GHz lights up 2402, 2426 and 2480 MHz in equal measure,
 *     because those three frequencies have nothing in common except that
 *     Bluetooth advertising uses them. Without it the total stops at 69.
 *   - Wi-Fi is the confound that matters, and it gets two ceilings. If the
 *     Wi-Fi centres are merely as busy as the advertising channels, the total
 *     stops at 65 - something is hammering 37, 38 and 39, but the Wi-Fi
 *     around you could be doing it, so SUSPECT is as far as it goes. If they
 *     are twice as busy, the total stops at 30: that is Wi-Fi, and the honest
 *     reading is "I cannot see through this".
 *   - The first sweep stops at 64. One look is not a watch.
 *   - Nothing ever reaches 100. Bulwark hears advertising traffic; it cannot
 *     read a single advertising packet, so it can never prove the packets are
 *     an attack rather than a very busy beacon farm. The ceiling is 94 and it
 *     is on the screen.
 *
 * Nothing here says a place is safe. An attacker three rooms away still
 * reaches a phone in a pocket that Bulwark cannot hear from where it is
 * sitting, and a phone with the popups already turned off is not attacked at
 * all. Bulwark reports what arrived at its own antenna, in the seconds it was
 * listening. That is the claim, and it is the only one.
 *
 * No furi here. test/ compiles this file and takes it apart.
 */
#pragma once

#include "bw_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Families of evidence. Independent by construction. */
typedef enum {
    BwFamilyOccupancy = 0,
    BwFamilyShape,
    BwFamilyLevel,
    BwFamilyPersistence,
    BwFamilyCount,
} BwFamily;

/** Individual signals. Order is the order they are shown in. */
typedef enum {
    BwSigAdvBusy = 0, /**< advertising channels busy at all       */
    BwSigAdvHot, /**< ... and loudly, not just detectably     */
    BwSigTriBalance, /**< 37, 38 and 39 hit in equal measure      */
    BwSigGuardContrast, /**< adv channels busier than Wi-Fi centres  */
    BwSigSwing, /**< peak well above the band floor         */
    BwSigCloseRange, /**< peak loud in absolute terms            */
    BwSigPersist, /**< still there sweep after sweep          */
    BwSigCount,
} BwSignal;

/** Caps. One of them is usually what the score actually is. */
typedef enum {
    BwCapNone = 0,
    BwCapQuietBand, /**< OCCUPANCY scored nothing            -> 20 */
    BwCapWifiDominant, /**< Wi-Fi twice as busy as advertising  -> 30 */
    BwCapAtBaseline, /**< no busier than this room always is  -> 30 */
    BwCapNoShape, /**< SHAPE scored nothing                -> 44 */
    BwCapFirstLook, /**< only one sweep so far               -> 64 */
    BwCapWifiComparable, /**< Wi-Fi as busy as advertising        -> 65 */
    BwCapNoBalance, /**< the tri-channel signature is absent -> 69 */
    BwCapNoBaseline, /**< no quiet reading to compare against -> 88 */
    BwCapCeiling, /**< cannot read the packets             -> 94 */
    BwCapCount,
} BwCap;

/** Verdict bands. */
typedef enum {
    BwVerdictNoData = 0,
    BwVerdictClear,
    BwVerdictBusy,
    BwVerdictSuspect,
    BwVerdictSpam,
    BwVerdictCount,
} BwVerdict;

/** Reported separately from the score, the way it should be. */
typedef enum {
    BwInterferenceNone = 0,
    BwInterferenceModerate,
    BwInterferenceHeavy,
} BwInterference;

/** One signal's outcome. */
typedef struct {
    BwSignal id;
    BwFamily family;
    uint8_t points;
    uint8_t max_points;
    const char* name; /**< <= 18 chars, fits the list */
    const char* detail; /**< one line of why. Static strings only. */
} BwSigResult;

/** What a quiet room looked like, if the user ever showed Bulwark one. */
typedef struct {
    bool taken;
    uint16_t adv_busy_ppt;
    uint16_t ref_busy_ppt;
    int8_t band_floor_dbm;
} BwBaseline;

/** Everything scoring is allowed to look at. */
typedef struct {
    BwSweep sweep;
    uint16_t streak; /**< consecutive sweeps over the gate, this one included */
    uint16_t sweeps_seen;
    BwBaseline baseline;
} BwScoreInput;

/** The decision. */
typedef struct {
    bool have_data;
    uint8_t raw_score; /**< before caps, 0..100 */
    uint8_t score; /**< after caps,  0..94  */
    BwVerdict verdict;
    BwInterference interference;
    uint8_t family_points[BwFamilyCount];
    uint8_t family_max[BwFamilyCount];
    BwSigResult sig[BwSigCount];
    uint32_t caps_applied; /**< bit (1u << BwCap) per cap that was in force */
    BwCap binding_cap; /**< the lowest ceiling - the one that actually bit */
} BwScore;

/** Rolling state across sweeps. Furi-free; the app owns one of these. */
typedef struct {
    uint16_t sweeps_seen;
    uint16_t streak;
    uint16_t peak_score;
    BwBaseline baseline;
} BwWatch;

/** Occupancy at or below this is "the advertising channels are idle". */
#define BW_GATE_PPT 30u

/** How much busier than the stored baseline counts as something new. */
#define BW_BASELINE_MARGIN_PPT 40u

/** The ceiling. Bulwark cannot read a packet, so it never reaches 100. */
#define BW_SCORE_CEILING 94u

/** Pure. Same input, same output, no state. This is what the tests hammer. */
void bw_score_eval(const BwScoreInput* in, BwScore* out);

/** Fold a finished sweep into the rolling state, then score it. */
void bw_watch_reset(BwWatch* watch);
void bw_watch_feed(BwWatch* watch, const BwSweep* sweep, BwScore* out);

/** Store a sweep as the "this room is quiet" reference. */
void bw_watch_set_baseline(BwWatch* watch, const BwSweep* sweep);

/* Names for the screen. Never NULL. */
const char* bw_verdict_name(BwVerdict v);
const char* bw_verdict_blurb(BwVerdict v);
const char* bw_family_name(BwFamily f);
const char* bw_cap_name(BwCap c);
const char* bw_cap_reason(BwCap c);
const char* bw_interference_name(BwInterference i);

#ifdef __cplusplus
}
#endif
