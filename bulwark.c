#include "bulwark_i.h"

#include <string.h>

/* ---------------- feedback ----------------
 *
 * The point of this app is that you are not looking at it. It sits on a table
 * or in a pocket while you do something else, so SUSPECT is a rising pair of
 * notes you can ignore and SPAM LIKELY is three notes, the buzzer and the red
 * LED - two things you can tell apart without taking it out.
 */

static const NotificationSequence seq_suspect = {
    &message_note_e5,
    &message_delay_50,
    &message_note_a5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_spam = {
    &message_note_a5,
    &message_delay_100,
    &message_note_c4,
    &message_delay_100,
    &message_note_a5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_led_suspect = {
    &message_red_255,
    &message_green_255,
    &message_delay_250,
    &message_red_0,
    &message_green_0,
    NULL,
};

static const NotificationSequence seq_led_spam = {
    &message_red_255,
    &message_delay_250,
    &message_red_0,
    &message_delay_100,
    &message_red_255,
    &message_delay_250,
    &message_red_0,
    NULL,
};

static const NotificationSequence seq_vibro = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

void bulwark_alert(BulwarkApp* app, BwVerdict verdict) {
    furi_assert(app);
    if(!app->settings.alert) return;
    if(verdict < BwVerdictSuspect) return;

    uint32_t now = furi_get_tick();
    /* A worse verdict always gets through: being told about a SUSPECT must
     * never be what stops you being told about the thing behind it. */
    if(verdict <= app->last_alert_verdict &&
       now - app->last_alert_tick < furi_ms_to_ticks(BULWARK_ALERT_GAP_MS)) {
        return;
    }
    app->last_alert_tick = now;
    app->last_alert_verdict = verdict;

    if(verdict >= BwVerdictSpam) {
        notification_message(app->notifications, &seq_spam);
        notification_message(app->notifications, &seq_led_spam);
        notification_message(app->notifications, &seq_vibro);
    } else {
        notification_message(app->notifications, &seq_suspect);
        notification_message(app->notifications, &seq_led_suspect);
    }
}

void bulwark_click(BulwarkApp* app) {
    furi_assert(app);
    if(app->settings.alert) notification_message(app->notifications, &sequence_semi_success);
}

/* ---------------- the radio ---------------- */

/* Called on the radio worker thread. It does exactly one thing: wake the GUI.
 * Everything else happens on the GUI thread, where the state lives. */
static void bulwark_on_sweep(void* context) {
    BulwarkApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BulwarkEventSweepDone);
}

bool bulwark_watch_start(BulwarkApp* app) {
    furi_assert(app);

    bw_radio_set_sweep_len(app->radio, app->settings.sweep_len);
    bw_radio_set_demo(app->radio, app->demo, app->demo_scene);

    bw_watch_reset(&app->watch);
    app->watch.baseline = app->settings.baseline;
    app->history_len = 0;
    app->have_result = false;
    app->last_alert_verdict = BwVerdictNoData;
    memset(&app->last_sweep, 0, sizeof(app->last_sweep));
    memset(&app->last_score, 0, sizeof(app->last_score));

    return bw_radio_start(app->radio, bulwark_on_sweep, app);
}

void bulwark_watch_stop(BulwarkApp* app) {
    furi_assert(app);
    bw_radio_stop(app->radio);
}

void bulwark_consume_sweep(BulwarkApp* app) {
    furi_assert(app);

    BwSweep sweep;
    if(!bw_radio_take_sweep(app->radio, &sweep)) return;

    if(app->baseline_run) {
        if(sweep.valid) {
            bw_watch_set_baseline(&app->watch, &sweep);
            app->settings.baseline = app->watch.baseline;
            bw_store_save(&app->settings);
            app->last_sweep = sweep;
            app->have_result = true;
            view_dispatcher_send_custom_event(app->view_dispatcher, BulwarkEventBaselineDone);
        }
        return;
    }

    bw_watch_feed(&app->watch, &sweep, &app->last_score);
    app->last_sweep = sweep;
    app->have_result = app->last_score.have_data;

    if(app->have_result) {
        /* The trend page is a queue: oldest column falls off the left. */
        if(app->history_len < BW_HISTORY_LEN) {
            app->history[app->history_len++] = app->last_score.score;
        } else {
            memmove(app->history, app->history + 1, BW_HISTORY_LEN - 1);
            app->history[BW_HISTORY_LEN - 1] = app->last_score.score;
        }

        if(app->settings.logging) {
            bw_store_log_sweep(&sweep, &app->last_score, app->demo);
        }
        bulwark_alert(app, app->last_score.verdict);
    }
}

/* ---------------- view dispatcher plumbing ---------------- */

static bool bulwark_custom_event_callback(void* context, uint32_t event) {
    BulwarkApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bulwark_back_event_callback(void* context) {
    BulwarkApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void bulwark_tick_event_callback(void* context) {
    BulwarkApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* ---------------- lifecycle ---------------- */

static BulwarkApp* bulwark_app_alloc(void) {
    BulwarkApp* app = malloc(sizeof(BulwarkApp));
    memset(app, 0, sizeof(BulwarkApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&bulwark_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, bulwark_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bulwark_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, bulwark_tick_event_callback, BULWARK_TICK_MS);

    bw_store_defaults(&app->settings);
    bw_store_load(&app->settings);
    app->watch.baseline = app->settings.baseline;

    app->radio = bw_radio_alloc();

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BulwarkViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BulwarkViewSettings,
        variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, BulwarkViewWidget, widget_get_view(app->widget));

    app->splash_view = splash_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BulwarkViewSplash, splash_view_get_view(app->splash_view));

    app->watch_view = watch_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BulwarkViewWatch, watch_view_get_view(app->watch_view));

    app->detail_view = detail_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BulwarkViewDetail, detail_view_get_view(app->detail_view));

    app->learn_view = learn_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BulwarkViewLearn, learn_view_get_view(app->learn_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void bulwark_app_free(BulwarkApp* app) {
    furi_assert(app);

    bulwark_watch_stop(app);
    bw_store_save(&app->settings);

    view_dispatcher_remove_view(app->view_dispatcher, BulwarkViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, BulwarkViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, BulwarkViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, BulwarkViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, BulwarkViewWatch);
    view_dispatcher_remove_view(app->view_dispatcher, BulwarkViewDetail);
    view_dispatcher_remove_view(app->view_dispatcher, BulwarkViewLearn);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    splash_view_free(app->splash_view);
    watch_view_free(app->watch_view);
    detail_view_free(app->detail_view);
    learn_view_free(app->learn_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    bw_radio_free(app->radio);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t bulwark_app(void* p) {
    UNUSED(p);
    BulwarkApp* app = bulwark_app_alloc();
    scene_manager_next_scene(app->scene_manager, BulwarkSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    bulwark_app_free(app);
    return 0;
}
