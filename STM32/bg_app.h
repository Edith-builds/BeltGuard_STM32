#ifndef BG_APP_H
#define BG_APP_H
#include "stm32f4xx_hal.h"
void BG_Init(void);
void BG_Task(void);
void BG_I2SHalf(void);
void BG_I2SFull(void);
void BG_I2SError(void);
void BG_UARTDone(void);
#endif
