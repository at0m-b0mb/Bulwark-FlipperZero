#include "bw_radio.h"

#include <furi.h>
#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>

#define TAG "Bulwark"

/** How long the radio stays on one channel before hopping. */
#define BW_DWELL_MS 40u

/** The radio needs a moment after a channel change before the reading means
 *  anything. Measured conservatively; it costs 5% of the dwell. */
#define BW_SETTLE_MS 2u

/** Bootstrap floor for the live bars, before a sweep has finished and told
 *  us what the band floor really is. */
#define BW_LIVE_FLOOR_DBM (-95)

/** Demo mode paces itself to roughly what the hardware achieves, so the
 *  sample-rate line on screen says the same sort of number either way. */
#define BW_DEMO_RATE_HZ 420u

typedef enum {
    BwWorkerFlagStop = (1 << 0),
} BwWorkerFlag;

struct BwRadio {
    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;

    BwSweepLen sweep_len;
    bool demo;
    BwDemoScene demo_scene;
    uint32_t demo_seed;

    BwRadioState state;

    /* Owned by the worker thread. Both live here rather than on its stack:
     * an accumulator is 1.5 kB and the worker's stack is not. */
    BwSweepAccum accum;
    BwSweepAccum demo_scratch;
    int8_t live_floor_dbm;
    /* Last finished sweep's occupancy, so the bars hold their value while the
     * new sweep is still only a handful of samples deep instead of collapsing
     * to zero six times a second. */
    uint16_t last_busy_ppt[BwChanCount];
    bool have_last;

    /* Shared. Guarded by mutex. */
    BwRadioLive live;
    BwSweep last_sweep;
    bool sweep_pending;

    BwRadioSweepCallback callback;
    void* callback_ctx;

    Bt* bt;
};

uint32_t bw_sweep_len_ms(BwSweepLen len) {
    switch(len) {
    case BwSweepFast:
        return 3000;
    case BwSweepCareful:
        return 12000;
    default:
        return 6000;
    }
}

const char* bw_sweep_len_name(BwSweepLen len) {
    switch(len) {
    case BwSweepFast:
        return "Fast 3s";
    case BwSweepCareful:
        return "Careful 12s";
    default:
        return "Normal 6s";
    }
}

/* ------------------------------------------------------------- live updates */

/** Provisional occupancy for the bars, against last sweep's band floor. */
static void radio_publish_live(BwRadio* radio, uint8_t chan_now, uint8_t progress_pct) {
    BwRadioLive live;
    memset(&live, 0, sizeof(live));
    live.chan_now = chan_now;
    live.progress_pct = progress_pct;

    uint32_t total = 0;
    const int thresh = radio->live_floor_dbm + BW_BUSY_MARGIN_DB;
    for(size_t c = 0; c < BwChanCount; c++) {
        const BwChanAccum* acc = &radio->accum.acc[c];
        total += acc->n;
        live.peak_dbm[c] = acc->peak_dbm;
        if(acc->n < BW_MIN_SAMPLES_PER_CHAN) {
            if(radio->have_last) live.busy_ppt[c] = radio->last_busy_ppt[c];
            continue;
        }

        uint32_t hit = 0;
        int top_bin = -thresh;
        if(top_bin < 0) top_bin = 0;
        if(top_bin > BW_HIST_BINS - 1) top_bin = BW_HIST_BINS - 1;
        for(int bin = 0; bin <= top_bin; bin++) hit += acc->hist[bin];
        live.busy_ppt[c] = (uint16_t)((hit * 1000u + acc->n / 2u) / acc->n);
    }

    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    live.sweeps_done = radio->live.sweeps_done;
    live.rate_hz = radio->live.rate_hz;
    if(radio->accum.duration_ms) {
        uint32_t rate = (total * 1000u) / radio->accum.duration_ms;
        live.rate_hz = rate > 65535u ? 65535u : (uint16_t)rate;
    }
    radio->live = live;
    furi_mutex_release(radio->mutex);
}

