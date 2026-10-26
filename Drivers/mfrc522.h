#ifndef MFRC522_H
#define MFRC522_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
 int (*read)(uint8_t reg,uint8_t *value);
 int (*write)(uint8_t reg,uint8_t value);
 uint32_t (*ms)(void);
} mfrc522;
int mfrc522_init(mfrc522 *);
/* 1 UID, 0 no response, -1 bus/protocol/collision/CRC error. One tag in field. */
int mfrc522_poll(mfrc522 *,uint8_t uid[10],uint8_t *length);
#endif
