#pragma once

#include <gui/view.h>

typedef struct LearnView LearnView;

LearnView* learn_view_alloc(void);
void learn_view_free(LearnView* v);
View* learn_view_get_view(LearnView* v);

/** Advance the animation on the current panel. */
void learn_view_tick(LearnView* v);

/** Start again at the first panel. */
void learn_view_reset(LearnView* v);
