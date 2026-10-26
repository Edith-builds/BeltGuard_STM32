#include "mfrc522.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t regs[64],tx[64],rx[64];static unsigned tx_n,rx_n,rx_i,stage;static uint32_t tick;
static int mode; /* 0=4 byte, 1=7 byte, 2=no tag, 3=bad BCC, 4=bad SAK CRC */
static void crc(const uint8_t *p,unsigned n,uint8_t out[2]){uint16_t c=0x6363;while(n--){uint8_t x=*p++^(uint8_t)c;x^=x<<4;c=(c>>8)^((uint16_t)x<<8)^((uint16_t)x<<3)^(x>>4);}out[0]=(uint8_t)c;out[1]=(uint8_t)(c>>8);}
static void respond(void){
 rx_i=rx_n=0;regs[4]=0x30;regs[6]=0;regs[12]=0;stage++;
 if(mode==2 || tx[0]==0x50){regs[4]=1;return;}
 if(tx[0]==0x52){rx[0]=4;rx[1]=0;rx_n=2;return;}
 if(tx[1]==0x20){
  if(mode==1 && tx[0]==0x93){rx[0]=0x88;rx[1]=1;rx[2]=2;rx[3]=3;}
  else {rx[0]=4;rx[1]=5;rx[2]=6;rx[3]=7;}
  rx[4]=rx[0]^rx[1]^rx[2]^rx[3];if(mode==3)rx[4]^=1;rx_n=5;return;
 }
 if(tx[1]==0x70){uint8_t c[2];crc(tx,7,c);assert(tx_n==9 && tx[7]==c[0] && tx[8]==c[1]);rx[0]=(mode==1 && tx[0]==0x93)?4:0;crc(rx,1,rx+1);if(mode==4)rx[1]^=1;rx_n=3;return;}
 assert(0);
}
static int write_reg(uint8_t r,uint8_t v){
 regs[r]=v;if(r==10 && (v&0x80))tx_n=0;
 if(r==9)tx[tx_n++]=v;
 if(r==13 && (v&0x80))respond();
 return 0;
}
static int read_reg(uint8_t r,uint8_t *v){
 if(r==9){assert(rx_i<rx_n);*v=rx[rx_i++];}
 else if(r==10)*v=(uint8_t)rx_n;
 else *v=regs[r];
 return 0;
}
static uint32_t ms(void){return tick++;}
int main(void){
 mfrc522 d={read_reg,write_reg,ms};regs[0x37]=0x92;assert(mfrc522_init(&d)==0);
 uint8_t uid[10],len;assert(mfrc522_poll(&d,uid,&len)==1 && len==4 && uid[0]==4 && uid[3]==7);
 mode=1;assert(mfrc522_poll(&d,uid,&len)==1 && len==7);for(unsigned i=0;i<7;i++)assert(uid[i]==i+1);
 mode=2;assert(mfrc522_poll(&d,uid,&len)==0);
 mode=3;assert(mfrc522_poll(&d,uid,&len)==-1);
 mode=4;assert(mfrc522_poll(&d,uid,&len)==-1);
 regs[0x37]=0;assert(mfrc522_init(&d)==-1);
 puts("PASS: RFID 4/7-byte UID, cascade, selection CRC, missing tag, BCC/CRC corruption, reader identity");
}
