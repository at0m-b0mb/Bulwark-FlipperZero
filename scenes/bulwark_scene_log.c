#include "../bulwark_i.h"

/* Every sweep, one row, on the SD card, with the per-channel occupancy that
 * produced the verdict. The point is that the arithmetic can be checked in a
 * spreadsheet afterwards by somebody who does not trust the app. */

typedef enum {
    LogEventClear = 300,
} LogEvent;

static void bulwark_scene_log_button_cb(GuiButtonType result, InputType type, void* context) {
    BulwarkApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, LogEventClear);
    }
}

static void bulwark_scene_log_show(BulwarkApp* app) {
    Widget* widget = app->widget;
    int32_t rows = bw_store_log_rows();

    widget_reset(widget);
    widget_add_string_element(widget, 64, 11, AlignCenter, AlignBottom, FontPrimary, "Sweep log");

    char line[48];
    if(rows < 0) {
        widget_add_string_multiline_element(
            widget,
            64,
            26,
            AlignCenter,
            AlignTop,
            FontSecondary,
            "No log yet.\nTurn logging on in Settings\nand watch the band.");
    } else {
        snprintf(line, sizeof(line), "%ld sweeps recorded", (long)rows);
        widget_add_string_element(widget, 64, 26, AlignCenter, AlignBottom, FontSecondary, line);
        widget_add_string_multiline_element(
            widget,
            64,
            32,
            AlignCenter,
            AlignTop,
            FontSecondary,
            "/ext/apps_data/bulwark/\nsweeps.csv");
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Clear", bulwark_scene_log_button_cb, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewWidget);
}

void bulwark_scene_log_on_enter(void* context) {
    BulwarkApp* app = context;
    bulwark_scene_log_show(app);
}

bool bulwark_scene_log_on_event(void* context, SceneManagerEvent event) {
    BulwarkApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == LogEventClear) {
        bw_store_log_clear();
        bulwark_click(app);
        bulwark_scene_log_show(app);
        return true;
    }
    return false;
}

void bulwark_scene_log_on_exit(void* context) {
    BulwarkApp* app = context;
    widget_reset(app->widget);
}
