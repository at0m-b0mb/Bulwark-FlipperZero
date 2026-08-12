/* The whole of the reasoning, in the order it matters.
 *
 * Why the score is not higher comes first, because that is the sentence the
 * user needs and the one an app like this is usually too proud to print.
 * Then every signal and what it was worth, then every ceiling that was in
 * force, then the raw numbers so the arithmetic can be checked by hand.
 */
#include "detail_view.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <string.h>
#include <stdio.h>

#define DV_ROWS_MAX 56
#define DV_LEFT_LEN 26
/* Wide enough for what -Werror=format-truncation believes an int can print,
 * not for what these values actually are. */
#define DV_RIGHT_LEN 20
#define DV_VISIBLE 5
#define DV_ROW_H 9
#define DV_FIRST_BASE 23
#define DV_WRAP 25

typedef enum {
    DvRowSection = 0,
    DvRowSignal,
    DvRowCap,
    DvRowFact,
    DvRowPara,
} DvRowType;

typedef struct {
    uint8_t type;
    char left[DV_LEFT_LEN];
    char right[DV_RIGHT_LEN];
} DvRow;

typedef struct {
    DvRow row[DV_ROWS_MAX];
    uint16_t count;
    uint16_t top;
    char title[16];
    char score[12];
} DetailModel;

struct DetailView {
    View* view;
};

/* ---------------------------------------------------------------- building */

static void dv_add(DetailModel* m, DvRowType type, const char* left, const char* right) {
    if(m->count >= DV_ROWS_MAX) return;
    DvRow* r = &m->row[m->count++];
    r->type = (uint8_t)type;
    strncpy(r->left, left ? left : "", DV_LEFT_LEN - 1);
    r->left[DV_LEFT_LEN - 1] = '\0';
    strncpy(r->right, right ? right : "", DV_RIGHT_LEN - 1);
    r->right[DV_RIGHT_LEN - 1] = '\0';
}

/** Break a sentence into screen-width lines without cutting words in half. */
static void dv_add_paragraph(DetailModel* m, const char* text) {
    char line[DV_WRAP + 1];
    size_t len = 0;
    size_t last_space = 0;

    for(const char* p = text;; p++) {
        if(*p != '\0' && len < DV_WRAP) {
            if(*p == ' ') last_space = len;
            line[len++] = *p;
            continue;
        }

        size_t cut = len;
        const char* next = p;
        if(*p != '\0') {
            /* Mid-word: back up to the last space we passed. */
            if(*p != ' ' && last_space > 0) {
                cut = last_space;
                next = p - (len - last_space);
            }
        }

        line[cut] = '\0';
        dv_add(m, DvRowPara, line, NULL);
        if(*p == '\0') break;

        while(*next == ' ') next++;
        p = next - 1;
        len = 0;
        last_space = 0;
    }
}

static void dv_pct(char* out, size_t size, uint16_t ppt) {
    if(ppt > 1000) ppt = 1000;
    snprintf(out, size, "%u.%u%%", ppt / 10u, ppt % 10u);
}

