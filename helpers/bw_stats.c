#include "bw_stats.h"

#include <string.h>

#define BW_DBM_MIN (-127)
#define BW_DBM_MAX (-1)

void bw_stats_reset(BwSweepAccum* accum) {
    memset(accum, 0, sizeof(BwSweepAccum));
    for(size_t i = 0; i < BwChanCount; i++) {
        accum->acc[i].peak_dbm = BW_DBM_MIN;
    }
}

void bw_stats_add(BwSweepAccum* accum, BwChan chan, int dbm) {
    if(chan >= BwChanCount) return;
    /* The stack reports exactly 0 dBm when the RSSI read failed. A real
     * reading of 0 dBm would mean the transmitter is touching the antenna,
     * so dropping it costs nothing and keeps a failed read from reading as
     * the loudest signal of the sweep. */
    if(dbm >= 0) return;
    if(dbm < BW_DBM_MIN) dbm = BW_DBM_MIN;
    if(dbm > BW_DBM_MAX) dbm = BW_DBM_MAX;

    BwChanAccum* acc = &accum->acc[chan];
    acc->hist[-dbm]++;
    acc->n++;
    if((int8_t)dbm > acc->peak_dbm) acc->peak_dbm = (int8_t)dbm;
}

int8_t bw_stats_percentile(const BwChanAccum* acc, uint32_t pct) {
    if(acc->n == 0) return BW_DBM_MIN;
    if(pct > 100) pct = 100;

    /* The first sample index that satisfies the percentile, counting from the
     * weak end. Rounded up so percentile(0) is the weakest sample rather than
     * an empty selection. */
    uint32_t want = (acc->n * pct + 99) / 100;
    if(want == 0) want = 1;

    uint32_t seen = 0;
    for(int bin = BW_HIST_BINS - 1; bin >= 0; bin--) {
        seen += acc->hist[bin];
        if(seen >= want) return (int8_t)(-bin);
    }
    return acc->peak_dbm;
}

/** Share of a channel's samples at or above @p thresh, in parts per thousand. */
static uint16_t bw_share_ppt(const BwChanAccum* acc, int thresh_dbm) {
    if(acc->n == 0) return 0;
    if(thresh_dbm < BW_DBM_MIN) thresh_dbm = BW_DBM_MIN;

    uint32_t hit = 0;
    int top_bin = -thresh_dbm;
    if(top_bin > BW_HIST_BINS - 1) top_bin = BW_HIST_BINS - 1;
    for(int bin = 0; bin <= top_bin; bin++) {
        hit += acc->hist[bin];
    }
    return (uint16_t)((hit * 1000u + acc->n / 2u) / acc->n);
}

void bw_stats_finalize(const BwSweepAccum* accum, BwSweep* out) {
    memset(out, 0, sizeof(BwSweep));
    out->duration_ms = accum->duration_ms;
    out->adv_peak_dbm = BW_DBM_MIN;
    out->band_floor_dbm = BW_DBM_MIN;

    /* Pass one: per-channel floors, and the quietest of them. */
    bool any_valid = false;
    int band_floor = 0; /* stronger than any legal reading, so any floor wins */
    for(size_t i = 0; i < BwChanCount; i++) {
        const BwChanAccum* acc = &accum->acc[i];
        BwChanResult* res = &out->chan[i];

        res->n = acc->n;
        res->valid = acc->n >= BW_MIN_SAMPLES_PER_CHAN;
        out->samples += acc->n;
        if(!res->valid) continue;

        res->floor_dbm = bw_stats_percentile(acc, BW_FLOOR_PCT);
        res->p90_dbm = bw_stats_percentile(acc, 90);
        res->peak_dbm = acc->peak_dbm;

        if(res->floor_dbm < band_floor) band_floor = res->floor_dbm;
        any_valid = true;
    }

    if(!any_valid) return;
    out->band_floor_dbm = (int8_t)band_floor;
    out->valid = true;

    /* Pass two: occupancy, measured against the quietest corner of the band. */
    uint32_t adv_busy_sum = 0, adv_hot_sum = 0, ref_busy_sum = 0;
    uint32_t adv_n = 0, ref_n = 0;
    uint16_t adv_busy_min = 1000, adv_busy_max = 0;

    for(size_t i = 0; i < BwChanCount; i++) {
        const BwChanAccum* acc = &accum->acc[i];
        BwChanResult* res = &out->chan[i];
        if(!res->valid) {
            out->valid = false;
            continue;
        }

        res->busy_ppt = bw_share_ppt(acc, band_floor + BW_BUSY_MARGIN_DB);
        res->hot_ppt = bw_share_ppt(acc, band_floor + BW_HOT_MARGIN_DB);

        if(bw_chan_is_adv((BwChan)i)) {
            adv_busy_sum += res->busy_ppt;
            adv_hot_sum += res->hot_ppt;
            adv_n++;
            if(res->busy_ppt < adv_busy_min) adv_busy_min = res->busy_ppt;
            if(res->busy_ppt > adv_busy_max) adv_busy_max = res->busy_ppt;
            if(res->peak_dbm > out->adv_peak_dbm) out->adv_peak_dbm = res->peak_dbm;
        } else {
            ref_busy_sum += res->busy_ppt;
            ref_n++;
        }
    }

    if(adv_n) {
        out->adv_busy_ppt = (uint16_t)(adv_busy_sum / adv_n);
        out->adv_hot_ppt = (uint16_t)(adv_hot_sum / adv_n);
        out->adv_busy_min_ppt = adv_busy_min;
        out->adv_busy_max_ppt = adv_busy_max;
    }
    if(ref_n) out->ref_busy_ppt = (uint16_t)(ref_busy_sum / ref_n);

    int swing = out->adv_peak_dbm - band_floor;
    if(swing < 0) swing = 0;
    if(swing > 255) swing = 255;
    out->adv_swing_db = (uint8_t)swing;

    if(out->duration_ms) {
        uint32_t rate = (out->samples * 1000u) / out->duration_ms;
        out->rate_hz = rate > 65535u ? 65535u : (uint16_t)rate;
    }
}
