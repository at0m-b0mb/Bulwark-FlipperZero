#include "bw_store.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define BW_SETTINGS_MAGIC 0xB0
#define BW_SETTINGS_VERSION 1

void bw_store_defaults(BwSettings* settings) {
    memset(settings, 0, sizeof(BwSettings));
    settings->sweep_len = BwSweepNormal;
    settings->alert = true;
    settings->logging = false;
    settings->keep_screen_on = true;
}

bool bw_store_load(BwSettings* settings) {
    bw_store_defaults(settings);
    BwSettings loaded;
    if(!saved_struct_load(
           BW_SETTINGS_PATH,
           &loaded,
           sizeof(BwSettings),
           BW_SETTINGS_MAGIC,
           BW_SETTINGS_VERSION)) {
        return false;
    }
    if(loaded.sweep_len >= BwSweepLenCount) loaded.sweep_len = BwSweepNormal;
    *settings = loaded;
    return true;
}

bool bw_store_save(const BwSettings* settings) {
    bw_store_ensure_folder();
    return saved_struct_save(
        BW_SETTINGS_PATH, settings, sizeof(BwSettings), BW_SETTINGS_MAGIC, BW_SETTINGS_VERSION);
}

void bw_store_ensure_folder(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, BW_APP_FOLDER);
    furi_record_close(RECORD_STORAGE);
}

bool bw_store_log_sweep(const BwSweep* sweep, const BwScore* score, bool demo) {
    bw_store_ensure_folder();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    const bool existed = storage_file_exists(storage, BW_LOG_PATH);
    if(storage_file_open(file, BW_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FuriString* line = furi_string_alloc();

        if(!existed) {
            furi_string_set(
                line,
                "when,source,verdict,score,raw,cap,interference,"
                "adv_busy_ppt,ref_busy_ppt,adv_peak_dbm,band_floor_dbm,swing_db,"
                "ch37_ppt,ch38_ppt,ch39_ppt,w1_ppt,w6_ppt,w11_ppt,samples,rate_hz\n");
            storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
        }

        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        furi_string_printf(
            line,
            "%04u-%02u-%02u %02u:%02u:%02u,%s,%s,%u,%u,%s,%s,"
            "%u,%u,%d,%d,%u,"
            "%u,%u,%u,%u,%u,%u,%lu,%u\n",
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            dt.second,
            demo ? "demo" : "radio",
            bw_verdict_name(score->verdict),
            score->score,
            score->raw_score,
            bw_cap_name(score->binding_cap),
            bw_interference_name(score->interference),
            sweep->adv_busy_ppt,
            sweep->ref_busy_ppt,
            sweep->adv_peak_dbm,
            sweep->band_floor_dbm,
            sweep->adv_swing_db,
            sweep->chan[BwChanAdv37].busy_ppt,
            sweep->chan[BwChanAdv38].busy_ppt,
            sweep->chan[BwChanAdv39].busy_ppt,
            sweep->chan[BwChanWifi1].busy_ppt,
            sweep->chan[BwChanWifi6].busy_ppt,
            sweep->chan[BwChanWifi11].busy_ppt,
            (unsigned long)sweep->samples,
            sweep->rate_hz);

        ok = storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line)) ==
             furi_string_size(line);
        furi_string_free(line);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

int32_t bw_store_log_rows(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    int32_t rows = -1;

    if(storage_file_open(file, BW_LOG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        rows = 0;
        uint8_t buf[128];
        size_t read;
        while((read = storage_file_read(file, buf, sizeof(buf))) > 0) {
            for(size_t i = 0; i < read; i++) {
                if(buf[i] == '\n') rows++;
            }
        }
        /* The header is not a sweep. */
        if(rows > 0) rows--;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return rows;
}

bool bw_store_log_clear(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool ok = storage_simply_remove(storage, BW_LOG_PATH);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
