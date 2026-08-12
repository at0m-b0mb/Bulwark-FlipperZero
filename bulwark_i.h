#pragma once

#include "helpers/bw_chan.h"
#include "helpers/bw_stats.h"
#include "helpers/bw_score.h"
#include "helpers/bw_demo.h"
#include "helpers/bw_radio.h"
#include "helpers/bw_store.h"

#include "views/splash_view.h"
#include "views/watch_view.h"
#include "views/detail_view.h"
#include "views/learn_view.h"

#include "scenes/bulwark_scene.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define BULWARK_TICK_MS 100u

/** How long the app keeps quiet after making a noise about the same verdict. */
#define BULWARK_ALERT_GAP_MS 8000u

/** How many sweeps the trend page remembers. One pixel column each. */
#define BW_HISTORY_LEN 118

typedef enum {
    BulwarkViewSubmenu,
    BulwarkViewSettings,
    BulwarkViewWidget,
    BulwarkViewSplash,
    BulwarkViewWatch,
    BulwarkViewDetail,
    BulwarkViewLearn,
} BulwarkViewId;

typedef enum {
    /* Submenu items send their own index, so app events start well clear. */
    BulwarkEventSkipSplash = 200,
    BulwarkEventSweepDone,
    BulwarkEventOpenDetail,
    BulwarkEventBaselineDone,
    BulwarkEventRadioUnsupported,
} BulwarkCustomEvent;

typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;

    SplashView* splash_view;
    WatchView* watch_view;
    DetailView* detail_view;
    LearnView* learn_view;

    BwRadio* radio;
    BwSettings settings;
    BwWatch watch;

    BwSweep last_sweep;
    BwScore last_score;
    bool have_result;

    /* The trend page: one score per finished sweep, oldest first. */
    uint8_t history[BW_HISTORY_LEN];
    uint16_t history_len;

    bool splash_done;
    bool demo;
    BwDemoScene demo_scene;

    /* The baseline run stores its sweep instead of scoring it. */
    bool baseline_run;

    BwVerdict last_alert_verdict;
    uint32_t last_alert_tick;
} BulwarkApp;

/* Shared behaviour, in bulwark.c. */

/** Start the radio (or the synthetic band) and clear the session. */
bool bulwark_watch_start(BulwarkApp* app);
void bulwark_watch_stop(BulwarkApp* app);

/** Fold a finished sweep into the session. Called from the GUI thread. */
void bulwark_consume_sweep(BulwarkApp* app);

/** Build the watch screen's snapshot. Shared with the baseline scene. */
void bulwark_scene_watch_fill(BulwarkApp* app, BwWatchSnapshot* snap);

/** Noise, light and buzz, rate-limited, honouring the settings. */
void bulwark_alert(BulwarkApp* app, BwVerdict verdict);
void bulwark_click(BulwarkApp* app);
