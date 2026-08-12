#include "bw_score.h"

#include <string.h>

/* Family ceilings. They add up to 100 before any cap is applied. */
static const uint8_t family_max[BwFamilyCount] = {
    [BwFamilyOccupancy] = 34,
    [BwFamilyShape] = 30,
    [BwFamilyLevel] = 20,
    [BwFamilyPersistence] = 16,
};

/* What each cap allows the total to reach. BwCapNone allows everything. */
static const uint8_t cap_ceiling[BwCapCount] = {
    [BwCapNone] = 100,
    [BwCapQuietBand] = 20,
    [BwCapWifiDominant] = 30,
    [BwCapAtBaseline] = 30,
    [BwCapNoShape] = 44,
    [BwCapFirstLook] = 64,
    [BwCapWifiComparable] = 65,
    [BwCapNoBalance] = 69,
    [BwCapNoBaseline] = 88,
    [BwCapCeiling] = BW_SCORE_CEILING,
};

static void sig_set(
    BwScore* out,
    BwSignal id,
    BwFamily family,
    uint8_t points,
    uint8_t max_points,
    const char* name,
    const char* detail) {
    BwSigResult* s = &out->sig[id];
    s->id = id;
    s->family = family;
    s->points = points;
    s->max_points = max_points;
    s->name = name;
    s->detail = detail;
}

/* ---------------------------------------------------------------- OCCUPANCY */

static void score_adv_busy(const BwScoreInput* in, BwScore* out) {
    const uint16_t busy = in->sweep.adv_busy_ppt;
    uint8_t pts;
    const char* detail;

    if(busy < BW_GATE_PPT) {
        pts = 0;
        detail = "Adv channels idle";
    } else if(busy < 80) {
        pts = 8;
        detail = "Light adv traffic";
    } else if(busy < 200) {
        pts = 16;
        detail = "Adv channels busy";
    } else {
        pts = 24;
        detail = "Adv channels flooded";
    }

    /* A baseline is the user telling Bulwark what this room always looks
     * like. If today is no busier than that, today is this room. */
    if(in->baseline.taken && busy <= (uint32_t)in->baseline.adv_busy_ppt + BW_BASELINE_MARGIN_PPT) {
        if(pts > 8) pts = 8;
        detail = "No busier than baseline";
    }

    sig_set(out, BwSigAdvBusy, BwFamilyOccupancy, pts, 24, "Adv occupancy", detail);
}

static void score_adv_hot(const BwScoreInput* in, BwScore* out) {
    const uint16_t hot = in->sweep.adv_hot_ppt;
    uint8_t pts;
    const char* detail;

    if(hot < 10) {
        pts = 0;
        detail = "Nothing loud on adv";
    } else if(hot < 50) {
        pts = 5;
        detail = "Loud arrivals on adv";
    } else {
        pts = 10;
        detail = "Loud nearly always";
    }

    sig_set(out, BwSigAdvHot, BwFamilyOccupancy, pts, 10, "Adv loudness", detail);
}

/* -------------------------------------------------------------------- SHAPE */

static void score_tri_balance(const BwScoreInput* in, BwScore* out) {
    const uint16_t lo = in->sweep.adv_busy_min_ppt;
    const uint16_t hi = in->sweep.adv_busy_max_ppt;
    uint8_t pts = 0;
    const char* detail;

    if(lo < BW_GATE_PPT) {
        detail = (hi >= BW_GATE_PPT) ? "Only some adv busy" : "Adv channels idle";
    } else if((uint32_t)hi * 2u <= (uint32_t)lo * 5u) {
        /* hi <= 2.5 * lo. Nothing else in this band does this. */
        pts = 16;
        detail = "37/38/39 hit equally";
    } else if((uint32_t)hi <= (uint32_t)lo * 4u) {
        pts = 8;
        detail = "37/38/39 all active";
    } else {
        detail = "Adv channels uneven";
    }

    sig_set(out, BwSigTriBalance, BwFamilyShape, pts, 16, "Tri-channel", detail);
}

