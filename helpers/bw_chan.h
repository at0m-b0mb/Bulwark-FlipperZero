/**
 * Bulwark - the channel plan.
 *
 * The Flipper's radio can be parked on any one of the forty 2 MHz-wide RF
 * channels the Bluetooth LE PHY defines, numbered the way the radio test
 * commands number them:
 *
 *     centre frequency (MHz) = 2402 + 2 * index,   index = 0 .. 39
 *
 * Bluetooth LE advertising - every popup attack, and every legitimate beacon,
 * headphone and fitness tracker - happens on exactly three of them. They are
 * called advertising channels 37, 38 and 39, and they are *not* channels 37,
 * 38 and 39 in the numbering above; they are indices 0, 12 and 39:
 *
 *     adv 37  ->  2402 MHz  ->  index 0
 *     adv 38  ->  2426 MHz  ->  index 12
 *     adv 39  ->  2480 MHz  ->  index 39
 *
 * Those three were chosen in 1998-era spectrum politics: they sit in the gaps
 * between the centres of Wi-Fi channels 1, 6 and 11, which is why a phone can
 * still find its earbuds in a room full of Wi-Fi. That accident of history is
 * the whole basis of this app. If we listen on the three advertising channels
 * *and* on the three Wi-Fi centres at the same time, we can tell "the band is
 * busy" from "the *advertising* channels are busy", and only the second one
 * means somebody is advertising at you.
 *
 *     Wi-Fi 1  centre 2412 MHz  ->  index 5
 *     Wi-Fi 6  centre 2437 MHz  ->  index 17 (2436 - the grid is 2 MHz)
 *     Wi-Fi 11 centre 2462 MHz  ->  index 30
 *
 * Nothing here touches furi, on purpose: the whole measurement and scoring
 * path is host-testable (see test/).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Every channel Bulwark camps on, in hop order. */
typedef enum {
    BwChanAdv37 = 0, /**< 2402 MHz - BLE advertising channel 37 */
    BwChanWifi1, /**< 2412 MHz - centre of Wi-Fi channel 1  */
    BwChanAdv38, /**< 2426 MHz - BLE advertising channel 38 */
    BwChanWifi6, /**< 2436 MHz - centre of Wi-Fi channel 6  */
    BwChanWifi11, /**< 2462 MHz - centre of Wi-Fi channel 11 */
    BwChanAdv39, /**< 2480 MHz - BLE advertising channel 39 */
    BwChanCount,
} BwChan;

#define BW_ADV_COUNT 3u
#define BW_REF_COUNT 3u

/** What a channel is for. */
typedef enum {
    BwChanRoleAdv, /**< listened to for the attack */
    BwChanRoleRef, /**< listened to so Wi-Fi can be told apart from it */
} BwChanRole;

typedef struct {
    uint8_t rf_index; /**< 0..39, what furi_hal_bt_start_rx() wants */
    uint16_t mhz; /**< centre frequency, for the screen */
    BwChanRole role;
    const char* label; /**< "37", "39", "W1", "W6" ... 2 chars, fits the grid */
} BwChanInfo;

/** The plan itself. Indexed by BwChan. */
extern const BwChanInfo bw_chan_info[BwChanCount];

/** The three advertising channels, in BwChan terms. */
extern const BwChan bw_chan_adv[BW_ADV_COUNT];

/** The three Wi-Fi reference channels, in BwChan terms. */
extern const BwChan bw_chan_ref[BW_REF_COUNT];

/** true when this channel is one of the three carrying advertising. */
bool bw_chan_is_adv(BwChan chan);

#ifdef __cplusplus
}
#endif
