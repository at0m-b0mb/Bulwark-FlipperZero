/**
 * Bulwark - settings, the stored baseline, and the sweep log.
 *
 * The baseline is the interesting one. It is a sweep the user took somewhere
 * they trust, kept between sessions, and it is what lets Bulwark say "this is
 * not what this place normally looks like" instead of guessing at what a
 * normal place looks like in general. Without one the score is capped, and
 * the app says which cap and why.
 */
#pragma once

#include "bw_score.h"
#include "bw_radio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BW_APP_FOLDER "/ext/apps_data/bulwark"
#define BW_SETTINGS_PATH BW_APP_FOLDER "/bulwark.settings"
#define BW_LOG_PATH BW_APP_FOLDER "/sweeps.csv"

typedef struct {
    uint8_t sweep_len; /**< BwSweepLen                              */
    bool alert; /**< buzz and flash when it turns SPAM LIKELY */
    bool logging; /**< append every sweep to sweeps.csv         */
    bool keep_screen_on;
    BwBaseline baseline;
} BwSettings;

void bw_store_defaults(BwSettings* settings);
bool bw_store_load(BwSettings* settings);
bool bw_store_save(const BwSettings* settings);

/** Make sure /ext/apps_data/bulwark exists. */
void bw_store_ensure_folder(void);

/** Append one scored sweep to the CSV. Creates the file with a header. */
bool bw_store_log_sweep(const BwSweep* sweep, const BwScore* score, bool demo);

/** Number of rows in the log, header excluded. -1 if there is no log. */
int32_t bw_store_log_rows(void);

/** Delete the log. */
bool bw_store_log_clear(void);

#ifdef __cplusplus
}
#endif