static void radio_publish_sweep(BwRadio* radio, const BwSweep* sweep) {
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    radio->last_sweep = *sweep;
    radio->sweep_pending = true;
    radio->live.sweeps_done++;
    radio->live.rate_hz = sweep->rate_hz;
    furi_mutex_release(radio->mutex);

    if(radio->callback) radio->callback(radio->callback_ctx);
}

/* ----------------------------------------------------------------- sampling */

/** One dwell on one channel, against the real antenna. */
static void radio_dwell_hw(BwRadio* radio, BwChan chan) {
    const BwChanInfo* info = &bw_chan_info[chan];

    furi_hal_bt_start_rx(info->rf_index);
    furi_delay_ms(BW_SETTLE_MS);

    const uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(BW_DWELL_MS - BW_SETTLE_MS);
    uint32_t n = 0;
    while(radio->running && furi_get_tick() < deadline) {
        /* Each read is a round trip to the radio co-processor, which blocks
         * this thread and yields the CPU, so this loop is not normally a
         * spin. If a read ever returns without blocking, the yield below
         * keeps it from starving the rest of the system anyway. */
        float rssi = furi_hal_bt_get_rssi();
        bw_stats_add(&radio->accum, chan, (int)rssi);
        if((++n & 0x0Fu) == 0) furi_delay_tick(1);
    }

    furi_hal_bt_stop_rx();
}

/** One dwell on one channel, against a synthetic band. */
static void radio_dwell_demo(BwRadio* radio, BwChan chan) {
    /* bw_demo describes a whole band at once; take this channel's share of it
     * so the live bars fill in the same order the hardware would fill them. */
    const uint32_t want = (BW_DEMO_RATE_HZ * BW_DWELL_MS) / 1000u;
    bw_demo_fill(radio->demo_scene, radio->demo_seed, want ? want : 1u, &radio->demo_scratch);
    radio->demo_seed = radio->demo_seed * 1664525u + 1013904223u;

    const BwChanAccum* src = &radio->demo_scratch.acc[chan];
    for(int bin = 0; bin < BW_HIST_BINS; bin++) {
        for(uint16_t k = 0; k < src->hist[bin]; k++) {
            bw_stats_add(&radio->accum, chan, -bin);
        }
    }
    furi_delay_ms(BW_DWELL_MS);
}

static int32_t bw_radio_worker(void* context) {
    BwRadio* radio = context;

    if(!radio->demo) {
        /* Hand the radio over: stop being a Bluetooth device so it can be a
         * plain receiver. */
        radio->bt = furi_record_open(RECORD_BT);
        bt_disconnect(radio->bt);
        /* The second core needs a moment to flush its key storage. */
        furi_delay_ms(200);
    }

    const uint32_t sweep_ms = bw_sweep_len_ms(radio->sweep_len);

    while(radio->running) {
        bw_stats_reset(&radio->accum);
        const uint32_t started = furi_get_tick();
        uint32_t elapsed = 0;

        while(radio->running && elapsed < sweep_ms) {
            for(size_t c = 0; c < BwChanCount && radio->running; c++) {
                if(radio->demo) {
                    radio_dwell_demo(radio, (BwChan)c);
                } else {
                    radio_dwell_hw(radio, (BwChan)c);
                }

                elapsed = furi_get_tick() - started;
                radio->accum.duration_ms = elapsed ? elapsed : 1;
                uint32_t pct = (elapsed * 100u) / sweep_ms;
                radio_publish_live(radio, (uint8_t)c, (uint8_t)(pct > 100 ? 100 : pct));
                if(elapsed >= sweep_ms) break;
            }
        }

        if(!radio->running) break;

        radio->accum.duration_ms = furi_get_tick() - started;
        if(radio->accum.duration_ms == 0) radio->accum.duration_ms = 1;

        BwSweep sweep;
        bw_stats_finalize(&radio->accum, &sweep);
        if(sweep.valid) {
            radio->live_floor_dbm = sweep.band_floor_dbm;
            for(size_t c = 0; c < BwChanCount; c++) {
                radio->last_busy_ppt[c] = sweep.chan[c].busy_ppt;
            }
            radio->have_last = true;
        }
        radio_publish_sweep(radio, &sweep);
    }

    if(!radio->demo) {
        furi_hal_bt_stop_rx();
        /* Put the radio back the way it was found, or the Flipper's own
         * Bluetooth stays dead until it is rebooted. */
        furi_hal_bt_reinit();
        furi_delay_ms(200);
        bt_keys_storage_set_default_path(radio->bt);
        bt_profile_restore_default(radio->bt);
        furi_record_close(RECORD_BT);
        radio->bt = NULL;
    }

    return 0;
}

