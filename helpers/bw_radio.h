/**
 * Bulwark - the listening half.
 *
 * The Flipper's own Bluetooth radio has a mode, meant for factory testing,
 * in which it stops being a Bluetooth device and becomes a plain receiver
 * parked on one 2 MHz channel, reporting how much energy is arriving. That is
 * the only way an unmodified Flipper can look at the advertising channels at
 * all, and it is what Bulwark uses.
 *
 * The price is honest and worth stating: in this mode the radio is not a
 * Bluetooth device, so it cannot decode anything. Bulwark hears advertising
 * traffic. It never sees an address, a name, a payload or a manufacturer ID,
 * and it never will without extra hardware. Everything it concludes, it
 * concludes from *when* and *how loudly* energy arrives on six frequencies.
 *
 * The worker walks the channel plan in rounds - a short dwell on each of the
 * six channels, over and over for the length of a sweep - so that a lorry
 * driving past does not land entirely on one channel and invent a signature.
 * While it runs, the Flipper's normal Bluetooth is off; it is put back the
 * way it was found on the way out.
 */
#pragma once

#include "bw_stats.h"
#include "bw_demo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BwRadio BwRadio;

typedef enum {
    BwRadioIdle = 0,
    BwRadioRunning,
    /** The radio stack on this Flipper has no RF test mode. Nothing to do. */
    BwRadioUnsupported,
} BwRadioState;

/** How long one sweep listens for, before it is finalised and scored. */
typedef enum {
    BwSweepFast = 0, /**<  3 s - twitchy, for walking around  */
    BwSweepNormal, /**<  6 s - the default                  */
    BwSweepCareful, /**< 12 s - for standing still and being sure */
    BwSweepLenCount,
} BwSweepLen;

uint32_t bw_sweep_len_ms(BwSweepLen len);
const char* bw_sweep_len_name(BwSweepLen len);

/** A cheap snapshot for the live screen. Safe to read from the GUI thread. */
typedef struct {
    uint16_t busy_ppt[BwChanCount]; /**< this sweep so far, provisional */
    int8_t peak_dbm[BwChanCount];
    uint8_t chan_now; /**< which channel the radio is parked on */
    uint8_t progress_pct; /**< how far through this sweep         */
    uint16_t rate_hz; /**< samples per second, measured        */
    uint32_t sweeps_done;
} BwRadioLive;

/**
 * Called from the worker thread when a sweep is finished.
 *
 * Do nothing here but wake the GUI - post a custom event and return. The
 * finished sweep itself is fetched with bw_radio_take_sweep().
 */
typedef void (*BwRadioSweepCallback)(void* ctx);

BwRadio* bw_radio_alloc(void);
void bw_radio_free(BwRadio* radio);

void bw_radio_set_sweep_len(BwRadio* radio, BwSweepLen len);

/**
 * Run against a synthetic band instead of the radio.
 *
 * Demo mode is the same accumulator, the same statistics and the same scorer
 * fed from bw_demo instead of the antenna. There is no second code path.
 */
void bw_radio_set_demo(BwRadio* radio, bool enabled, BwDemoScene scene);
bool bw_radio_is_demo(const BwRadio* radio);

/** Start the worker. Returns false if the radio has no test mode. */
bool bw_radio_start(BwRadio* radio, BwRadioSweepCallback cb, void* ctx);

/** Stop the worker and give the Flipper its Bluetooth back. */
void bw_radio_stop(BwRadio* radio);

BwRadioState bw_radio_state(const BwRadio* radio);

/** Copy the live snapshot. Cheap; call it from the view timer. */
void bw_radio_get_live(BwRadio* radio, BwRadioLive* out);

/** Copy the most recently finished sweep. True if there was a new one. */
bool bw_radio_take_sweep(BwRadio* radio, BwSweep* out);

#ifdef __cplusplus
}
#endif
