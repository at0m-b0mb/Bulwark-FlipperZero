#include "../bulwark_i.h"

/* The radio must survive a trip into the breakdown screen, and it must not
 * survive walking out of the watch altogether. The scene state tells the two
 * apart: scene_manager_next_scene() runs this scene's on_exit on the way to a
 * child, so on_exit has to know which kind of leaving it is looking at. */
typedef enum {
    WatchStateNormal = 0,
    WatchStateChild,
} WatchState;

static void bulwark_scene_watch_open_cb(void* context) {
    BulwarkApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BulwarkEventOpenDetail);
}

static void bulwark_scene_watch_reset_cb(void* context) {
    BulwarkApp* app = context;
    bulwark_watch_stop(app);
    bulwark_watch_start(app);
    bulwark_click(app);
}

void bulwark_scene_watch_fill(BulwarkApp* app, BwWatchSnapshot* snap) {
    memset(snap, 0, sizeof(*snap));

    BwRadioLive live;
    bw_radio_get_live(app->radio, &live);

    snap->supported = bw_radio_state(app->radio) != BwRadioUnsupported;
    snap->demo = app->demo;
    snap->baseline_run = app->baseline_run;
    snap->have_result = app->have_result;
    snap->baseline_taken = app->settings.baseline.taken;
    snap->baseline_busy_ppt = app->settings.baseline.adv_busy_ppt;

    snap->verdict = app->last_score.verdict;
    snap->score = app->last_score.score;
    snap->interference = app->last_score.interference;

    memcpy(snap->busy_ppt, live.busy_ppt, sizeof(snap->busy_ppt));
    snap->chan_now = live.chan_now;
    snap->progress_pct = live.progress_pct;
    snap->rate_hz = live.rate_hz;
    snap->sweeps_done = live.sweeps_done;
    snap->streak = app->watch.streak;
    snap->peak_score = app->watch.peak_score;
    snap->sweep_secs = (uint8_t)(bw_sweep_len_ms(app->settings.sweep_len) / 1000u);

    uint16_t n = app->history_len;
    if(n > BW_HISTORY_LEN) n = BW_HISTORY_LEN;
    memcpy(snap->history, app->history, n);
    snap->history_len = n;
}

void bulwark_scene_watch_on_enter(void* context) {
    BulwarkApp* app = context;

    watch_view_set_open_callback(app->watch_view, bulwark_scene_watch_open_cb, app);
    watch_view_set_reset_callback(app->watch_view, bulwark_scene_watch_reset_cb, app);

    if(bw_radio_state(app->radio) != BwRadioRunning) {
        app->baseline_run = false;
        bulwark_watch_start(app);
    }

    if(app->settings.keep_screen_on) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    BwWatchSnapshot snap;
    bulwark_scene_watch_fill(app, &snap);
    watch_view_update(app->watch_view, &snap);

    view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewWatch);
}

bool bulwark_scene_watch_on_event(void* context, SceneManagerEvent event) {
    BulwarkApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        BwWatchSnapshot snap;
        bulwark_scene_watch_fill(app, &snap);
        watch_view_update(app->watch_view, &snap);
        watch_view_tick(app->watch_view);
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BulwarkEventSweepDone) {
        bulwark_consume_sweep(app);
        return true;
    }

    if(event.event == BulwarkEventOpenDetail) {
        if(!app->have_result) {
            bulwark_click(app);
            return true;
        }
        detail_view_update(
            app->detail_view,
            &app->last_score,
            &app->last_sweep,
            app->demo ? bw_demo_name(app->demo_scene) : NULL,
            app->demo ? bw_demo_blurb(app->demo_scene) : NULL);
        scene_manager_set_scene_state(app->scene_manager, BulwarkSceneWatch, WatchStateChild);
        scene_manager_next_scene(app->scene_manager, BulwarkSceneDetail);
        return true;
    }

    return false;
}

void bulwark_scene_watch_on_exit(void* context) {
    BulwarkApp* app = context;

    if(scene_manager_get_scene_state(app->scene_manager, BulwarkSceneWatch) == WatchStateChild) {
        /* A detour, not an exit. Leave the radio listening. */
        scene_manager_set_scene_state(app->scene_manager, BulwarkSceneWatch, WatchStateNormal);
        return;
    }

    bulwark_watch_stop(app);
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
}
