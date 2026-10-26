#include "bg.h"
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
bool bg_tracker_init(bg_tracker *t, const bg_config *c) {
 memset(t,0,sizeof *t); t->section=-1;
 if(c->loop_counts<=0 || c->window_counts<=0 || 2LL*c->window_counts>=c->loop_counts || !isfinite(c->metres_per_count) || c->metres_per_count<=0) return false;
 for(int i=0;i<BG_TAGS;i++) {
  if(c->tags[i].len!=4 && c->tags[i].len!=7 && c->tags[i].len!=10) return false;
  if(c->tags[i].phase<0 || c->tags[i].phase>=c->loop_counts) return false;
  for(int j=0;j<i;j++) {
   int d=abs(c->tags[i].phase-c->tags[j].phase);
   if(d<=2LL*c->window_counts || c->loop_counts-d<=2LL*c->window_counts) return false;
   if(c->tags[i].len==c->tags[j].len && !memcmp(c->tags[i].uid,c->tags[j].uid,c->tags[i].len)) return false;
  }
 }
 t->cfg=*c; return true;
}
void bg_tracker_unsync(bg_tracker *t) { t->synced=false; t->section=-1; }
int32_t bg_encoder_update(bg_tracker *t,uint32_t raw) {
 if(!t->started) { t->started=true; t->last_raw=raw; return 0; }
 uint32_t u=raw-t->last_raw; t->last_raw=raw;
 int64_t d=u<=INT32_MAX?(int64_t)u:(int64_t)u-4294967296LL;
 t->position+=d;
 if(d<0) {t->reversals++; bg_tracker_unsync(t);}
 return (int32_t)d;
}
void bg_tracker_advance(bg_tracker *t) {
 if(!t->synced) return;
 for(int i=0;i<BG_TAGS;i++) {
  bg_tag_state *s=&t->tag[i];
  /* A huge jump is a position fault, not thousands of fabricated missing tags. */
  if(t->position-s->next_center>2LL*t->cfg.loop_counts) {t->sync_errors++;bg_tracker_unsync(t);return;}
  while(t->position>s->next_center+t->cfg.window_counts) {
   s->passes++;
   if(s->seen) s->streak=0; else {s->misses++;s->streak++;}
   s->seen=false; s->next_center+=t->cfg.loop_counts;
  }
 }
 int64_t phase=(t->position-t->origin)%t->cfg.loop_counts;
 if(phase<0) phase+=t->cfg.loop_counts;
 int best=-1; int32_t bestphase=-1, maxphase=-1; int maxi=-1;
 for(int i=0;i<BG_TAGS;i++) {
  int32_t p=t->cfg.tags[i].phase;
  if(p>maxphase){maxphase=p;maxi=i;}
  if(p<=phase && p>bestphase){bestphase=p;best=i;}
 }
 t->section=best>=0?best:maxi;
}
int bg_tag_read(bg_tracker *t,const uint8_t *uid,uint8_t len,uint32_t latency) {
 int id=-1;
 for(int i=0;i<BG_TAGS;i++) if(len==t->cfg.tags[i].len && !memcmp(uid,t->cfg.tags[i].uid,len)) {id=i;break;}
 if(id<0){t->unknown_reads++;return -1;}
 if(!t->synced) {
  t->origin=t->position-t->cfg.tags[id].phase;
  for(int i=0;i<BG_TAGS;i++) {
   t->tag[i].next_center=t->origin+t->cfg.tags[i].phase;
   while(t->tag[i].next_center+t->cfg.window_counts<t->position) t->tag[i].next_center+=t->cfg.loop_counts;
   t->tag[i].seen=false;
  }
  t->synced=true;
 }
 bg_tracker_advance(t);
 if(!t->synced) return -2;
 bg_tag_state *s=&t->tag[id];
 int64_t d=t->position-s->next_center;
 if(d < -t->cfg.window_counts || d > t->cfg.window_counts) {t->sync_errors++;return -2;}
 /* Count once per passage; repeated polling does not make the tag healthier. */
 if(!s->seen){s->seen=true;s->reads++;s->latency_us=latency;}
 return id;
}
void bg_dsp_reset(bg_dsp *d){memset(d,0,sizeof *d);}
bool bg_dsp_push(bg_dsp *d,float v){
 if(d->n>=BG_FFT_N) return true;
 /* Streaming DC rejection (alpha .001); maintain state between valid blocks. */
 d->dc+=0.001f*(v-d->dc); float x=v-d->dc;
 d->envelope+=0.02f*(fabsf(x)-d->envelope);
 d->env_sum+=d->envelope;d->env_sum2+=d->envelope*d->envelope;
 d->x[d->n++]=x; float q=x*x;d->sum2+=q;d->sum4+=q*q;
 if(fabsf(x)>d->peak)d->peak=fabsf(x);
 return d->n==BG_FFT_N;
}
void bg_dsp_finish(bg_dsp *d,float fs,bg_features *f){
 static float im[BG_FFT_N]; /* Main-context only; non-reentrant. */
 memset(f,0,sizeof *f);if(d->n!=BG_FFT_N || fs<=0)return;
 float mean2=d->sum2/BG_FFT_N;
 float env_mean=d->env_sum/BG_FFT_N;
 f->envelope_ac_rms=sqrtf(fmaxf(0,d->env_sum2/BG_FFT_N-env_mean*env_mean));
 f->samples=BG_FFT_N;f->rms=sqrtf(mean2);f->peak=d->peak;
 f->crest=f->rms>1e-12f?d->peak/f->rms:0;
 f->kurtosis=mean2>1e-12f?(d->sum4/BG_FFT_N)/(mean2*mean2):0;
 for(unsigned i=0;i<BG_FFT_N;i++){d->x[i]*=0.5f-0.5f*cosf(6.28318530718f*i/(BG_FFT_N-1));im[i]=0;}
 for(unsigned i=1,j=0;i<BG_FFT_N;i++){
  unsigned b=BG_FFT_N>>1;for(;j&b;b>>=1)j^=b;j^=b;
  if(i<j){float a=d->x[i];d->x[i]=d->x[j];d->x[j]=a;}
 }
 for(unsigned len=2;len<=BG_FFT_N;len<<=1){
  float ang=-6.28318530718f/len,wr0=cosf(ang),wi0=sinf(ang);
  for(unsigned i=0;i<BG_FFT_N;i+=len){float wr=1,wi=0;
   for(unsigned j=0;j<len/2;j++){
    unsigned a=i+j,b=a+len/2;float vr=d->x[b]*wr-im[b]*wi,vi=d->x[b]*wi+im[b]*wr;
    d->x[b]=d->x[a]-vr;im[b]=im[a]-vi;d->x[a]+=vr;im[a]+=vi;
    float nwr=wr*wr0-wi*wi0;wi=wr*wi0+wi*wr0;wr=nwr;
   }
  }
 }
 float total=0,hf=0,pk=0;
 for(unsigned k=1;k<=BG_FFT_N/2;k++){
  float p=d->x[k]*d->x[k]+im[k]*im[k];if(k<BG_FFT_N/2)p*=2;
  float hz=k*fs/BG_FFT_N;total+=p;if(hz>=2000 && hz<=6000)hf+=p;
  if(p>pk){pk=p;f->peak_hz=hz;}
 }
 f->hf_fraction=total>1e-12f?hf/total:0;
 d->env_sum=0;d->env_sum2=0;d->n=0;d->sum2=0;d->sum4=0;d->peak=0;
}
uint16_t bg_crc16(const uint8_t *p,size_t n){uint16_t c=0xffff;while(n--){c^=*p++;for(int i=0;i<8;i++)c=(c&1)?(c>>1)^0xa001:c>>1;}return c;}