void detail_view_update(DetailView* v, const BwScore* score, const BwSweep* sweep, bool demo) {
    furi_assert(v);

    with_view_model(
        v->view,
        DetailModel * m,
        {
            memset(m, 0, sizeof(DetailModel));
            char buf[DV_LEFT_LEN];
            char val[DV_RIGHT_LEN];

            strncpy(m->title, bw_verdict_name(score->verdict), sizeof(m->title) - 1);
            snprintf(m->score, sizeof(m->score), "%u/%u", score->score, BW_SCORE_CEILING);

            if(!score->have_data) {
                dv_add(m, DvRowSection, "NO DATA", NULL);
                dv_add_paragraph(
                    m,
                    "Not enough samples to describe the band. Let a sweep "
                    "finish before asking.");
            } else {
                /* 1. Why it is not higher. */
                dv_add(m, DvRowSection, "WHY NOT HIGHER", NULL);
                if(score->binding_cap == BwCapNone) {
                    dv_add_paragraph(m, "Nothing held this score down. It is the evidence.");
                } else {
                    dv_add(m, DvRowCap, bw_cap_name(score->binding_cap), "held");
                    dv_add_paragraph(m, bw_cap_reason(score->binding_cap));
                }

                /* 2. Every signal, and what it was worth. */
                for(size_t f = 0; f < BwFamilyCount; f++) {
                    snprintf(
                        val, sizeof(val), "%u/%u", score->family_points[f], score->family_max[f]);
                    dv_add(m, DvRowSection, bw_family_name((BwFamily)f), val);
                    for(size_t i = 0; i < BwSigCount; i++) {
                        if(score->sig[i].family != (BwFamily)f) continue;
                        if(score->sig[i].points) {
                            snprintf(val, sizeof(val), "+%u", score->sig[i].points);
                        } else {
                            strncpy(val, "-", sizeof(val) - 1);
                            val[sizeof(val) - 1] = '\0';
                        }
                        dv_add(m, DvRowSignal, score->sig[i].name, val);
                        snprintf(buf, sizeof(buf), "  %s", score->sig[i].detail);
                        dv_add(m, DvRowPara, buf, NULL);
                    }
                }

                /* 3. Every ceiling in force, whether or not it bit. */
                dv_add(m, DvRowSection, "CEILINGS", NULL);
                for(size_t c = 1; c < BwCapCount; c++) {
                    if(!(score->caps_applied & (1u << c))) continue;
                    dv_add(
                        m,
                        DvRowCap,
                        bw_cap_name((BwCap)c),
                        (score->binding_cap == (BwCap)c) ? "held" : "");
                }

                /* 4. The arithmetic, so it can be checked. */
                dv_add(m, DvRowSection, "MEASURED", NULL);
                snprintf(val, sizeof(val), "%u", score->raw_score);
                dv_add(m, DvRowFact, "Raw score", val);
                dv_pct(val, sizeof(val), sweep->adv_busy_ppt);
                dv_add(m, DvRowFact, "Adv busy", val);
                dv_pct(val, sizeof(val), sweep->ref_busy_ppt);
                dv_add(m, DvRowFact, "Wi-Fi busy", val);
                for(size_t i = 0; i < BW_ADV_COUNT; i++) {
                    BwChan ch = bw_chan_adv[i];
                    snprintf(buf, sizeof(buf), "  ch %s (%u MHz)", bw_chan_info[ch].label, bw_chan_info[ch].mhz);
                    dv_pct(val, sizeof(val), sweep->chan[ch].busy_ppt);
                    dv_add(m, DvRowFact, buf, val);
                }
                snprintf(val, sizeof(val), "%d dBm", sweep->adv_peak_dbm);
                dv_add(m, DvRowFact, "Loudest adv", val);
                snprintf(val, sizeof(val), "%d dBm", sweep->band_floor_dbm);
                dv_add(m, DvRowFact, "Band floor", val);
                snprintf(val, sizeof(val), "%u dB", sweep->adv_swing_db);
                dv_add(m, DvRowFact, "Peak swing", val);
                dv_add(m, DvRowFact, "Interference", bw_interference_name(score->interference));
                snprintf(val, sizeof(val), "%lu", (unsigned long)sweep->samples);
                dv_add(m, DvRowFact, "Samples", val);
                snprintf(val, sizeof(val), "%u Hz", sweep->rate_hz);
                dv_add(m, DvRowFact, "Sample rate", val);
                dv_add(m, DvRowFact, "Source", demo ? "demo" : "radio");

                /* 5. The thing the app cannot do, on the same screen as the
                 *    thing it just did. */
                dv_add(m, DvRowSection, "NOT MEASURED", NULL);
                dv_add_paragraph(
                    m,
                    "Addresses, names, payloads, which attack it is, how many "
                    "devices. The radio is in test mode and decodes nothing.");
            }
        },
        true);
}

/* ---------------------------------------------------------------- drawing */

static void detail_view_draw(Canvas* canvas, void* model) {
    DetailModel* m = model;
    canvas_clear(canvas);

    /* Fixed header: the answer, always on screen while you read the why. */
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, m->title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, m->score);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    for(uint16_t i = 0; i < DV_VISIBLE; i++) {
        uint16_t idx = (uint16_t)(m->top + i);
        if(idx >= m->count) break;
        const DvRow* r = &m->row[idx];
        int base = DV_FIRST_BASE + i * DV_ROW_H;

        if(r->type == DvRowSection) {
            canvas_draw_box(canvas, 0, base - 7, 122, 8);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 2, base, r->left);
            if(r->right[0]) {
                canvas_draw_str_aligned(canvas, 120, base, AlignRight, AlignBottom, r->right);
            }
            canvas_set_color(canvas, ColorBlack);
            continue;
        }

        canvas_draw_str(canvas, 2, base, r->left);
        if(r->right[0]) {
            canvas_draw_str_aligned(canvas, 120, base, AlignRight, AlignBottom, r->right);
        }
    }

    if(m->count > DV_VISIBLE) {
        elements_scrollbar_pos(canvas, 126, 14, 50, m->top, (uint16_t)(m->count - DV_VISIBLE + 1));
    }
}

static bool detail_view_input(InputEvent* event, void* context) {
    DetailView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool handled = false;
    with_view_model(
        v->view,
        DetailModel * m,
        {
            uint16_t max_top = (m->count > DV_VISIBLE) ? (uint16_t)(m->count - DV_VISIBLE) : 0;
            if(event->key == InputKeyDown) {
                if(m->top < max_top) m->top++;
                handled = true;
            } else if(event->key == InputKeyUp) {
                if(m->top > 0) m->top--;
                handled = true;
            } else if(event->key == InputKeyRight) {
                m->top = (uint16_t)((m->top + DV_VISIBLE > max_top) ? max_top : m->top + DV_VISIBLE);
                handled = true;
            } else if(event->key == InputKeyLeft) {
                m->top = (uint16_t)((m->top < DV_VISIBLE) ? 0 : m->top - DV_VISIBLE);
                handled = true;
            }
        },
        true);
    return handled;
}

DetailView* detail_view_alloc(void) {
    DetailView* v = malloc(sizeof(DetailView));
    memset(v, 0, sizeof(DetailView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, detail_view_draw);
    view_set_input_callback(v->view, detail_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(DetailModel));
    return v;
}

void detail_view_free(DetailView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* detail_view_get_view(DetailView* v) {
    furi_assert(v);
    return v->view;
}
