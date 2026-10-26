#include "mfrc522.h"
#include <string.h>
static void crc_a(const uint8_t *p,unsigned n,uint8_t out[2]){
 uint16_t c=0x6363;while(n--){uint8_t x=*p++^(uint8_t)c;x^=x<<4;c=(c>>8)^((uint16_t)x<<8)^((uint16_t)x<<3)^(x>>4);}out[0]=(uint8_t)c;out[1]=(uint8_t)(c>>8);
}
#define WR(r,v) do {if(d->write((r),(v)))return -1;} while(0)
#define RD(r,v) do {if(d->read((r),&(v)))return -1;} while(0)
static int exchange(mfrc522 *d,const uint8_t *tx,unsigned n,unsigned bits,uint8_t *rx,unsigned cap,unsigned *len){
 WR(0x01,0);WR(0x04,0x7f);WR(0x0a,0x80);
 for(unsigned i=0;i<n;i++)WR(0x09,tx[i]);
 WR(0x0d,bits);WR(0x01,0x0c);WR(0x0d,0x80|bits);
 uint32_t start=d->ms();uint8_t irq=0;
 do {RD(0x04,irq);if(irq&0x33)break;} while((uint32_t)(d->ms()-start)<3);
 WR(0x0d,bits);WR(0x01,0);
 uint8_t err,count,last;RD(0x06,err);if(err&0x1f)return -1;
 if(!(irq&0x30))return 0;
 RD(0x0a,count);RD(0x0c,last);
 if(count>cap || (last&7))return -1;
 *len=count;
 for(unsigned i=0;i<count;i++)RD(0x09,rx[i]);
 return 1;
}
int mfrc522_init(mfrc522 *d){
 uint8_t v;RD(0x37,v);if(v!=0x91 && v!=0x92)return -1;
 WR(0x01,0);WR(0x2a,0x80);WR(0x2b,0xa9);WR(0x2c,0);WR(0x2d,50);
 WR(0x15,0x40);WR(0x11,0x3d);WR(0x12,0);WR(0x13,0);WR(0x14,0x83);
 return 0;
}
int mfrc522_poll(mfrc522 *d,uint8_t uid[10],uint8_t *length){
 uint8_t tx[9]={0x52},rx[10];unsigned n=0;*length=0;
 int r=exchange(d,tx,1,7,rx,sizeof rx,&n);if(r<=0)return r;if(n!=2)return -1;
 for(unsigned level=0;level<3;level++){
  tx[0]=(uint8_t)(0x93+2*level);tx[1]=0x20;
  r=exchange(d,tx,2,0,rx,sizeof rx,&n);if(r!=1 || n!=5)return -1;
  if((rx[0]^rx[1]^rx[2]^rx[3])!=rx[4])return -1;
  /* Cascade tag 0x88 is not part of the UID. */
  unsigned skip=rx[0]==0x88?1:0;
  if(*length+4-skip>10)return -1;
  memcpy(uid+*length,rx+skip,4-skip);*length+=(uint8_t)(4-skip);
  tx[1]=0x70;memcpy(tx+2,rx,5);crc_a(tx,7,tx+7);
  r=exchange(d,tx,9,0,rx,sizeof rx,&n);if(r!=1 || n!=3)return -1;
  uint8_t crc[2];crc_a(rx,1,crc);if(rx[1]!=crc[0] || rx[2]!=crc[1])return -1;
  if(!(rx[0]&4)){
   if(skip || (*length!=4 && *length!=7 && *length!=10))return -1;
   tx[0]=0x50;tx[1]=0;crc_a(tx,2,tx+2);
   (void)exchange(d,tx,4,0,rx,sizeof rx,&n); /* HALT normally times out. */
   return 1;
  }
  if(!skip)return -1;
 }
 return -1;
}
