#include "../bulwark_i.h"

/* The breakdown is a snapshot of the sweep you opened, and it stays that way
 * while you read it. The session behind it keeps running, because walking
 * away from the screen is not the same as walking away from the band. */

void bulwark_scene_detail_on_enter(void* context) {
    BulwarkApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewDetail);
}

bool bulwark_scene_detail_on_event(void* context, SceneManagerEvent event) {
    BulwarkApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == BulwarkEventSweepDone) {
        bulwark_consume_sweep(app);
        return true;
    }
    return false;
}

void bulwark_scene_detail_on_exit(void* context) {
    UNUSED(context);
}
