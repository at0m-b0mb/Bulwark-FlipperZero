#include "bw_chan.h"

const BwChanInfo bw_chan_info[BwChanCount] = {
    [BwChanAdv37] = {.rf_index = 0, .mhz = 2402, .role = BwChanRoleAdv, .label = "37"},
    [BwChanWifi1] = {.rf_index = 5, .mhz = 2412, .role = BwChanRoleRef, .label = "W1"},
    [BwChanAdv38] = {.rf_index = 12, .mhz = 2426, .role = BwChanRoleAdv, .label = "38"},
    [BwChanWifi6] = {.rf_index = 17, .mhz = 2436, .role = BwChanRoleRef, .label = "W6"},
    [BwChanWifi11] = {.rf_index = 30, .mhz = 2462, .role = BwChanRoleRef, .label = "W11"},
    [BwChanAdv39] = {.rf_index = 39, .mhz = 2480, .role = BwChanRoleAdv, .label = "39"},
};

const BwChan bw_chan_adv[BW_ADV_COUNT] = {BwChanAdv37, BwChanAdv38, BwChanAdv39};

const BwChan bw_chan_ref[BW_REF_COUNT] = {BwChanWifi1, BwChanWifi6, BwChanWifi11};

bool bw_chan_is_adv(BwChan chan) {
    if(chan >= BwChanCount) return false;
    return bw_chan_info[chan].role == BwChanRoleAdv;
}
