/* The screen you actually watch.
 *
 * Two pages. WALL is the band itself: six bars at their real spacing across
 * 2402 to 2480 MHz, the three advertising channels solid and the three Wi-Fi
 * centres drawn as texture, standing under a crenellated title bar. TREND is
 * the same story over time - one column per finished sweep - because "is it
 * following me" is a different question from "is it here now", and it is the
 * question that matters when you are walking away from something.
 */
#include "watch_view.h"
#include "wall_art.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <string.h>

#define WV_HDR_H 12
#define WV_CREN_Y 12
#define WV_CREN_H 3
#define WV_BAR_BASE 47
#define WV_BAR_H 30
#define WV_LABEL_BASE 55
#define WV_STATUS_BASE 63
#define WV_GRAPH_X 5

typedef enum {
    WatchPageWall = 0,
    WatchPageTrend,
    WatchPageCount,
} WatchPage;

struct WatchView {
    View* view;
    WatchViewCallback open_cb;
    void* open_ctx;
    WatchViewCallback reset_cb;
    void* reset_ctx;
};

typedef struct {
    BwWatchSnapshot snap;
    uint8_t page;
    uint8_t blink;
} WatchModel;

/* ------------------------------------------------------------------ header */

static void watch_draw_header(Canvas* canvas, const WatchModel* m) {
    const BwWatchSnapshot* s = &m->snap;

    canvas_draw_box(canvas, 0, 0, 128, WV_HDR_H);
    wall_art_crenellations(canvas, 0, WV_CREN_Y, 128, WV_CREN_H);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);

    const char* title;
    if(!s->supported) {
        title = "NO RF MODE";
    } else if(s->baseline_run) {
        title = "BASELINE";
    } else if(!s->have_result) {
        title = "LISTENING";
    } else {
        title = bw_verdict_name(s->verdict);
    }
    canvas_draw_str(canvas, 2, 10, title);

    if(s->have_result && !s->baseline_run) {
        char score[8];
        snprintf(score, sizeof(score), "%u", s->score > 99 ? 99 : s->score);
        canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, score);
        /* A SPAM LIKELY verdict gets a blinking marker: the number alone is
         * easy to walk past. */
        if(s->verdict == BwVerdictSpam && (m->blink & 4)) {
            canvas_draw_box(canvas, 96, 2, 3, 8);
        }
    } else if(s->supported) {
        char pct[8];
        snprintf(pct, sizeof(pct), "%u%%", s->progress_pct > 100 ? 100 : s->progress_pct);
        canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, pct);
    }

    canvas_set_color(canvas, ColorBlack);
}

/* ------------------------------------------------------------ status line */

static void watch_draw_status(Canvas* canvas, const WatchModel* m) {
    const BwWatchSnapshot* s = &m->snap;
    /* Sized for what the compiler can prove, not for what the values are:
     * -Werror=format-truncation assumes ten digits for every %lu. */
    char left[40];
    char right[24];

    canvas_set_font(canvas, FontSecondary);

    uint32_t sweeps = s->sweeps_done > 999 ? 999 : s->sweeps_done;
    uint16_t rate = s->rate_hz > 9999 ? 9999 : s->rate_hz;
    snprintf(left, sizeof(left), "%lus  #%lu  %uHz", (unsigned long)s->sweep_secs, (unsigned long)sweeps, rate);
    canvas_draw_str(canvas, 2, WV_STATUS_BASE, left);

    if(s->demo) {
        strncpy(right, "DEMO", sizeof(right) - 1);
        right[sizeof(right) - 1] = '\0';
    } else if(s->interference == BwInterferenceHeavy) {
        strncpy(right, "WIFI HIGH", sizeof(right) - 1);
        right[sizeof(right) - 1] = '\0';
    } else if(!s->baseline_taken) {
        strncpy(right, "NO BASE", sizeof(right) - 1);
        right[sizeof(right) - 1] = '\0';
    } else {
        snprintf(right, sizeof(right), "BASE  x%u", s->streak > 99 ? 99 : s->streak);
    }
    canvas_draw_str_aligned(canvas, 126, WV_STATUS_BASE, AlignRight, AlignBottom, right);
}

/* -------------------------------------------------------------- wall page */

static void watch_draw_wall(Canvas* canvas, const WatchModel* m) {
    const BwWatchSnapshot* s = &m->snap;

    /* What this place normally does, if the user ever showed Bulwark. A bar
     * standing above this line is the whole question the app exists to ask. */
    if(s->baseline_taken) {
        int y = WV_BAR_BASE - wall_art_bar_h(s->baseline_busy_ppt, WV_BAR_H);
        for(int x = 0; x < 128; x += 3) canvas_draw_dot(canvas, x, y);
    }

    wall_art_spectrum(canvas, s->busy_ppt, WV_BAR_BASE, WV_BAR_H, s->chan_now);
    canvas_draw_line(canvas, 0, WV_BAR_BASE + 1, 127, WV_BAR_BASE + 1);
    wall_art_spectrum_labels(canvas, WV_LABEL_BASE, s->chan_now);
}

