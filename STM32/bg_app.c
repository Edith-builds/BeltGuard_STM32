#include "bg_app.h"
#include "bg_config.h"
#include "bg.h"
#include "iis3dwb_reg.h"
#include "mfrc522.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
extern SPI_HandleTypeDef hspi1,hspi3;
extern TIM_HandleTypeDef htim2;
extern UART_HandleTypeDef huart2;
extern IWDG_HandleTypeDef hiwdg;
#if BG_ENABLE_MIC
extern I2S_HandleTypeDef hi2s2;
#endif
static bg_tracker track;
static bg_dsp vib[3],mic;
static bg_features vf[3],mf;
static uint32_t vib_ms,mic_ms,load_ms,seq,spi_errors,fifo_gaps,rf_errors,tx_drops;
static uint32_t last_speed,last_rf,last_packet,last_retry,raw_previous;
static uint32_t uid_ms,uid_latency;static uint8_t last_uid[10],last_uid_len;
static float speed,load_kg;static int32_t load_raw;
static bool vib_ok,rf_ok,load_ok,gated,was_gated,config_ok;
static int vib_section=-1,mic_section=-1,vib_block_section=-1,mic_block_section=-1;
static volatile bool tx_busy;
static char packet[2048];
static stmdev_ctx_t sensor;
static void delay_us(uint32_t us){uint32_t start=DWT->CYCCNT,cycles=us*(SystemCoreClock/1000000U);while((uint32_t)(DWT->CYCCNT-start)<cycles){}}
static int32_t acc_read(void *handle,uint8_t reg,uint8_t *data,uint16_t n){
 (void)handle;uint8_t a=reg|0x80;HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6,GPIO_PIN_RESET);
 HAL_StatusTypeDef r=HAL_SPI_Transmit(&hspi1,&a,1,2);
 if(r==HAL_OK)r=HAL_SPI_Receive(&hspi1,data,n,2);
 HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6,GPIO_PIN_SET);return r==HAL_OK?0:-1;
}
static int32_t acc_write(void *handle,uint8_t reg,const uint8_t *data,uint16_t n){
 (void)handle;HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6,GPIO_PIN_RESET);
 HAL_StatusTypeDef r=HAL_SPI_Transmit(&hspi1,&reg,1,2);
 if(r==HAL_OK)r=HAL_SPI_Transmit(&hspi1,(uint8_t *)data,n,2);
 HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6,GPIO_PIN_SET);return r==HAL_OK?0:-1;
}
static int rf_read(uint8_t reg,uint8_t *v){
 uint8_t tx[2]={(uint8_t)(0x80|((reg<<1)&0x7e)),0},rx[2]={0};
 HAL_GPIO_WritePin(GPIOA,GPIO_PIN_4,GPIO_PIN_RESET);
 HAL_StatusTypeDef r=HAL_SPI_TransmitReceive(&hspi3,tx,rx,2,2);
 HAL_GPIO_WritePin(GPIOA,GPIO_PIN_4,GPIO_PIN_SET);*v=rx[1];return r==HAL_OK?0:-1;
}
static int rf_write(uint8_t reg,uint8_t v){
 uint8_t tx[2]={(uint8_t)((reg<<1)&0x7e),v};
 HAL_GPIO_WritePin(GPIOA,GPIO_PIN_4,GPIO_PIN_RESET);
 HAL_StatusTypeDef r=HAL_SPI_Transmit(&hspi3,tx,2,2);
 HAL_GPIO_WritePin(GPIOA,GPIO_PIN_4,GPIO_PIN_SET);return r==HAL_OK?0:-1;
}
static mfrc522 reader={rf_read,rf_write,HAL_GetTick};
static bool acc_init(void){
 uint8_t id=0,rst=1;
 if(iis3dwb_device_id_get(&sensor,&id) || id!=IIS3DWB_ID)return false;
 if(iis3dwb_reset_set(&sensor,1))return false;
 uint32_t start=HAL_GetTick();
 do {if(iis3dwb_reset_get(&sensor,&rst))return false;}while(rst && HAL_GetTick()-start<20);
 if(rst)return false;
 return !(iis3dwb_block_data_update_set(&sensor,1) || iis3dwb_auto_increment_set(&sensor,1) ||
 iis3dwb_xl_full_scale_set(&sensor,IIS3DWB_4g) ||
 iis3dwb_fifo_mode_set(&sensor,IIS3DWB_BYPASS_MODE) ||
 iis3dwb_fifo_xl_batch_set(&sensor,IIS3DWB_XL_BATCHED_AT_26k7Hz) ||
 iis3dwb_fifo_mode_set(&sensor,IIS3DWB_STREAM_MODE) ||
 iis3dwb_xl_data_rate_set(&sensor,IIS3DWB_XL_ODR_26k7Hz));
}
static void reset_vib(void){for(int a=0;a<3;a++)bg_dsp_reset(&vib[a]);vib_ms=0;vib_section=-1;}
static void service_vib(void){
 if(!vib_ok)return;
 iis3dwb_fifo_status_t s;
 if(iis3dwb_fifo_status_get(&sensor,&s)){spi_errors++;vib_ok=false;reset_vib();return;}
 if(s.fifo_ovr){
  fifo_gaps++;reset_vib();
  if(iis3dwb_fifo_mode_set(&sensor,IIS3DWB_BYPASS_MODE) || iis3dwb_fifo_mode_set(&sensor,IIS3DWB_STREAM_MODE))vib_ok=false;
  return;
 }
 unsigned n=s.fifo_level;if(n>128)n=128;if(!n)return;
 iis3dwb_fifo_out_raw_t samples[128];
 if(iis3dwb_fifo_out_multi_raw_get(&sensor,samples,(uint16_t)n)){spi_errors++;vib_ok=false;reset_vib();return;}
 for(unsigned i=0;i<n;i++){
  if((samples[i].tag>>3)!=IIS3DWB_XL_TAG)continue;
  if(!gated)continue;
  if(vib[0].n==0)vib_block_section=track.section;
  if(track.section!=vib_block_section)vib_block_section=-1;
  for(unsigned a=0;a<3;a++){
   uint16_t u=(uint16_t)(samples[i].data[2*a]|((uint16_t)samples[i].data[2*a+1]<<8));
   int32_t v=u<32768?u:(int32_t)u-65536;
   if(bg_dsp_push(&vib[a],v*0.000122f)){
    bg_dsp_finish(&vib[a],BG_VIB_FS,&vf[a]);
    vib_ms=HAL_GetTick();vib_section=vib_block_section;
   }
  }
 }
}
#define MIC_RING 4096u
static volatile uint32_t mic_wr,mic_rd,mic_gaps,mic_errors;
static float mic_ring[MIC_RING];
#if BG_ENABLE_MIC
static uint16_t mic_dma[1024];
static void mic_copy(unsigned offset){
 /* ISR copies completed half before DMA reuses it. No DSP or HAL in ISR. */
 for(unsigned i=offset;i<offset+512;i+=4){
  uint32_t raw=((uint32_t)mic_dma[i]<<16)|mic_dma[i+1];
  uint32_t u=raw>>8;int32_t v=(u&0x800000)?(int32_t)u-16777216:(int32_t)u;
  if(mic_wr-mic_rd>=MIC_RING){mic_gaps++;continue;}
  mic_ring[mic_wr&(MIC_RING-1)]=v/8388608.0f;__DMB();mic_wr++;
 }
}
void BG_I2SHalf(void){mic_copy(0);}
void BG_I2SFull(void){mic_copy(512);}
#else
void BG_I2SHalf(void){}
void BG_I2SFull(void){}
#endif
void BG_I2SError(void){mic_errors++;}
void BG_UARTDone(void){tx_busy=false;}
static void service_mic(void){
 static uint32_t old_gaps,old_errors;
 if(mic_gaps!=old_gaps || mic_errors!=old_errors){
  uint32_t mask=__get_PRIMASK();__disable_irq();mic_rd=mic_wr;__set_PRIMASK(mask);
  old_gaps=mic_gaps;old_errors=mic_errors;bg_dsp_reset(&mic);mic_ms=0;
 }
 unsigned budget=512;
 while(mic_rd!=mic_wr && budget--){
  __DMB();float v=mic_ring[mic_rd&(MIC_RING-1)];__DMB();mic_rd++;
  if(!gated)continue;
  if(mic.n==0)mic_block_section=track.section;
  if(mic_block_section!=track.section)mic_block_section=-1;
  if(bg_dsp_push(&mic,v)){bg_dsp_finish(&mic,BG_MIC_FS,&mf);mic_ms=HAL_GetTick();mic_section=mic_block_section;}
 }
}
static void service_load(void){
#if BG_ENABLE_LOAD
 if(HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_0)!=GPIO_PIN_RESET)return;
 uint32_t v=0,mask=__get_PRIMASK();__disable_irq();
 for(int i=0;i<24;i++){
  HAL_GPIO_WritePin(GPIOC,GPIO_PIN_1,GPIO_PIN_SET);delay_us(1);
  v=(v<<1)|(HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_0)==GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOC,GPIO_PIN_1,GPIO_PIN_RESET);delay_us(1);
 }
 HAL_GPIO_WritePin(GPIOC,GPIO_PIN_1,GPIO_PIN_SET);delay_us(1);
 HAL_GPIO_WritePin(GPIOC,GPIO_PIN_1,GPIO_PIN_RESET);__set_PRIMASK(mask);
 load_raw=(v&0x800000)?(int32_t)v-16777216:(int32_t)v;
 load_kg=(load_raw-(float)BG_LOAD_TARE)/BG_LOAD_COUNTS_PER_KG;
 load_ms=HAL_GetTick();load_ok=v!=0x7fffff && v!=0x800000;
