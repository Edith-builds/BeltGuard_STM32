#ifndef BG_H
#define BG_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define BG_TAGS 4
#define BG_FFT_N 1024
/* All positions in encoder x4 counts; direction is commissioned forward. */
typedef struct { uint8_t uid[10], len; int32_t phase; } bg_tag_cfg;
typedef struct {
 int32_t loop_counts, window_counts; float metres_per_count;
 bg_tag_cfg tags[BG_TAGS];
} bg_config;
typedef struct {
 uint32_t passes, misses, streak, reads, latency_us;
 bool seen; int64_t next_center;
} bg_tag_state;
typedef struct {
 bg_config cfg; bg_tag_state tag[BG_TAGS];
 uint32_t last_raw; int64_t position, origin; bool started, synced;
 int section; uint32_t unknown_reads, reversals, sync_errors;
} bg_tracker;
bool bg_tracker_init(bg_tracker *, const bg_config *);
int32_t bg_encoder_update(bg_tracker *, uint32_t raw);
void bg_tracker_advance(bg_tracker *);
int bg_tag_read(bg_tracker *, const uint8_t *, uint8_t, uint32_t latency_us);
void bg_tracker_unsync(bg_tracker *);
/* Streaming AC statistics and block FFT. Units are g for vibration, FS for mic. */
typedef struct { float rms, peak, crest, kurtosis, hf_fraction, peak_hz, envelope_ac_rms; uint32_t samples; } bg_features;
typedef struct { float x[BG_FFT_N], dc, sum2, sum4, peak, envelope, env_sum, env_sum2; uint32_t n; } bg_dsp;
void bg_dsp_reset(bg_dsp *);
bool bg_dsp_push(bg_dsp *, float sample);
void bg_dsp_finish(bg_dsp *, float fs, bg_features *);
uint16_t bg_crc16(const uint8_t *, size_t);
#endif