static void score_guard_contrast(const BwScoreInput* in, BwScore* out) {
    const uint32_t adv = in->sweep.adv_busy_ppt;
    /* Zero would divide; one part per thousand is below anything measurable
     * anyway, so the ratio stays honest. */
    const uint32_t ref = in->sweep.ref_busy_ppt ? in->sweep.ref_busy_ppt : 1u;
    uint8_t pts = 0;
    const char* detail;

    if(adv < BW_GATE_PPT) {
        detail = "Adv channels idle";
    } else if(adv >= ref * 4u) {
        pts = 14;
        detail = "On BLE, not on Wi-Fi";
    } else if(adv >= ref * 2u) {
        pts = 8;
        detail = "Adv busier than Wi-Fi";
    } else {
        detail = "Wi-Fi just as busy";
    }

    sig_set(out, BwSigGuardContrast, BwFamilyShape, pts, 14, "Guard contrast", detail);
}

/* -------------------------------------------------------------------- LEVEL */

static void score_swing(const BwScoreInput* in, BwScore* out) {
    const uint8_t swing = in->sweep.adv_swing_db;
    uint8_t pts;
    const char* detail;

    if(swing >= 35) {
        pts = 12;
        detail = "Peaks tower over floor";
    } else if(swing >= 25) {
        pts = 8;
        detail = "Peaks well over floor";
    } else if(swing >= 15) {
        pts = 4;
        detail = "Peaks over floor";
    } else {
        pts = 0;
        detail = "Nothing rises above";
    }

    sig_set(out, BwSigSwing, BwFamilyLevel, pts, 12, "Peak swing", detail);
}

static void score_close_range(const BwScoreInput* in, BwScore* out) {
    const int8_t peak = in->sweep.adv_peak_dbm;
    uint8_t pts;
    const char* detail;

    /* Same convention as the sampler: 0 dBm is not a reading, it is the
     * absence of one, and it must not read as the loudest thing in the room. */
    if(peak >= 0) {
        pts = 0;
        detail = "Nothing close by";
    } else if(peak >= -50) {
        pts = 8;
        detail = "Loudest is very close";
    } else if(peak >= -65) {
        pts = 4;
        detail = "Loudest is nearby";
    } else {
        pts = 0;
        detail = "Nothing close by";
    }

    sig_set(out, BwSigCloseRange, BwFamilyLevel, pts, 8, "Proximity", detail);
}

/* -------------------------------------------------------------- PERSISTENCE */

static void score_persist(const BwScoreInput* in, BwScore* out) {
    const uint16_t streak = in->streak;
    uint8_t pts;
    const char* detail;

    if(streak >= 8) {
        pts = 16;
        detail = "Held for 8+ sweeps";
    } else if(streak >= 4) {
        pts = 11;
        detail = "Held for 4+ sweeps";
    } else if(streak >= 2) {
        pts = 6;
        detail = "Held for 2+ sweeps";
    } else {
        pts = 0;
        detail = "Not seen twice yet";
    }

    sig_set(out, BwSigPersist, BwFamilyPersistence, pts, 16, "Persistence", detail);
}

/* --------------------------------------------------------------------- caps */

static void cap_apply(BwScore* out, BwCap cap) {
    out->caps_applied |= (1u << cap);
}

static BwInterference interference_of(const BwSweep* sweep) {
    if(sweep->ref_busy_ppt >= 250) return BwInterferenceHeavy;
    if(sweep->ref_busy_ppt >= 80) return BwInterferenceModerate;
    return BwInterferenceNone;
}

static BwVerdict verdict_of(uint8_t score) {
    if(score <= 24) return BwVerdictClear;
    if(score <= 44) return BwVerdictBusy;
    if(score <= 69) return BwVerdictSuspect;
    return BwVerdictSpam;
}