#endif
}
static void emit(uint32_t now){
 if(tx_busy){tx_drops++;return;}
 char uidhex[21];for(unsigned i=0;i<last_uid_len;i++)snprintf(uidhex+2*i,3,"%02X",last_uid[i]);uidhex[2*last_uid_len]=0;
 unsigned warn=0;for(int i=0;i<BG_TAGS;i++)if(track.synced && track.tag[i].streak>=BG_MISSING_STREAK)warn|=1u<<i;
 HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,(warn || !vib_ok || (BG_LOCALIZATION && !rf_ok))?GPIO_PIN_SET:GPIO_PIN_RESET);
 int n=snprintf(packet,sizeof packet,
 "{\"v\":1,\"node\":%d,\"seq\":%lu,\"ms\":%lu,\"calibrated\":%d,\"gated\":%d,\"speed_mps\":%.5f,\"load_raw\":%ld,\"load_kg\":%.4f,\"load_valid\":%d,\"sync\":%d,\"section\":%d,\"position_counts\":%lld,\"uid\":\"%s\",\"uid_ms\":%lu,\"uid_latency_us\":%lu,\"rf_ok\":%d,\"rf_warning_mask\":%u,\"rf_passes\":[%lu,%lu,%lu,%lu],\"rf_misses\":[%lu,%lu,%lu,%lu],\"rf_streak\":[%lu,%lu,%lu,%lu],\"vib_valid\":%d,\"vib_ms\":%lu,\"vib_section\":%d,\"vib_rms_g\":[%.5f,%.5f,%.5f],\"vib_crest\":[%.3f,%.3f,%.3f],\"vib_kurtosis\":[%.3f,%.3f,%.3f],\"vib_hf_fraction\":[%.4f,%.4f,%.4f],\"vib_peak_hz\":[%.1f,%.1f,%.1f],\"vib_envelope_ac_rms_g\":[%.5f,%.5f,%.5f],\"mic_valid\":%d,\"mic_ms\":%lu,\"mic_section\":%d,\"mic_rms_fs\":%.6f,\"mic_crest\":%.3f,\"mic_kurtosis\":%.3f,\"mic_hf_fraction\":%.4f,\"mic_peak_hz\":%.1f,\"mic_envelope_ac_rms_fs\":%.6f,\"fifo_gaps\":%lu,\"mic_gaps\":%lu,\"mic_errors\":%lu,\"spi_errors\":%lu,\"rf_errors\":%lu,\"sync_errors\":%lu,\"unknown_uids\":%lu,\"tx_drops\":%lu,\"health_index\":null}",
 BG_NODE_ID,(unsigned long)seq++,(unsigned long)now,BG_CALIBRATED,gated && !BG_REMOTE_GATE,speed,(long)load_raw,load_kg,load_ok && now-load_ms<500,track.synced,track.section,(long long)track.position,uidhex,(unsigned long)uid_ms,(unsigned long)uid_latency,rf_ok,warn,
 (unsigned long)track.tag[0].passes,(unsigned long)track.tag[1].passes,(unsigned long)track.tag[2].passes,(unsigned long)track.tag[3].passes,
 (unsigned long)track.tag[0].misses,(unsigned long)track.tag[1].misses,(unsigned long)track.tag[2].misses,(unsigned long)track.tag[3].misses,
 (unsigned long)track.tag[0].streak,(unsigned long)track.tag[1].streak,(unsigned long)track.tag[2].streak,(unsigned long)track.tag[3].streak,
 gated && vib_ok && vib_ms && now-vib_ms<200,(unsigned long)vib_ms,vib_section,
 vf[0].rms,vf[1].rms,vf[2].rms,vf[0].crest,vf[1].crest,vf[2].crest,vf[0].kurtosis,vf[1].kurtosis,vf[2].kurtosis,
 vf[0].hf_fraction,vf[1].hf_fraction,vf[2].hf_fraction,vf[0].peak_hz,vf[1].peak_hz,vf[2].peak_hz,vf[0].envelope_ac_rms,vf[1].envelope_ac_rms,vf[2].envelope_ac_rms,
 gated && mic_ms && now-mic_ms<200,(unsigned long)mic_ms,mic_section,mf.rms,mf.crest,mf.kurtosis,mf.hf_fraction,mf.peak_hz,mf.envelope_ac_rms,
 (unsigned long)fifo_gaps,(unsigned long)mic_gaps,(unsigned long)mic_errors,(unsigned long)spi_errors,(unsigned long)rf_errors,(unsigned long)track.sync_errors,(unsigned long)track.unknown_reads,(unsigned long)tx_drops);
 if(n<=0 || n>(int)sizeof(packet)-8){tx_drops++;return;}
 uint16_t crc=bg_crc16((uint8_t *)packet,(size_t)n);
 int tail=snprintf(packet+n,sizeof(packet)-(size_t)n,"*%04X\n",crc);
 tx_busy=true;
 if(HAL_UART_Transmit_DMA(&huart2,(uint8_t *)packet,(uint16_t)(n+tail))!=HAL_OK){tx_busy=false;tx_drops++;}
}
void BG_Init(void){
 CoreDebug->DEMCR|=CoreDebug_DEMCR_TRCENA_Msk;DWT->CYCCNT=0;DWT->CTRL|=DWT_CTRL_CYCCNTENA_Msk;
 HAL_GPIO_WritePin(GPIOB,GPIO_PIN_6,GPIO_PIN_SET);HAL_GPIO_WritePin(GPIOA,GPIO_PIN_4,GPIO_PIN_SET);
 HAL_GPIO_WritePin(GPIOC,GPIO_PIN_1,GPIO_PIN_RESET);
 bg_config c={BG_LOOP_COUNTS,BG_WINDOW_COUNTS,BG_METRES_PER_COUNT,BG_TAG_CONFIG};
 config_ok=bg_tracker_init(&track,&c);
 sensor.read_reg=acc_read;sensor.write_reg=acc_write;sensor.mdelay=HAL_Delay;
 vib_ok=acc_init();
#if BG_LOCALIZATION
 HAL_GPIO_WritePin(GPIOC,GPIO_PIN_7,GPIO_PIN_RESET);HAL_Delay(2);
 HAL_GPIO_WritePin(GPIOC,GPIO_PIN_7,GPIO_PIN_SET);HAL_Delay(50);
 rf_ok=mfrc522_init(&reader)==0;
#endif
 HAL_TIM_Encoder_Start(&htim2,TIM_CHANNEL_ALL);
 raw_previous=__HAL_TIM_GET_COUNTER(&htim2);
#if BG_ENCODER_SIGN == -1
 raw_previous=0u-raw_previous;
#endif
 bg_encoder_update(&track,raw_previous);last_speed=HAL_GetTick();
#if BG_ENABLE_MIC
 /* F4 HAL doubles Size for 24-bit words: 512 words -> 1024 halfwords. */
 if(HAL_I2S_Receive_DMA(&hi2s2,mic_dma,512)!=HAL_OK)mic_errors++;
#endif
}
void BG_Task(void){
 uint32_t now=HAL_GetTick();
 uint32_t raw=__HAL_TIM_GET_COUNTER(&htim2);
#if BG_ENCODER_SIGN == -1
 raw=0u-raw;
#endif
 bg_encoder_update(&track,raw);
 if(now-last_speed>=100){
  uint32_t u=raw-raw_previous;int64_t delta=u<=2147483647u?(int64_t)u:(int64_t)u-4294967296LL;
  speed=(float)delta*BG_METRES_PER_COUNT*1000/(now-last_speed);
  raw_previous=raw;last_speed=now;
 }
 service_load();
 gated=BG_CALIBRATED && config_ok && (BG_REMOTE_GATE || (speed>=BG_MIN_SPEED && load_ok && now-load_ms<500 && load_kg>=BG_MIN_LOAD_KG));
 if(gated!=was_gated){reset_vib();bg_dsp_reset(&mic);mic_ms=0;was_gated=gated;}
 service_vib();service_mic();
#if BG_LOCALIZATION
 if(rf_ok && now-last_rf>=20){
  uint8_t uid[10],len=0;uint32_t start=DWT->CYCCNT;int r=mfrc522_poll(&reader,uid,&len);
  uint32_t elapsed=(uint32_t)(DWT->CYCCNT-start)/(SystemCoreClock/1000000U);last_rf=HAL_GetTick();
  if(r==1){memcpy(last_uid,uid,len);last_uid_len=len;uid_ms=last_rf;uid_latency=elapsed;
   if(BG_CALIBRATED && config_ok && speed>=BG_MIN_SPEED)bg_tag_read(&track,uid,len,elapsed);
  } else if(r<0){rf_errors++;uint8_t version=0;if(rf_read(0x37,&version) || (version!=0x91 && version!=0x92)){rf_ok=false;bg_tracker_unsync(&track);}}
 }
 if(BG_CALIBRATED && rf_ok && speed>=BG_MIN_SPEED)bg_tracker_advance(&track);
#endif
 /* Recovery is explicit in quality counters; an absent sensor never supplies zeros as valid data. */
 if(now-last_retry>=2000){last_retry=now;if(!vib_ok)vib_ok=acc_init();
#if BG_LOCALIZATION
 if(!rf_ok)rf_ok=mfrc522_init(&reader)==0;
#endif
 }
 if(now-last_packet>=1000){last_packet=now;emit(now);}
 HAL_IWDG_Refresh(&hiwdg); /* only after one bounded acquisition/processing pass */
}
