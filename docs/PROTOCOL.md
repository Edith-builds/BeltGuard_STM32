# Serial feature protocol v1

Transport: USART2 1152008N1. Each line is UTF-8/ASCII JSON, followed by `*` and four uppercase hexadecimal CRC digits, then LF. CRC16/Modbus parameters: initial0xFFFF, reflected polynomial0xA001, no final xor. CRC covers **exactly the JSON bytes**, not the star, checksum or newline. Check vector `123456789` → `4B37`. Max transmit buffer2048 bytes; allocation/truncation failure increments `tx_drops`.

`host/logger.py` verifies checksum and version, appends `host_unix_ns`, and logs JSON Lines. One logger per USB port can be used for the two nodes. `seq` is an unsigned32-bit packet sequence, `ms` is boot-relative32-bit milliseconds and wraps about49.7 days. Segment runs on reset/sequence discontinuity; compare elapsed times using modulo32 arithmetic. `position_counts` is signed64-bit extended encoder displacement.

| Field group | Interpretation |
|---|---|
| `node`, `seq`, `ms` | Source identity, sequence, boot-relative report time |
| `calibrated` | Configuration commissioning switch; not a calibration certificate |
| `gated` | Primary node's valid speed+load analysis gate. Node2 remote mode always reports0; edge must join primary gate |
| `load_raw`, `load_kg`, `load_valid` | HX711 signed raw/converted value and freshness/saturation validity. Converted kg is meaningless until calibrated |
| `speed_mps` | Signed encoder speed averaged over roughly100ms |
| `sync`, `section` | Recognised-tag phase anchor and current encoder-derived section; -1 means unknown |
| `uid`, `uid_ms`, `uid_latency_us` | Latest observed complete UID, observation time and software transaction latency. May be stale; not necessarily the current section's tag |
| `rf_ok` | Reader register communication/identity, not RF antenna self-test |
| `rf_warning_mask` | Bit0..3 correspond to configured tag index0..3; repeated-miss advisory, not confirmed damage |
| `rf_passes`, `rf_misses`, `rf_streak` | Per-tag cumulative completed passages/missing passages and current consecutive misses |
| `vib_valid`, `mic_valid` | Latest processed block fresh (<200ms) and usable at this node. Node2 still requires remote load/run gate |
| `vib_ms`, `mic_ms` | Block processing-completion time, approximate; not sample-clock timestamps |
| `vib_section`, `mic_section` | Section seen during the block; -1 on boundary change/unknown. Queue delays and mount offsets remain to be corrected |
| `vib_rms_g`, `vib_crest`, `vib_kurtosis`, `vib_hf_fraction`, `vib_peak_hz` | X/Y/Z arrays; RMS in g, dimensionless crest/kurtosis/HF ratio, peak in Hz |
| `vib_envelope_ac_rms_g`, `mic_envelope_ac_rms_fs` | AC RMS of the rectified/low-pass envelope, g for axes and digital FS for mic |
| `mic_rms_fs`, `mic_crest`, `mic_kurtosis`, `mic_hf_fraction`, `mic_peak_hz` | Microphone digital full-scale units; **not calibrated SPL/dB** |
| `fifo_gaps`, `mic_gaps`, `mic_errors`, `spi_errors`, `rf_errors`, `sync_errors`, `unknown_uids`, `tx_drops` | Cumulative diagnostic counters; resetting a board clears them |
| `health_index` | Null: learned multimodal BHI is an edge-compute responsibility |

Always check validity and timestamps before using values. Invalid fields retain their last numeric measurement for diagnostics and must not be interpreted as current healthy data. Do not equate a missing message to a zero anomaly score.
