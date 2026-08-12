/* Three volleys of advertising arrive from off-screen and break against a
 * wall that builds itself out of the ground while they are in flight. About
 * a second and a half; any key skips it. */
#include "splash_view.h"
#include "wall_art.h"

#include <furi.h>
#include <gui/gui.h>
#include <string.h>

#define SPL_BUILD 7 /* ticks for the rampart to finish building */
#define SPL_TOTAL 17 /* ticks before the scene moves on by itself */

#define SPL_WALL_Y 29
#define SPL_WALL_H 12
#define SPL_SHIELD_X 50
#define SPL_SHIELD_Y 2
#define SPL_TITLE_BASE 53
#define SPL_TAG_BASE 63

struct SplashView {
    View* view;
    SplashViewCallback skip_cb;
    void* skip_ctx;
};

typedef struct {
    uint8_t anim;
} SplashModel;

static void splash_view_draw(Canvas* canvas, void* model) {
    SplashModel* m = model;
    canvas_clear(canvas);

    /* The wall grows out of the floor, left to right. */
    int built = (int)m->anim * 128 / SPL_BUILD;
    if(built > 128) built = 128;
    if(built > 0) {
        canvas_draw_box(canvas, 0, SPL_WALL_Y + 4, built, SPL_WALL_H - 4);
        wall_art_crenellations(canvas, 0, SPL_WALL_Y, built, 4);
        /* Courses of stone, so it reads as masonry and not a black bar. */
        canvas_set_color(canvas, ColorWhite);
        for(int x = 4; x < built; x += 12) {
            canvas_draw_line(canvas, x, SPL_WALL_Y + 8, x, SPL_WALL_Y + SPL_WALL_H - 1);
        }
        canvas_draw_line(canvas, 0, SPL_WALL_Y + 7, built - 1, SPL_WALL_Y + 7);
        canvas_set_color(canvas, ColorBlack);
    }

    if(m->anim >= 2) {
        wall_art_shield(canvas, SPL_SHIELD_X, SPL_SHIELD_Y, false);
    }

    /* Volleys, arriving from both sides and stopping dead at the wall. */
    if(m->anim >= 3 && m->anim < SPL_TOTAL - 2) {
        int t = (int)m->anim - 3;
        for(int i = 0; i < 3; i++) {
            int phase = (t + i * 2) % 7;
            int x = 6 + phase * 5;
            int y = 12 + i * 6;
            if(phase < 6) {
                wall_art_arrow(canvas, x, y, 1, 0);
                wall_art_arrow(canvas, 127 - x, y, -1, 0);
            }
        }
    }

    if(m->anim >= SPL_BUILD) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, SPL_TITLE_BASE, AlignCenter, AlignBottom, "BULWARK");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, SPL_TAG_BASE, AlignCenter, AlignBottom, "Hears the popup flood");
    }
}

static bool splash_view_input(InputEvent* event, void* context) {
    SplashView* v = context;
    if(event->type == InputTypeShort || event->type == InputTypePress) {
        if(v->skip_cb) v->skip_cb(v->skip_ctx);
        return true;
    }
    return false;
}

SplashView* splash_view_alloc(void) {
    SplashView* v = malloc(sizeof(SplashView));
    memset(v, 0, sizeof(SplashView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, splash_view_draw);
    view_set_input_callback(v->view, splash_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SplashModel));
    return v;
}

void splash_view_free(SplashView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* splash_view_get_view(SplashView* v) {
    furi_assert(v);
    return v->view;
}

bool splash_view_tick(SplashView* v) {
    furi_assert(v);
    bool done = false;
    with_view_model(
        v->view,
        SplashModel * m,
        {
            if(m->anim < SPL_TOTAL) m->anim++;
            done = m->anim >= SPL_TOTAL;
        },
        true);
    return done;
}

void splash_view_set_skip_callback(SplashView* v, SplashViewCallback cb, void* context) {
    furi_assert(v);
    v->skip_cb = cb;
    v->skip_ctx = context;
}
