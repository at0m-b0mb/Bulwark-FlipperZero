#include "../bulwark_i.h"

/* The intro plays for a shade under two seconds at the 100 ms tick and any key
 * skips it. It lives inside the root scene rather than on the scene stack, so
 * coming back to the menu from a watch never replays it, and Back from the
 * menu still leaves the app cleanly. */

typedef enum {
    StartIndexWatch,
    StartIndexBaseline,
    StartIndexForget,
    StartIndexLearn,
    StartIndexLog,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void bulwark_scene_start_submenu_cb(void* context, uint32_t index) {
    BulwarkApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void bulwark_scene_start_show_menu(BulwarkApp* app) {
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Bulwark");
    submenu_add_item(
        submenu, "Watch the band", StartIndexWatch, bulwark_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu,
        app->settings.baseline.taken ? "Retake baseline" : "Take a baseline",
        StartIndexBaseline,
        bulwark_scene_start_submenu_cb,
        app);
    if(app->settings.baseline.taken) {
        submenu_add_item(
            submenu, "Forget baseline", StartIndexForget, bulwark_scene_start_submenu_cb, app);
    }
    submenu_add_item(
        submenu, "How BLE spam works", StartIndexLearn, bulwark_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "Sweep log", StartIndexLog, bulwark_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Settings", StartIndexSettings, bulwark_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, bulwark_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, BulwarkSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewSubmenu);
}

static void bulwark_scene_start_skip_splash(void* context) {
    BulwarkApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BulwarkEventSkipSplash);
}

void bulwark_scene_start_on_enter(void* context) {
    BulwarkApp* app = context;

    if(!app->splash_done) {
        splash_view_set_skip_callback(app->splash_view, bulwark_scene_start_skip_splash, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewSplash);
    } else {
        bulwark_scene_start_show_menu(app);
    }
}

bool bulwark_scene_start_on_event(void* context, SceneManagerEvent event) {
    BulwarkApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        if(!app->splash_done && splash_view_tick(app->splash_view)) {
            app->splash_done = true;
            bulwark_scene_start_show_menu(app);
        }
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BulwarkEventSkipSplash) {
        if(!app->splash_done) {
            app->splash_done = true;
            bulwark_scene_start_show_menu(app);
        }
        return true;
    }

    scene_manager_set_scene_state(app->scene_manager, BulwarkSceneStart, event.event);

    switch(event.event) {
    case StartIndexWatch:
        app->baseline_run = false;
        scene_manager_next_scene(app->scene_manager, BulwarkSceneWatch);
        return true;
    case StartIndexBaseline:
        app->baseline_run = true;
        scene_manager_next_scene(app->scene_manager, BulwarkSceneBaseline);
        return true;
    case StartIndexForget:
        memset(&app->settings.baseline, 0, sizeof(app->settings.baseline));
        memset(&app->watch.baseline, 0, sizeof(app->watch.baseline));
        bw_store_save(&app->settings);
        bulwark_click(app);
        scene_manager_set_scene_state(app->scene_manager, BulwarkSceneStart, StartIndexBaseline);
        bulwark_scene_start_show_menu(app);
        return true;
    case StartIndexLearn:
        scene_manager_next_scene(app->scene_manager, BulwarkSceneLearn);
        return true;
    case StartIndexLog:
        scene_manager_next_scene(app->scene_manager, BulwarkSceneLog);
        return true;
    case StartIndexSettings:
        scene_manager_next_scene(app->scene_manager, BulwarkSceneSettings);
        return true;
    case StartIndexAbout:
        scene_manager_next_scene(app->scene_manager, BulwarkSceneAbout);
        return true;
    default:
        return false;
    }
}

void bulwark_scene_start_on_exit(void* context) {
    BulwarkApp* app = context;
    submenu_reset(app->submenu);
}