/* ------------------------------------------------------------- trend page */

static void watch_draw_trend(Canvas* canvas, const WatchModel* m) {
    const BwWatchSnapshot* s = &m->snap;
    const int base = WV_BAR_BASE;

    /* Where the verdict bands fall, so a column's height means something
     * without having to read the number. */
    static const uint8_t marks[3] = {25, 45, 70};
    for(size_t i = 0; i < 3; i++) {
        int y = base - (marks[i] * WV_BAR_H) / 100;
        for(int x = 0; x < 128; x += 4) canvas_draw_dot(canvas, x, y);
    }

    uint16_t n = s->history_len;
    if(n > BW_HISTORY_LEN) n = BW_HISTORY_LEN;
    for(uint16_t i = 0; i < n; i++) {
        int h = ((int)s->history[i] * WV_BAR_H) / 100;
        int x = WV_GRAPH_X + i;
        if(x > 126) break;
        if(h <= 0) {
            canvas_draw_dot(canvas, x, base);
        } else {
            canvas_draw_line(canvas, x, base - h, x, base);
        }
    }

    canvas_draw_line(canvas, 0, base + 1, 127, base + 1);

    canvas_set_font(canvas, FontSecondary);
    if(n == 0) {
        canvas_draw_str(canvas, 2, WV_LABEL_BASE, "no sweeps yet");
    } else {
        char info[32];
        snprintf(
            info,
            sizeof(info),
            "peak %u  run %u",
            s->peak_score > 99 ? 99 : s->peak_score,
            s->streak > 99 ? 99 : s->streak);
        canvas_draw_str(canvas, 2, WV_LABEL_BASE, info);
        canvas_draw_str_aligned(canvas, 126, WV_LABEL_BASE, AlignRight, AlignBottom, "TREND");
    }
}

/* ------------------------------------------------------------- unsupported */

static void watch_draw_unsupported(Canvas* canvas) {
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(
        canvas,
        64,
        22,
        AlignCenter,
        AlignTop,
        "This Flipper's radio stack\nhas no RF test mode, so\nthere is no way to listen.\nDemo mode still works.");
}

static void watch_view_draw(Canvas* canvas, void* model) {
    WatchModel* m = model;
    canvas_clear(canvas);

    watch_draw_header(canvas, m);

    if(!m->snap.supported) {
        watch_draw_unsupported(canvas);
        return;
    }

    if(m->page == WatchPageTrend) {
        watch_draw_trend(canvas, m);
    } else {
        watch_draw_wall(canvas, m);
    }

    watch_draw_status(canvas, m);
}

static bool watch_view_input(InputEvent* event, void* context) {
    WatchView* v = context;

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft || event->key == InputKeyRight) {
            with_view_model(
                v->view,
                WatchModel * m,
                {
                    if(event->key == InputKeyRight) {
                        m->page = (uint8_t)((m->page + 1) % WatchPageCount);
                    } else {
                        m->page = (uint8_t)((m->page + WatchPageCount - 1) % WatchPageCount);
                    }
                },
                true);
            return true;
        }
        if(event->key == InputKeyOk) {
            if(v->open_cb) v->open_cb(v->open_ctx);
            return true;
        }
    }

    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(v->reset_cb) v->reset_cb(v->reset_ctx);
        return true;
    }

    return false;
}

WatchView* watch_view_alloc(void) {
    WatchView* v = malloc(sizeof(WatchView));
    memset(v, 0, sizeof(WatchView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, watch_view_draw);
    view_set_input_callback(v->view, watch_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(WatchModel));
    return v;
}

void watch_view_free(WatchView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* watch_view_get_view(WatchView* v) {
    furi_assert(v);
    return v->view;
}

void watch_view_update(WatchView* v, const BwWatchSnapshot* snap) {
    furi_assert(v);
    with_view_model(v->view, WatchModel * m, { m->snap = *snap; }, true);
}

void watch_view_tick(WatchView* v) {
    furi_assert(v);
    with_view_model(v->view, WatchModel * m, { m->blink++; }, true);
}

void watch_view_set_open_callback(WatchView* v, WatchViewCallback cb, void* context) {
    furi_assert(v);
    v->open_cb = cb;
    v->open_ctx = context;
}

void watch_view_set_reset_callback(WatchView* v, WatchViewCallback cb, void* context) {
    furi_assert(v);
    v->reset_cb = cb;
    v->reset_ctx = context;
}
