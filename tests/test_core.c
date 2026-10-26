#include "bg.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static bg_config config(void){bg_config c={1000,40,0.001f,{{{1,2,3,0},4,0},{{1,2,3,1},4,250},{{1,2,3,2},4,500},{{1,2,3,3},4,750}}};return c;}
int main(void){
 bg_config c=config();bg_tracker t;assert(bg_tracker_init(&t,&c));
 assert(bg_encoder_update(&t,0xfffffffeu)==0);assert(bg_encoder_update(&t,3)==5);
 assert(bg_encoder_update(&t,1)==-2);assert(!t.synced);
 assert(bg_tracker_init(&t,&c));bg_encoder_update(&t,0);
 assert(bg_tag_read(&t,c.tags[0].uid,4,100)==0);assert(t.synced);
 assert(bg_tag_read(&t,c.tags[0].uid,4,200)==0);assert(t.tag[0].reads==1);
 bg_encoder_update(&t,41);bg_tracker_advance(&t);assert(t.tag[0].passes==1 && !t.tag[0].misses);
 bg_encoder_update(&t,291);bg_tracker_advance(&t);assert(t.tag[1].misses==1 && t.tag[1].streak==1);
 bg_encoder_update(&t,750);assert(bg_tag_read(&t,c.tags[3].uid,4,55)==3);assert(t.tag[2].misses==1);
 bg_encoder_update(&t,791);bg_tracker_advance(&t);assert(t.tag[3].passes==1 && !t.tag[3].misses);
 bg_encoder_update(&t,1250);assert(bg_tag_read(&t,c.tags[1].uid,4,55)==1);
 bg_encoder_update(&t,1291);bg_tracker_advance(&t);assert(t.tag[1].streak==0 && t.tag[1].passes==2);
 bg_encoder_update(&t,1290);assert(!t.synced && t.section==-1);
 uint8_t unknown[4]={9,9,9,9};assert(bg_tag_read(&t,unknown,4,1)==-1);
 /* All missing with continued encoder movement counts all four independently. */
 assert(bg_tracker_init(&t,&c));bg_encoder_update(&t,0);bg_tag_read(&t,c.tags[0].uid,4,10);
 for(uint32_t p=50;p<=4100;p+=50){bg_encoder_update(&t,p);bg_tracker_advance(&t);}
 for(int i=0;i<4;i++)assert(t.tag[i].streak>=3);
 /* Position jump invalidates synchronisation instead of inventing passage counts. */
 bg_encoder_update(&t,12000);bg_tracker_advance(&t);assert(!t.synced);
 c.tags[1].phase=10;assert(!bg_tracker_init(&t,&c));c=config();c.tags[1]=c.tags[0];assert(!bg_tracker_init(&t,&c));
 assert(bg_crc16((const uint8_t *)"123456789",9)==0x4b37);
 static bg_dsp d;bg_features f;bg_dsp_reset(&d);
 for(int i=0;i<BG_FFT_N;i++)assert(bg_dsp_push(&d,sinf(6.28318530718f*128*i/BG_FFT_N))==(i==BG_FFT_N-1));
 bg_dsp_finish(&d,32000,&f);assert(fabsf(f.rms-0.7071f)<0.01f);assert(fabsf(f.peak_hz-4000)<1);assert(f.hf_fraction>0.99f);assert(f.kurtosis>1.45f && f.kurtosis<1.55f);
 bg_dsp_reset(&d);for(int i=0;i<BG_FFT_N;i++)bg_dsp_push(&d,0);bg_dsp_finish(&d,32000,&f);assert(f.rms==0 && f.crest==0 && f.hf_fraction==0);
 puts("PASS: encoder wrap/reversal, sync, repeat UID, missed passes, recovery, all-missing, config validation, CRC, FFT, zero signal");
}