void bw_score_eval(const BwScoreInput* in, BwScore* out) {
    memset(out, 0, sizeof(BwScore));
    memcpy(out->family_max, family_max, sizeof(family_max));
    out->binding_cap = BwCapNone;

    if(!in->sweep.valid) {
        out->have_data = false;
        out->verdict = BwVerdictNoData;
        /* Still fill the signal list so the detail screen is never blank. */
        BwScoreInput empty;
        memset(&empty, 0, sizeof(empty));
        score_adv_busy(&empty, out);
        score_adv_hot(&empty, out);
        score_tri_balance(&empty, out);
        score_guard_contrast(&empty, out);
        score_swing(&empty, out);
        score_close_range(&empty, out);
        score_persist(&empty, out);
        return;
    }

    out->have_data = true;
    out->interference = interference_of(&in->sweep);

    score_adv_busy(in, out);
    score_adv_hot(in, out);
    score_tri_balance(in, out);
    score_guard_contrast(in, out);
    score_swing(in, out);
    score_close_range(in, out);
    score_persist(in, out);

    for(size_t i = 0; i < BwSigCount; i++) {
        out->family_points[out->sig[i].family] =
            (uint8_t)(out->family_points[out->sig[i].family] + out->sig[i].points);
    }
    for(size_t f = 0; f < BwFamilyCount; f++) {
        if(out->family_points[f] > family_max[f]) out->family_points[f] = family_max[f];
        out->raw_score = (uint8_t)(out->raw_score + out->family_points[f]);
    }

    /* Which ceilings are in force. */
    if(out->family_points[BwFamilyOccupancy] == 0) cap_apply(out, BwCapQuietBand);
    if(out->family_points[BwFamilyShape] == 0) cap_apply(out, BwCapNoShape);
    if(out->sig[BwSigTriBalance].points < 16) cap_apply(out, BwCapNoBalance);
    /* Wi-Fi channels 1, 6 and 11 are 20 MHz wide, so a busy access point does
     * spill onto 2402, 2426 and 2480 MHz. Two ceilings, because "as busy as"
     * and "twice as busy as" are different amounts of blindness. */
    if(in->sweep.ref_busy_ppt >= BW_GATE_PPT) {
        if(in->sweep.ref_busy_ppt >= (uint32_t)in->sweep.adv_busy_ppt * 2u) {
            cap_apply(out, BwCapWifiDominant);
        } else if(in->sweep.ref_busy_ppt >= in->sweep.adv_busy_ppt) {
            cap_apply(out, BwCapWifiComparable);
        }
    }
    if(in->baseline.taken) {
        if(in->sweep.adv_busy_ppt <=
           (uint32_t)in->baseline.adv_busy_ppt + BW_BASELINE_MARGIN_PPT) {
            cap_apply(out, BwCapAtBaseline);
        }
    } else {
        cap_apply(out, BwCapNoBaseline);
    }
    if(in->sweeps_seen < 2) cap_apply(out, BwCapFirstLook);
    cap_apply(out, BwCapCeiling);

    /* The binding cap is the lowest ceiling in force. Ties go to the lower
     * enum value, so the answer never depends on iteration order. */
    uint8_t ceiling = 100;
    for(size_t c = 1; c < BwCapCount; c++) {
        if(!(out->caps_applied & (1u << c))) continue;
        if(cap_ceiling[c] < ceiling) {
            ceiling = cap_ceiling[c];
            out->binding_cap = (BwCap)c;
        }
    }

    out->score = out->raw_score > ceiling ? ceiling : out->raw_score;
    /* A cap that never actually bit is not worth showing as the reason. */
    if(out->raw_score <= ceiling) out->binding_cap = BwCapNone;

    out->verdict = verdict_of(out->score);
}

/* ------------------------------------------------------------- rolling state */

void bw_watch_reset(BwWatch* watch) {
    BwBaseline keep = watch->baseline;
    memset(watch, 0, sizeof(BwWatch));
    watch->baseline = keep;
}

void bw_watch_feed(BwWatch* watch, const BwSweep* sweep, BwScore* out) {
    if(sweep->valid) {
        watch->sweeps_seen++;
        if(sweep->adv_busy_ppt >= BW_GATE_PPT) {
            if(watch->streak < 0xFFFFu) watch->streak++;
        } else {
            watch->streak = 0;
        }
    }

    BwScoreInput in;
    memset(&in, 0, sizeof(in));
    in.sweep = *sweep;
    in.streak = watch->streak;
    in.sweeps_seen = watch->sweeps_seen;
    in.baseline = watch->baseline;

    bw_score_eval(&in, out);
    if(out->score > watch->peak_score) watch->peak_score = out->score;
}

