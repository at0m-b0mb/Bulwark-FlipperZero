/**
 * Bulwark - turning a stream of RSSI readings into a description of the band.
 *
 * The radio gives us one number at a time: how much energy is arriving right
 * now on whichever channel we are parked on. On its own that number says
 * almost nothing - it is high while somebody transmits and low in the gaps,
 * and both happen thousands of times a second. What matters is the shape of
 * the distribution, so every sample goes into a per-channel histogram and the
 * channel is described afterwards by four numbers taken out of it:
 *
 *   floor   the 20th percentile - what this channel looks like when nothing
 *           is happening on it. Taken per channel, because the noise floor is
 *           not flat across 80 MHz.
 *   busy    the share of samples sitting BW_BUSY_MARGIN_DB above the quietest
 *           floor in the band. This is the important one: it is a *duty
 *           cycle*, and an advertising spammer's duty cycle is unlike
 *           anything a room full of ordinary devices produces.
 *   peak    the loudest thing heard. Proximity, roughly.
 *   p90     the 90th percentile - a peak that cannot be produced by one
 *           unlucky sample.
 *
 * "busy" is measured against the *band* floor (the quietest of the six
 * channel floors) rather than the channel's own floor, deliberately: a
 * transmitter that never stops lifts its own channel's floor, and measuring
 * against that floor would hide it completely. Measuring against the quietest
 * corner of the band instead makes a continuous carrier read as busy ~100% of
 * the time, which is what it is.
 *
 * No furi here. This file is compiled and taken apart by test/.
 */
#pragma once

#include "bw_chan.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Histogram bin i holds the count of samples read as -i dBm. */
#define BW_HIST_BINS 128

/** How far above the band floor a sample has to be to count as "busy". */
#define BW_BUSY_MARGIN_DB 6

/** ... and to count as "hot": something loud, not something across the road. */
#define BW_HOT_MARGIN_DB 20

/** Percentile taken as a channel's own quiet level. */
#define BW_FLOOR_PCT 20u

/** Nothing is described from fewer samples than this - it would be noise. */
#define BW_MIN_SAMPLES_PER_CHAN 24u

/** Per-channel accumulator. Fed one sample at a time by the radio worker. */
typedef struct {
    uint16_t hist[BW_HIST_BINS];
    uint32_t n;
    int8_t peak_dbm;
} BwChanAccum;

/** One sweep's worth of raw sampling, across every channel in the plan. */
typedef struct {
    BwChanAccum acc[BwChanCount];
    uint32_t duration_ms;
} BwSweepAccum;

/** What a finished sweep says about one channel. */
typedef struct {
    bool valid; /**< enough samples to describe */
    uint32_t n;
    int8_t floor_dbm; /**< 20th percentile of this channel  */
    int8_t p90_dbm;
    int8_t peak_dbm;
    uint16_t busy_ppt; /**< parts per thousand, vs band floor + BUSY margin */
    uint16_t hot_ppt; /**< parts per thousand, vs band floor + HOT margin  */
} BwChanResult;

/** What a finished sweep says about the band. This is what gets scored. */
typedef struct {
    bool valid; /**< every channel had enough samples */
    BwChanResult chan[BwChanCount];

    int8_t band_floor_dbm; /**< quietest channel floor - the reference */

    uint16_t adv_busy_ppt; /**< mean busy over the three adv channels */
    uint16_t adv_busy_min_ppt; /**< the least busy of the three           */
    uint16_t adv_busy_max_ppt; /**< the most busy of the three            */
    uint16_t adv_hot_ppt; /**< mean hot over the three adv channels  */
    uint16_t ref_busy_ppt; /**< mean busy over the three Wi-Fi centres */

    int8_t adv_peak_dbm; /**< loudest sample on any adv channel */
    uint8_t adv_swing_db; /**< that peak, above the band floor   */

    uint32_t samples; /**< total samples, all channels */
    uint32_t duration_ms;
    uint16_t rate_hz; /**< achieved sample rate. Reported, not assumed. */
} BwSweep;

/** Throw away everything and start a new sweep. */
void bw_stats_reset(BwSweepAccum* accum);

/**
 * Record one RSSI reading.
 *
 * @param dbm  RSSI in dBm. Values outside -127..-1 are clamped; the radio
 *             reports exactly 0 when the read failed, and that is dropped.
 */
void bw_stats_add(BwSweepAccum* accum, BwChan chan, int dbm);

/** Reduce a sweep's accumulators to the description that gets scored. */
void bw_stats_finalize(const BwSweepAccum* accum, BwSweep* out);

/**
 * The dBm at or below which @p pct percent of a channel's samples fall.
 * Exposed for the tests, which check it against a brute-force sort.
 */
int8_t bw_stats_percentile(const BwChanAccum* acc, uint32_t pct);

#ifdef __cplusplus
}
#endif
