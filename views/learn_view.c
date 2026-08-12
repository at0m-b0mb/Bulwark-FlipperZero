/* Seven panels on how the attack works, why the Flipper can hear it, and what
 * it can never tell you. Left and Right move between them; each one animates
 * on the tick.
 *
 * The last panel is the honest one. It is there on purpose.
 */
#include "learn_view.h"
#include "wall_art.h"

#include "../helpers/bw_chan.h"

#include <furi.h>
#include <gui/gui.h>
#include <string.h>

#define LV_ART_Y 16
#define LV_ART_H 22
#define LV_TEXT1 45
#define LV_TEXT2 53
#define LV_TEXT3 61

typedef struct {
    const char* title;
    const char* line[3];
} LearnPanel;

static const LearnPanel panels[] = {
    {
        "THE POPUP FLOOD",
        {
            "Your phone keeps asking",
            "to pair with headphones",
            "that do not exist.",
        },
    },
    {
        "WHY PHONES LISTEN",
        {
            "Bluetooth devices shout",
            "'I am here' several times",
            "a second. Phones listen.",
        },
    },
    {
        "ONLY THREE CHANNELS",
        {
            "All that shouting uses",
            "2402, 2426, 2480 MHz -",
            "the gaps between Wi-Fi.",
        },
    },
    {
        "WHAT A FLOOD LOOKS LIKE",
        {
            "A room: busy a few %.",
            "A spammer: a third of the",
            "time, on all three.",
        },
    },
    {
        "WHAT BULWARK HEARS",
        {
            "Test mode measures energy",
            "and decodes nothing. No",
            "address, name or payload.",
        },
    },
    {
        "TURNING IT OFF",
        {
            "Android: stop Fast Pair",
            "scanning. Windows: stop",
            "Swift Pair. iOS: BT off.",
        },
    },
    {
        "THE HONEST LIMIT",
        {
            "Sixty shop beacons look",
            "the same from outside",
            "the packets. Never 100.",
        },
    },
};

#define LV_PANELS (sizeof(panels) / sizeof(panels[0]))

struct LearnView {
    View* view;
};

typedef struct {
    uint8_t panel;
    uint8_t anim;
} LearnModel;

/* -------------------------------------------------------------- panel art */

/** A phone, drawn once and reused. */
static void lv_phone(Canvas* canvas, int x, int y) {
    canvas_draw_rframe(canvas, x, y, 14, 22, 2);
    canvas_draw_line(canvas, x + 4, y + 2, x + 9, y + 2);
    canvas_draw_dot(canvas, x + 7, y + 19);
}

static void lv_panel_flood(Canvas* canvas, uint8_t anim) {
    lv_phone(canvas, 8, LV_ART_Y);

    /* Popup cards arriving and stacking up on the phone. */
    int arrived = (anim / 3) % 5;
    for(int i = 0; i < arrived; i++) {
        int x = 30 + i * 3;
        int y = LV_ART_Y + 1 + i * 4;
        canvas_draw_rframe(canvas, x, y, 60 - i * 2, 5, 1);
        canvas_draw_line(canvas, x + 2, y + 2, x + 20, y + 2);
    }
    if(arrived < 5) {
        int x = 118 - (anim % 3) * 9;
        canvas_draw_rframe(canvas, x, LV_ART_Y + 1 + arrived * 4, 10, 5, 1);
    }
}

static void lv_panel_listen(Canvas* canvas, uint8_t anim) {
    lv_phone(canvas, 57, LV_ART_Y);

    /* Two devices shouting; the phone in the middle hears them. */
    for(int side = 0; side < 2; side++) {
        int dir = side ? 1 : -1;
        int origin = side ? 108 : 20;
        canvas_draw_box(canvas, origin - 3, LV_ART_Y + 8, 7, 7);
        for(int w = 0; w < 3; w++) {
            int phase = (anim + w * 2) % 6;
            int d = 7 + phase * 3;
            int x = origin + dir * d;
            if(x < 34 || x > 94) continue;
            canvas_draw_line(canvas, x, LV_ART_Y + 6, x, LV_ART_Y + 16);
        }
    }
}

static void lv_panel_channels(Canvas* canvas, uint8_t anim) {
    const int base = LV_ART_Y + LV_ART_H - 3;

    /* Wi-Fi 1, 6 and 11 as wide, soft blocks behind the advertising spikes.
     * That is the whole reason those three frequencies were chosen. */
    for(size_t r = 0; r < BW_REF_COUNT; r++) {
        int cx = wall_art_chan_x(bw_chan_ref[r]) + WALL_BAR_W / 2;
        for(int y = base - 8; y < base; y += 2) {
            canvas_draw_line(canvas, cx - 13, y, cx + 13, y);
        }
    }

    canvas_draw_line(canvas, 0, base, 127, base);
    for(int i = 0; i <= 39; i++) {
        int x = 4 + (i * 104) / 39;
        canvas_draw_dot(canvas, x, base - 1);
    }

    for(size_t a = 0; a < BW_ADV_COUNT; a++) {
        int cx = wall_art_chan_x(bw_chan_adv[a]) + WALL_BAR_W / 2;
        int h = 14 + (((anim / 2) + (int)a) % 2) * 2;
        canvas_draw_box(canvas, cx - 1, base - h, 3, h);
    }
}

