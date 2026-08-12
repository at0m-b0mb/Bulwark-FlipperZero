#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) BulwarkScene##id,
typedef enum {
#include "bulwark_scene_config.h"
    BulwarkSceneNum,
} BulwarkScene;
#undef ADD_SCENE

extern const SceneManagerHandlers bulwark_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                               \
    void prefix##_scene_##name##_on_enter(void* context);                         \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "bulwark_scene_config.h"
#undef ADD_SCENE
