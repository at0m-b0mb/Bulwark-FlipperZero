#pragma once

#include "../helpers/bw_score.h"
#include "../helpers/bw_radio.h"

#include <gui/view.h>

/** How many finished sweeps the trend page remembers. One column each. */
#define BW_HISTORY_LEN 118

/** Everything the watch screen is allowed to know. The scene fills one of
 *  these on the tick; the view never reaches into the radio itself. */
typedef struct {
    bool supported; /**< false when this radio stack has no RF test mode */
    bool demo;
    bool baseline_run;
    bool have_result;
    bool baseline_taken;
    uint16_t baseline_busy_ppt; /**< drawn as the "normal here" line */

    BwVerdict verdict;
    uint8_t score;
    BwInterference interference;

    uint16_t busy_ppt[BwChanCount];
    uint8_t chan_now;
    uint8_t progress_pct;
    uint16_t rate_hz;
    uint32_t sweeps_done;
    uint16_t streak;
    uint16_t peak_score;
    uint8_t sweep_secs;

    uint8_t history[BW_HISTORY_LEN];
    uint16_t history_len;
} BwWatchSnapshot;

typedef struct WatchView WatchView;
typedef void (*WatchViewCallback)(void* context);

WatchView* watch_view_alloc(void);
void watch_view_free(WatchView* v);
View* watch_view_get_view(WatchView* v);

void watch_view_update(WatchView* v, const BwWatchSnapshot* snap);
void watch_view_tick(WatchView* v);

/** OK - open the breakdown. */
void watch_view_set_open_callback(WatchView* v, WatchViewCallback cb, void* context);
/** OK held - throw the session away and start listening again. */
void watch_view_set_reset_callback(WatchView* v, WatchViewCallback cb, void* context);