void bw_watch_set_baseline(BwWatch* watch, const BwSweep* sweep) {
    if(!sweep->valid) return;
    watch->baseline.taken = true;
    watch->baseline.adv_busy_ppt = sweep->adv_busy_ppt;
    watch->baseline.ref_busy_ppt = sweep->ref_busy_ppt;
    watch->baseline.band_floor_dbm = sweep->band_floor_dbm;
}

/* -------------------------------------------------------------------- names */

const char* bw_verdict_name(BwVerdict v) {
    switch(v) {
    case BwVerdictClear:
        return "CLEAR";
    case BwVerdictBusy:
        return "BUSY BAND";
    case BwVerdictSuspect:
        return "SUSPECT";
    case BwVerdictSpam:
        return "SPAM LIKELY";
    default:
        return "LISTENING";
    }
}

const char* bw_verdict_blurb(BwVerdict v) {
    switch(v) {
    case BwVerdictClear:
        return "Advertising channels quiet";
    case BwVerdictBusy:
        return "Band busy, not BLE-shaped";
    case BwVerdictSuspect:
        return "Advertising flood, unproven";
    case BwVerdictSpam:
        return "Someone is flooding 37/38/39";
    default:
        return "Not enough samples yet";
    }
}

const char* bw_family_name(BwFamily f) {
    switch(f) {
    case BwFamilyOccupancy:
        return "OCCUPANCY";
    case BwFamilyShape:
        return "SHAPE";
    case BwFamilyLevel:
        return "LEVEL";
    case BwFamilyPersistence:
        return "PERSISTENCE";
    default:
        return "?";
    }
}

const char* bw_cap_name(BwCap c) {
    switch(c) {
    case BwCapQuietBand:
        return "Quiet band";
    case BwCapWifiDominant:
        return "Wi-Fi dominant";
    case BwCapWifiComparable:
        return "Wi-Fi comparable";
    case BwCapAtBaseline:
        return "At baseline";
    case BwCapNoShape:
        return "Not BLE-shaped";
    case BwCapNoBalance:
        return "No tri-channel";
    case BwCapFirstLook:
        return "First look";
    case BwCapNoBaseline:
        return "No baseline";
    case BwCapCeiling:
        return "Cannot read packets";
    default:
        return "None";
    }
}

const char* bw_cap_reason(BwCap c) {
    switch(c) {
    case BwCapQuietBand:
        return "Nothing is transmitting much. Shape and level of an empty "
               "band prove nothing at all.";
    case BwCapWifiDominant:
        return "The Wi-Fi centres are twice as busy as the advertising "
               "channels. Bulwark cannot see through that.";
    case BwCapWifiComparable:
        return "Wi-Fi is as busy as the advertising channels, and a 20 MHz "
               "Wi-Fi channel reaches them. Could be either.";
    case BwCapAtBaseline:
        return "No busier than the quiet reading you stored for this place. "
               "This is what this room is like.";
    case BwCapNoShape:
        return "The band is busy but the busyness is not Bluetooth-shaped. "
               "Ovens and video senders do this.";
    case BwCapNoBalance:
        return "2402, 2426 and 2480 MHz are not being hit in equal measure. "
               "Only BLE advertising does that.";
    case BwCapFirstLook:
        return "One sweep is not a watch. Bulwark wants to see it again "
               "before it says so out loud.";
    case BwCapNoBaseline:
        return "No quiet reading stored for this place, so there is nothing "
               "to say what normal looks like here.";
    case BwCapCeiling:
        return "Bulwark hears advertising traffic. It cannot read one packet, "
               "so it never reaches certainty.";
    default:
        return "Nothing held this score down.";
    }
}

const char* bw_interference_name(BwInterference i) {
    switch(i) {
    case BwInterferenceHeavy:
        return "heavy";
    case BwInterferenceModerate:
        return "moderate";
    default:
        return "low";
    }
}
