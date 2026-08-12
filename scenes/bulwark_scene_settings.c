#include "../bulwark_i.h"

/* Demo mode sits in here rather than in its own menu entry, because it is not
 * a feature - it is a switch that says "the numbers you are about to see did
 * not come from the antenna", and the watch screen says DEMO the whole time
 * it is on. */

static const char* const alert_names[] = {"Off", "On"};
static const char* const log_names[] = {"Off", "On"};
static const char* const light_names[] = {"Auto", "Stay on"};

static void bulwark_settings_sweep_cb(VariableItem* item) {
    BulwarkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.sweep_len = index;
    variable_item_set_current_value_text(item, bw_sweep_len_name(index));
}

static void bulwark_settings_alert_cb(VariableItem* item) {
    BulwarkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.alert = index != 0;
    variable_item_set_current_value_text(item, alert_names[index]);
}

static void bulwark_settings_log_cb(VariableItem* item) {
    BulwarkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.logging = index != 0;
    variable_item_set_current_value_text(item, log_names[index]);
}

static void bulwark_settings_light_cb(VariableItem* item) {
    BulwarkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->settings.keep_screen_on = index != 0;
    variable_item_set_current_value_text(item, light_names[index]);
}

static void bulwark_settings_demo_cb(VariableItem* item) {
    BulwarkApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index == 0) {
        app->demo = false;
        variable_item_set_current_value_text(item, "Off");
    } else {
        app->demo = true;
        app->demo_scene = (BwDemoScene)(index - 1);
        variable_item_set_current_value_text(item, bw_demo_name(app->demo_scene));
    }
}

void bulwark_scene_settings_on_enter(void* context) {
    BulwarkApp* app = context;
    VariableItemList* list = app->var_item_list;
    VariableItem* item;

    variable_item_list_reset(list);

    item = variable_item_list_add(
        list, "Sweep", BwSweepLenCount, bulwark_settings_sweep_cb, app);
    variable_item_set_current_value_index(item, app->settings.sweep_len);
    variable_item_set_current_value_text(item, bw_sweep_len_name(app->settings.sweep_len));

    item = variable_item_list_add(list, "Alerts", 2, bulwark_settings_alert_cb, app);
    variable_item_set_current_value_index(item, app->settings.alert ? 1 : 0);
    variable_item_set_current_value_text(item, alert_names[app->settings.alert ? 1 : 0]);

    item = variable_item_list_add(list, "Log sweeps", 2, bulwark_settings_log_cb, app);
    variable_item_set_current_value_index(item, app->settings.logging ? 1 : 0);
    variable_item_set_current_value_text(item, log_names[app->settings.logging ? 1 : 0]);

    item = variable_item_list_add(list, "Backlight", 2, bulwark_settings_light_cb, app);
    variable_item_set_current_value_index(item, app->settings.keep_screen_on ? 1 : 0);
    variable_item_set_current_value_text(item, light_names[app->settings.keep_screen_on ? 1 : 0]);

    item = variable_item_list_add(
        list, "Demo band", (uint8_t)(BwDemoCount + 1), bulwark_settings_demo_cb, app);
    uint8_t demo_index = app->demo ? (uint8_t)(app->demo_scene + 1) : 0;
    variable_item_set_current_value_index(item, demo_index);
    variable_item_set_current_value_text(
        item, app->demo ? bw_demo_name(app->demo_scene) : "Off");

    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, BulwarkSceneSettings));

    view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewSettings);
}

bool bulwark_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bulwark_scene_settings_on_exit(void* context) {
    BulwarkApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        BulwarkSceneSettings,
        variable_item_list_get_selected_item_index(app->var_item_list));
    variable_item_list_reset(app->var_item_list);
    bw_store_save(&app->settings);
}
