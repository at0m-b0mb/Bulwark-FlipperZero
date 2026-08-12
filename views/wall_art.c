#include "wall_art.h"

#include <furi.h>
#include <string.h>

/* The band runs 2402..2480 MHz. Laying the bars out at their real spacing
 * costs nothing and makes the picture true: 37 and 39 really are at the two
 * ends, and Wi-Fi 11 really does sit between 38 and 39. */
#define WALL_SPAN_X 4
#define WALL_SPAN_W 104
#define WALL_MHZ_LO 2402
#define WALL_MHZ_HI 2480

uint8_t wall_art_chan_x(BwChan chan) {
    if(chan >= BwChanCount) return WALL_SPAN_X;
    int off = bw_chan_info[chan].mhz - WALL_MHZ_LO;
    return (uint8_t)(WALL_SPAN_X + (off * WALL_SPAN_W) / (WALL_MHZ_HI - WALL_MHZ_LO));
}

void wall_art_crenellations(Canvas* canvas, int x, int y, int w, int h) {
    for(int i = x; i < x + w; i += 8) {
        int block = (i + 4 <= x + w) ? 4 : (x + w - i);
        canvas_draw_box(canvas, i, y, block, h);
    }
}

void wall_art_shield(Canvas* canvas, int x, int y, bool filled) {
    const int w = 28;
    const int shoulder = 14; /* where the sides start to close in */

    if(filled) {
        canvas_draw_box(canvas, x + 1, y + 1, w - 1, shoulder);
        /* The taper, one shrinking row at a time. */
        for(int i = 0; i < 12; i++) {
            int inset = (i * (w / 2)) / 12;
            canvas_draw_line(
                canvas, x + 1 + inset, y + shoulder + i, x + w - 1 - inset, y + shoulder + i);
        }
    }

    canvas_draw_line(canvas, x, y, x + w, y);
    canvas_draw_line(canvas, x, y, x, y + shoulder);
    canvas_draw_line(canvas, x + w, y, x + w, y + shoulder);
    canvas_draw_line(canvas, x, y + shoulder, x + w / 2, y + shoulder + 12);
    canvas_draw_line(canvas, x + w, y + shoulder, x + w / 2, y + shoulder + 12);

    if(!filled) {
        /* Three bars inside: 37, 38 and 39, the three channels the wall
         * stands in front of. */
        static const int bar_h[3] = {6, 10, 8};
        for(int i = 0; i < 3; i++) {
            int bx = x + 7 + i * 5;
            canvas_draw_box(canvas, bx, y + 4 + (10 - bar_h[i]), 3, bar_h[i]);
        }
    }
}

uint8_t wall_art_bar_h(uint16_t busy_ppt, uint8_t height) {
    if(busy_ppt > 1000) busy_ppt = 1000;

    const uint32_t low = (uint32_t)height * 40u / 100u; /* 0 .. 10% occupancy  */
    const uint32_t mid = (uint32_t)height * 35u / 100u; /* 10 .. 30%           */
    const uint32_t top = height - low - mid; /* 30 .. 100%          */

    uint32_t h;
    if(busy_ppt <= 100) {
        h = (busy_ppt * low) / 100u;
    } else if(busy_ppt <= 300) {
        h = low + ((busy_ppt - 100u) * mid) / 200u;
    } else {
        h = low + mid + ((busy_ppt - 300u) * top) / 700u;
    }
    if(h > height) h = height;
    return (uint8_t)h;
}

void wall_art_spectrum(
    Canvas* canvas,
    const uint16_t busy_ppt[BwChanCount],
    int base_y,
    int height,
    int marked_chan) {
    UNUSED(marked_chan);

    for(size_t c = 0; c < BwChanCount; c++) {
        int x = wall_art_chan_x((BwChan)c);
        int h = wall_art_bar_h(busy_ppt[c], (uint8_t)height);

        if(bw_chan_is_adv((BwChan)c)) {
            /* Solid, with a notched top: this is a piece of wall. */
            if(h > 0) {
                canvas_draw_box(canvas, x, base_y - h, WALL_BAR_W, h);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_dot(canvas, x + 3, base_y - h);
                canvas_draw_dot(canvas, x + 6, base_y - h);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_line(canvas, x, base_y, x + WALL_BAR_W - 1, base_y);
            }
        } else {
            /* Texture, not substance. Wi-Fi is context. */
            for(int yy = base_y - h; yy < base_y; yy += 2) {
                canvas_draw_line(canvas, x, yy, x + WALL_BAR_W - 1, yy);
            }
            canvas_draw_line(canvas, x, base_y, x + WALL_BAR_W - 1, base_y);
        }
    }
}

void wall_art_spectrum_labels(Canvas* canvas, int base_y, int marked_chan) {
    canvas_set_font(canvas, FontSecondary);
    for(size_t c = 0; c < BwChanCount; c++) {
        int x = wall_art_chan_x((BwChan)c);
        const char* label = bw_chan_info[c].label;

        if((int)c == marked_chan) {
            /* Where the radio is parked right now. Knocked out of a filled
             * tab rather than underlined: an underline would have to live in
             * the two pixels between this row and the status line. */
            int w = (int)strlen(label) * 5 + 1;
            canvas_draw_box(canvas, x - 1, base_y - 7, w + 1, 8);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, base_y, label);
            canvas_set_color(canvas, ColorBlack);
            continue;
        }

        canvas_draw_str(canvas, x, base_y, label);
    }
}

void wall_art_arrow(Canvas* canvas, int x, int y, int dx, int dy) {
    /* A short shaft with a two-pixel head, pointing the way it is travelling. */
    canvas_draw_line(canvas, x, y, x - dx * 4, y - dy * 4);
    canvas_draw_dot(canvas, x - dy, y - dx);
    canvas_draw_dot(canvas, x + dy, y + dx);
}
