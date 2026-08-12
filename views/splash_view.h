#pragma once

#include <gui/view.h>

typedef struct SplashView SplashView;
typedef void (*SplashViewCallback)(void* context);

SplashView* splash_view_alloc(void);
void splash_view_free(SplashView* v);
View* splash_view_get_view(SplashView* v);

/** Advance the animation. Returns true when it has played out. */
bool splash_view_tick(SplashView* v);

/** Any key skips it. */
void splash_view_set_skip_callback(SplashView* v, SplashViewCallback cb, void* context);
