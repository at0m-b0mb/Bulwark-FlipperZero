#include "../bulwark_i.h"

/* One sweep, stored as "this is what this place is like when nothing is
 * wrong". It is the single most useful thing a user can do for the accuracy
 * of everything else, and without it the score is capped at 88 and says so.
 *
 * Take it somewhere you trust, standing where you will be standing. */

typedef enum {
    BaselineStateRunning = 0,
    BaselineStateDone,
} BaselineState;

static void bulwark_scene_baseline_show_result(BulwarkApp* app) {
    Widget* widget = app->widget;
    const BwSweep* s = &app->last_sweep;

    widget_reset(widget);
    widget_add_string_element(widget, 64, 10, AlignCenter, AlignBottom, FontPrimary, "Baseline stored");

    char line[48];
    snprintf(
        line,
        sizeof(line),
        "Advertising busy: %u.%u%%",
        s->adv_busy_ppt / 10u,
        s->adv_busy_ppt % 10u);
    widget_add_string_element(widget, 2, 24, AlignLeft, AlignBottom, FontSecondary, line);

    snprintf(
        line, sizeof(line), "Wi-Fi busy: %u.%u%%", s->ref_busy_ppt / 10u, s->ref_busy_ppt % 10u);
    widget_add_string_element(widget, 2, 34, AlignLeft, AlignBottom, FontSecondary, line);

    snprintf(line, sizeof(line), "Band floor: %d dBm", s->band_floor_dbm);
    widget_add_string_element(widget, 2, 44, AlignLeft, AlignBottom, FontSecondary, line);

    widget_add_string_multiline_element(
        widget,
        64,
        54,
        AlignCenter,
        AlignBottom,
        FontSecondary,
        "Anything much busier than\nthis is not this place.");

    view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewWidget);
}

void bulwark_scene_baseline_on_enter(void* context) {
    BulwarkApp* app = context;

    scene_manager_set_scene_state(app->scene_manager, BulwarkSceneBaseline, BaselineStateRunning);
    app->baseline_run = true;
    bulwark_watch_start(app);

    if(app->settings.keep_screen_on) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }

    BwWatchSnapshot snap;
    bulwark_scene_watch_fill(app, &snap);
    watch_view_update(app->watch_view, &snap);
    view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewWatch);
}

bool bulwark_scene_baseline_on_event(void* context, SceneManagerEvent event) {
    BulwarkApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        if(scene_manager_get_scene_state(app->scene_manager, BulwarkSceneBaseline) ==
           BaselineStateRunning) {
            BwWatchSnapshot snap;
            bulwark_scene_watch_fill(app, &snap);
            watch_view_update(app->watch_view, &snap);
            watch_view_tick(app->watch_view);
        }
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BulwarkEventSweepDone) {
        bulwark_consume_sweep(app);
        return true;
    }

    if(event.event == BulwarkEventBaselineDone) {
        bulwark_watch_stop(app);
        app->baseline_run = false;
        scene_manager_set_scene_state(app->scene_manager, BulwarkSceneBaseline, BaselineStateDone);
        bulwark_click(app);
        bulwark_scene_baseline_show_result(app);
        return true;
    }

    return false;
}

void bulwark_scene_baseline_on_exit(void* context) {
    BulwarkApp* app = context;
    bulwark_watch_stop(app);
    app->baseline_run = false;
    widget_reset(app->widget);
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
}
