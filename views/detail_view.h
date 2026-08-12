#pragma once

#include "../helpers/bw_score.h"

#include <gui/view.h>

typedef struct DetailView DetailView;

DetailView* detail_view_alloc(void);
void detail_view_free(DetailView* v);
View* detail_view_get_view(DetailView* v);

/**
 * Rebuild the breakdown from a scored sweep.
 *
 * @param demo_name   the synthetic band these numbers came from, or NULL when
 *                    they came from the antenna. Passing it puts the band and
 *                    its description on the same screen as the verdict, so a
 *                    demo screenshot cannot be mistaken for a measurement.
 * @param demo_blurb  that band's one-line description, or NULL.
 */
void detail_view_update(
    DetailView* v,
    const BwScore* score,
    const BwSweep* sweep,
    const char* demo_name,
    const char* demo_blurb);