/* -------------------------------------------------------------------- API */

BwRadio* bw_radio_alloc(void) {
    BwRadio* radio = malloc(sizeof(BwRadio));
    memset(radio, 0, sizeof(BwRadio));
    radio->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    radio->sweep_len = BwSweepNormal;
    radio->live_floor_dbm = BW_LIVE_FLOOR_DBM;
    radio->demo_seed = 0xB1AD1E5u;
    radio->state = BwRadioIdle;
    bw_stats_reset(&radio->accum);
    return radio;
}

void bw_radio_free(BwRadio* radio) {
    furi_assert(radio);
    bw_radio_stop(radio);
    furi_mutex_free(radio->mutex);
    free(radio);
}

void bw_radio_set_sweep_len(BwRadio* radio, BwSweepLen len) {
    furi_assert(radio);
    if(len >= BwSweepLenCount) len = BwSweepNormal;
    radio->sweep_len = len;
}

void bw_radio_set_demo(BwRadio* radio, bool enabled, BwDemoScene scene) {
    furi_assert(radio);
    radio->demo = enabled;
    radio->demo_scene = scene < BwDemoCount ? scene : BwDemoQuietRoom;
}

bool bw_radio_is_demo(const BwRadio* radio) {
    return radio->demo;
}

bool bw_radio_start(BwRadio* radio, BwRadioSweepCallback cb, void* ctx) {
    furi_assert(radio);
    if(radio->running) return true;

    if(!radio->demo && !furi_hal_bt_is_testing_supported()) {
        /* Some radio stacks (the "light" one) have no RF test mode at all.
         * There is nothing clever to do about it and nothing to pretend. */
        radio->state = BwRadioUnsupported;
        return false;
    }

    radio->callback = cb;
    radio->callback_ctx = ctx;
    radio->running = true;
    radio->state = BwRadioRunning;
    radio->live_floor_dbm = BW_LIVE_FLOOR_DBM;

    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    memset(&radio->live, 0, sizeof(radio->live));
    radio->sweep_pending = false;
    furi_mutex_release(radio->mutex);

    radio->thread = furi_thread_alloc_ex("BulwarkRadio", 4096, bw_radio_worker, radio);
    furi_thread_start(radio->thread);
    return true;
}

void bw_radio_stop(BwRadio* radio) {
    furi_assert(radio);
    if(!radio->thread) {
        if(radio->state == BwRadioRunning) radio->state = BwRadioIdle;
        return;
    }

    radio->running = false;
    furi_thread_join(radio->thread);
    furi_thread_free(radio->thread);
    radio->thread = NULL;
    radio->callback = NULL;
    if(radio->state == BwRadioRunning) radio->state = BwRadioIdle;
}

BwRadioState bw_radio_state(const BwRadio* radio) {
    return radio->state;
}

void bw_radio_get_live(BwRadio* radio, BwRadioLive* out) {
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    *out = radio->live;
    furi_mutex_release(radio->mutex);
}

bool bw_radio_take_sweep(BwRadio* radio, BwSweep* out) {
    furi_mutex_acquire(radio->mutex, FuriWaitForever);
    bool had = radio->sweep_pending;
    if(had) {
        *out = radio->last_sweep;
        radio->sweep_pending = false;
    }
    furi_mutex_release(radio->mutex);
    return had;
}