static void lv_panel_duty(Canvas* canvas, uint8_t anim) {
    /* Two timelines scrolling past: what a room does, and what a spammer
     * does. Same axis, same speed. */
    static const uint8_t quiet[16] = {0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0};
    static const uint8_t flood[16] = {1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1};

    for(int row = 0; row < 2; row++) {
        const uint8_t* pat = row ? flood : quiet;
        int y = LV_ART_Y + row * 12;
        canvas_draw_line(canvas, 26, y + 8, 127, y + 8);
        for(int i = 0; i < 26; i++) {
            int idx = (i + anim / 2) % 16;
            if(!pat[idx]) continue;
            int x = 27 + i * 4;
            if(x > 126) break;
            canvas_draw_box(canvas, x, y + 2, 3, 6);
        }
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, LV_ART_Y + 8, "room");
    canvas_draw_str(canvas, 0, LV_ART_Y + 20, "spam");
}

static void lv_panel_hears(Canvas* canvas, uint8_t anim) {
    wall_art_shield(canvas, 4, LV_ART_Y - 1, false);

    /* Energy arrives and is counted. A packet arrives and bounces off. */
    for(int w = 0; w < 3; w++) {
        int phase = (anim + w * 2) % 6;
        int x = 100 - phase * 6;
        if(x < 40) continue;
        canvas_draw_line(canvas, x, LV_ART_Y + 3, x, LV_ART_Y + 13);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 40, LV_ART_Y + 21, "energy: yes");
    canvas_draw_str(canvas, 96, LV_ART_Y + 21, "bits: no");
}

static void lv_panel_off(Canvas* canvas, uint8_t anim) {
    static const char* const os[3] = {"Android", "Windows", "iOS"};
    for(int i = 0; i < 3; i++) {
        int y = LV_ART_Y + i * 7;
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, y + 6, os[i]);

        int x = 60;
        bool off = ((anim / 4) % 4) > (unsigned)i;
        canvas_draw_rframe(canvas, x, y, 18, 7, 3);
        canvas_draw_box(canvas, off ? x + 1 : x + 10, y + 1, 7, 5);
        canvas_draw_str(canvas, x + 22, y + 6, off ? "off" : "on");
    }
}

static void lv_panel_limit(Canvas* canvas, uint8_t anim) {
    /* The same picture twice, with a different label. That is the point. */
    static const uint16_t both[BwChanCount] = {700, 120, 740, 130, 110, 690};
    UNUSED(anim);

    canvas_set_font(canvas, FontSecondary);
    for(int side = 0; side < 2; side++) {
        int ox = side * 66;
        for(size_t c = 0; c < BwChanCount; c++) {
            int h = (both[c] * 14) / 1000;
            int x = ox + 2 + (int)c * 8;
            if(bw_chan_is_adv((BwChan)c)) {
                canvas_draw_box(canvas, x, LV_ART_Y + 14 - h, 5, h);
            } else {
                for(int y = LV_ART_Y + 14 - h; y < LV_ART_Y + 14; y += 2) {
                    canvas_draw_line(canvas, x, y, x + 4, y);
                }
            }
        }
        canvas_draw_line(canvas, ox + 1, LV_ART_Y + 15, ox + 50, LV_ART_Y + 15);
        canvas_draw_str(canvas, ox + 2, LV_ART_Y + 22, side ? "attack" : "shop");
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 56, LV_ART_Y + 12, "=");
}

/* --------------------------------------------------------------- drawing */

static void learn_view_draw(Canvas* canvas, void* model) {
    LearnModel* m = model;
    const LearnPanel* p = &panels[m->panel % LV_PANELS];

    canvas_clear(canvas);

    canvas_draw_box(canvas, 0, 0, 128, 12);
    wall_art_crenellations(canvas, 0, 12, 128, 3);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, p->title);
    char pos[8];
    snprintf(pos, sizeof(pos), "%u/%u", (unsigned)(m->panel + 1), (unsigned)LV_PANELS);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, pos);
    canvas_set_color(canvas, ColorBlack);

    switch(m->panel) {
    case 0:
        lv_panel_flood(canvas, m->anim);
        break;
    case 1:
        lv_panel_listen(canvas, m->anim);
        break;
    case 2:
        lv_panel_channels(canvas, m->anim);
        break;
    case 3:
        lv_panel_duty(canvas, m->anim);
        break;
    case 4:
        lv_panel_hears(canvas, m->anim);
        break;
    case 5:
        lv_panel_off(canvas, m->anim);
        break;
    default:
        lv_panel_limit(canvas, m->anim);
        break;
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, LV_TEXT1, p->line[0]);
    canvas_draw_str(canvas, 2, LV_TEXT2, p->line[1]);
    canvas_draw_str(canvas, 2, LV_TEXT3, p->line[2]);
}

static bool learn_view_input(InputEvent* event, void* context) {
    LearnView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key != InputKeyLeft && event->key != InputKeyRight) return false;

    with_view_model(
        v->view,
        LearnModel * m,
        {
            if(event->key == InputKeyRight) {
                m->panel = (uint8_t)((m->panel + 1) % LV_PANELS);
            } else {
                m->panel = (uint8_t)((m->panel + LV_PANELS - 1) % LV_PANELS);
            }
            m->anim = 0;
        },
        true);
    return true;
}

LearnView* learn_view_alloc(void) {
    LearnView* v = malloc(sizeof(LearnView));
    memset(v, 0, sizeof(LearnView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, learn_view_draw);
    view_set_input_callback(v->view, learn_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(LearnModel));
    return v;
}

void learn_view_free(LearnView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* learn_view_get_view(LearnView* v) {
    furi_assert(v);
    return v->view;
}

void learn_view_tick(LearnView* v) {
    furi_assert(v);
    with_view_model(v->view, LearnModel * m, { m->anim++; }, true);
}

void learn_view_reset(LearnView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        LearnModel * m,
        {
            m->panel = 0;
            m->anim = 0;
        },
        true);
}
