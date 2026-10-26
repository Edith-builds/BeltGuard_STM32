# Requirements and primary sources

Local requirements: uploaded final `BeltGuard_Vishwakarma_Stage2(1).pdf`, pages on architecture, hardware selection, electronics and prototype BOM; plus user's explicit RFID-health and fixed-camera prototype decisions. It names STM32 F4/H7 class and candidate peripherals, not a committed board/pinout. This code chooses an affordable F446 reference implementation; it does not change the final deck's BOM totals or purchase anything.

Primary hardware documentation checked 26 October 2026:

- ST IIS3DWB datasheet: https://www.st.com/resource/en/datasheet/iis3dwb.pdf
- Official ST register driver: https://github.com/STMicroelectronics/iis3dwb-pid . The two files are included unmodified with `Drivers/LICENSE_ST`; see `source_hashes.txt` for exact delivered bytes.
- ST Nucleo-64 UM1724: https://www.st.com/resource/en/user_manual/dm00105823.pdf
- ST STM32F446 reference manual RM0390: https://www.st.com/resource/en/reference_manual/dm00135183.pdf
- Official F4 HAL I2S implementation (checked DMA size doubling): https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_hal_i2s.c
- ST HAL headers used for source syntax checking: https://github.com/STMicroelectronics/stm32f4xx-hal-driver
- STM32F4 CMSIS device headers: https://github.com/STMicroelectronics/cmsis-device-f4
- ARM CMSIS Core: https://github.com/ARM-software/CMSIS_5
- NXP MFRC522 datasheet: https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf
- TDK/InvenSense INMP441 datasheet: https://invensense.tdk.com/wp-content/uploads/2015/02/INMP441.pdf

The existing GitHub URL printed in the deck was unavailable through the source retrieval used in this task; no existing repository was modified or assumed to match these files. This is a separate source package for integration/review.
