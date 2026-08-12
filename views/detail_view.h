#pragma once

#include "../helpers/bw_score.h"

#include <gui/view.h>

typedef struct DetailView DetailView;

DetailView* detail_view_alloc(void);
void detail_view_free(DetailView* v);
View* detail_view_get_view(DetailView* v);

/** Rebuild the breakdown from a scored sweep. */
void detail_view_update(DetailView* v, const BwScore* score, const BwSweep* sweep, bool demo);
