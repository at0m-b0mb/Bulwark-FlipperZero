#include "../bulwark_i.h"

void bulwark_scene_learn_on_enter(void* context) {
    BulwarkApp* app = context;
    learn_view_reset(app->learn_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, BulwarkViewLearn);
}

bool bulwark_scene_learn_on_event(void* context, SceneManagerEvent event) {
    BulwarkApp* app = context;
    if(event.type == SceneManagerEventTypeTick) {
        learn_view_tick(app->learn_view);
        return true;
    }
    return false;
}

void bulwark_scene_learn_on_exit(void* context) {
    UNUSED(context);
}
