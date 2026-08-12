#include "bulwark_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
void (*const bulwark_on_enter_handlers[])(void*) = {
#include "bulwark_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
bool (*const bulwark_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "bulwark_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
void (*const bulwark_on_exit_handlers[])(void* context) = {
#include "bulwark_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers bulwark_scene_handlers = {
    .on_enter_handlers = bulwark_on_enter_handlers,
    .on_event_handlers = bulwark_on_event_handlers,
    .on_exit_handlers = bulwark_on_exit_handlers,
    .scene_num = BulwarkSceneNum,
};
