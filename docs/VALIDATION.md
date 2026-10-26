# Validation record — 26 October 2026

## Passed

- GCC C11 portable algorithm build with `-O2 -Wall -Wextra -Werror -pedantic`.
- Core assertions: encoder uint32 wrap, reverse motion, initial phase acquisition, repeat UID deduplication, successful/missing passage boundaries, miss-streak recovery, all-four-tag loss, jump desynchronisation, invalid configuration, CRC known vector, synthetic4kHz FFT peak, RMS/HF-energy/kurtosis and zero-signal behaviour.
- MFRC522 mocked-register tests:4-byte and7-byte UID acquisition, cascade selection, transmitted SELECT CRC, no response, corrupted BCC/SAK CRC and unsupported reader identity. This checks protocol code paths, not real RF behaviour.
- Four Python logger tests: checksum known vector, valid frame, corrupted frame, malformed frame.
- AddressSanitizer + UndefinedBehaviorSanitizer core and RFID tests passed. Leak checking was disabled because LeakSanitizer could not inspect this runtime's process/thread metadata; application algorithms do not allocate dynamically.
- `bg_app.c` source syntax checks against current official STM32F4 HAL, ST CMSIS-device and ARM CMSIS-Core headers with STM32F446xx selected, for both primary and secondary node configurations. `-Wall -Wextra -Werror`; host-width pointer-cast warnings in CMSIS were disabled because the syntax checker is64-bit x86. No project-source warning was suppressed.
- Unmodified ST IIS3DWB driver and custom MFRC522 driver passed C11 syntax checking.

## Not performed / not claimed

- No ARM cross-link, CubeMX project generation or `.elf`/`.bin` flashing. The package requires STM32CubeIDE board generation as documented; host syntax checking is not an embedded build.
- No real board, sensor, DMA, timing, power, pinout or thermal verification.
- No real RF read-zone/antenna/collision test.10-byte UID logic is implemented but not exercised by the delivered mock tests.
- No detection accuracy, predictive lead time, industrial reliability or safety certification.
- No 1Hz-resolution FFT, trained anomaly model, complete laptop twin/vision pipeline, hardware camera trigger or PLC integration test.

Re-run `make test` after changes; build in CubeIDE with warnings enabled and inspect memory usage before flashing. Use `COMMISSIONING.md` for the hardware acceptance procedure. Do not treat these source checks as measured prototype results.
