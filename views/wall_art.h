/**
 * Bulwark - the drawing the whole app is built out of.
 *
 * Two ideas, reused everywhere: a rampart (a wall with crenellations, which
 * is what a bulwark is) and a little spectrum of the six frequencies the app
 * listens to, laid out left to right in real frequency order so the picture
 * is a picture of the band and not a bar chart with labels on it.
 */
#pragma once

#include "../helpers/bw_chan.h"

#include <gui/gui.h>

/** Where a channel's bar starts, in pixels, spaced by real frequency. */
uint8_t wall_art_chan_x(BwChan chan);

/** How wide every channel bar is. */
#define WALL_BAR_W 10

/**
 * A crenellated top edge, drawn downward from @p y.
 * Used as the bottom edge of the inverted title bar, so the screen reads as
 * something happening below a wall.
 */
void wall_art_crenellations(Canvas* canvas, int x, int y, int w, int h);

/** The Bulwark shield, top-left corner at @p x, @p y. 28 x 26. */
void wall_art_shield(Canvas* canvas, int x, int y, bool filled);

/**
 * Bar height for an occupancy figure, on a deliberately compressed scale.
 *
 * Real occupancy almost never exceeds a third, so a linear 0..100% axis would
 * spend two thirds of a thirty-pixel-tall screen on space nothing ever
 * reaches. Instead the first 10% gets 40% of the height, the next 20% gets
 * another 35%, and everything above 30% shares the rest - so a quiet room is
 * a visible stub rather than nothing, and a flood is nearly full height. The
 * exact figures are on the breakdown screen; this is a picture.
 */
uint8_t wall_art_bar_h(uint16_t busy_ppt, uint8_t height);

/**
 * The six-channel spectrum.
 *
 * Advertising channels are solid; the Wi-Fi reference channels are drawn as
 * texture, because they are context rather than evidence.
 *
 * @param busy_ppt  per channel occupancy, 0..1000
 * @param base_y    the line the bars stand on
 * @param height    the height of a full bar
 */
void wall_art_spectrum(
    Canvas* canvas,
    const uint16_t busy_ppt[BwChanCount],
    int base_y,
    int height,
    int marked_chan);

/**
 * Channel labels under the bars, at text baseline @p base_y. The channel the
 * radio is parked on right now gets an underline.
 */
void wall_art_spectrum_labels(Canvas* canvas, int base_y, int marked_chan);

/** A small arrow flying at the wall, used by the splash and the walkthrough. */
void wall_art_arrow(Canvas* canvas, int x, int y, int dx, int dy);
