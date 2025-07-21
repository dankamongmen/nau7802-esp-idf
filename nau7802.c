#include "nau7802.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#define TIMEOUT_MS 1000 // FIXME why 1s?

static const char* TAG = "nau";

// Bits within the PU_CTRL register
typedef enum {
  NAU7802_PU_CTRL_RR = 1,
  NAU7802_PU_CTRL_PUD = 2,
  NAU7802_PU_CTRL_PUA = 4,
  NAU7802_PU_CTRL_PUR = 8,
  NAU7802_PU_CTRL_CS = 16,
  NAU7802_PU_CTRL_CR = 32,
  NAU7802_PU_CTRL_OSCS = 64,
  NAU7802_PU_CTRL_AVDDS = 128,
} PU_CTRL_Bits;

// Bits within the PGA register
typedef enum {
  NAU7802_PGA_CHPDIS = 1,   // chopper disable
  NAU7802_PGA_RES1 = 2,     // reserved
  NAU7802_PGA_RES2 = 4,     // reserved
  NAU7802_PGA_PGAINV = 8,   // invert pga phase
  NAU7802_PGA_BYPASS = 16,  // pga bypass
  NAU7802_PGA_DISBUF = 32,  // disable pga output buffer
  NAU7802_PGA_LDOMODE = 64, // pga ldomode
  NAU7802_PGA_OTPSEL = 128, // output select
} PGA_Bits;

typedef enum {
  NAU7802_PU_CTRL = 0x00,
  NAU7802_CTRL1,
  NAU7802_CTRL2,
  NAU7802_OCAL1_B2,
  NAU7802_OCAL1_B1,
  NAU7802_OCAL1_B0,
  NAU7802_GCAL1_B3,
  NAU7802_GCAL1_B2,
  NAU7802_GCAL1_B1,
  NAU7802_GCAL1_B0,
  NAU7802_OCAL2_B2,
  NAU7802_OCAL2_B1,
  NAU7802_OCAL2_B0,
  NAU7802_GCAL2_B3,
  NAU7802_GCAL2_B2,
  NAU7802_GCAL2_B1,
  NAU7802_GCAL2_B0,
  NAU7802_I2C_CONTROL,
  NAU7802_ADCO_B2,
  NAU7802_ADCO_B1,
  NAU7802_ADCO_B0,
  NAU7802_ADC = 0x15, //Shared ADC and OTP 32:24
  NAU7802_OTP_B1,     //OTP 23:16 or 7:0?
  NAU7802_OTP_B0,     //OTP 15:8
  NAU7802_PGA = 0x1B,
  NAU7802_PGA_PWR = 0x1C,
  NAU7802_DEVICE_REV = 0x1F,
} registers;

#define NAU7802_ADDRESS 0x2A

int nau7802_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cnau){
  const unsigned addr = NAU7802_ADDRESS;
  esp_err_t e = i2c_master_probe(i2c, addr, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) detecting NAU7802 at 0x%02x", esp_err_to_name(e), addr);
    return -1;
  }
  i2c_device_config_t devcfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = addr,
    .scl_speed_hz = 100000,
	};
  ESP_LOGI(TAG, "successfully detected NAU7802 at 0x%02x", addr);
  if((e = i2c_master_bus_add_device(i2c, &devcfg, i2cnau)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) adding nau7802 i2c device", esp_err_to_name(e));
    return -1;
  }
  return 0;
}

// FIXME we'll probably want this to be async
static int
nau7802_xmit(i2c_master_dev_handle_t i2c, const void* buf, size_t blen){
  esp_err_t e = i2c_master_transmit(i2c, buf, blen, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) transmitting %zuB via I2C", esp_err_to_name(e), blen);
    return -1;
  }
  return 0;
}

int nau7802_reset(i2c_master_dev_handle_t i2c){
  uint8_t buf[] = {
    NAU7802_PU_CTRL,
    NAU7802_PU_CTRL_RR
  };
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "reset NAU7802");
  return 0;
}

// get the single byte of some register
static inline esp_err_t
nau7802_readreg(i2c_master_dev_handle_t i2c, registers reg,
                const char* regname, uint8_t* val){
  uint8_t r = reg;
  esp_err_t e;
  if((e = i2c_master_transmit_receive(i2c, &r, 1, val, 1, TIMEOUT_MS)) != ESP_OK){
    ESP_LOGE(TAG, "error (%s) requesting %s via I2C", esp_err_to_name(e), regname);
    return e;
  }
  ESP_LOGD(TAG, "got %s: 0x%02x", regname, *val);
  return ESP_OK;
}

static inline esp_err_t
nau7802_pu_ctrl(i2c_master_dev_handle_t i2c, uint8_t* val){
  return nau7802_readreg(i2c, NAU7802_PU_CTRL, "PU_CTRL", val);
}

static inline esp_err_t
nau7802_ctrl1(i2c_master_dev_handle_t i2c, uint8_t* val){
  return nau7802_readreg(i2c, NAU7802_CTRL1, "CTRL1", val);
}

static inline esp_err_t
nau7802_ctrl2(i2c_master_dev_handle_t i2c, uint8_t* val){
  return nau7802_readreg(i2c, NAU7802_CTRL2, "CTRL2", val);
}

static inline esp_err_t
nau7802_pga(i2c_master_dev_handle_t i2c, uint8_t* val){
  return nau7802_readreg(i2c, NAU7802_PGA, "PGA", val);
}

static int
nau7802_internal_calibrate(i2c_master_dev_handle_t i2c){
  uint8_t r;
  if(nau7802_ctrl2(i2c, &r)){
    return -1;
  }
  // queue the internal calibration
  const uint8_t buf[] = {
    NAU7802_CTRL2,
    (r | 0x4) & 0xfc // set CALS, zero out CALMOD
  };
  ESP_LOGI(TAG, "running internal offset calibration");
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  do{
    if(nau7802_ctrl2(i2c, &r)){
      return -1;
    }
  }while(r & 0x4);
  bool failed = (r & 0x8);
  ESP_LOGI(TAG, "completed internal offset calibration with%s error", failed ? "" : "out");
  return failed ? -1 : 0;
}

// the power on sequence is:
//  * send a reset
//  * set PUD and PUA in PU_CTRL
//  * check for PUR bit in PU_CTRL after short delay
//  * set CS in PU_CTRL
//  * set 0x30 in ADC_CTRL (REG_CHPS)
//  * run an internal offset calibration (CALS/CALMOD)
//
// we ought also "wait through six cycles of data conversion" (1.14),
// but are not yet doing so.
int nau7802_poweron(i2c_master_dev_handle_t i2c){
  uint8_t buf[] = {
    NAU7802_PU_CTRL,
    NAU7802_PU_CTRL_PUD | NAU7802_PU_CTRL_PUA
  };
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  // the data sheet says we must allow up to 200 microseconds for the device
  // to power up. we'll allow it a full millisecond.
  vTaskDelay(pdMS_TO_TICKS(1));
  if(nau7802_pu_ctrl(i2c, &buf[1])){
    return -1;
  }
  if(!(buf[1] & NAU7802_PU_CTRL_PUR)){
    ESP_LOGE(TAG, "didn't see powered-on bit");
    return -1;
  }
  esp_err_t e;
  buf[1] |= NAU7802_PU_CTRL_CS;
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  buf[0] = NAU7802_ADC;
  if(nau7802_readreg(i2c, buf[0], "ADC", &buf[1])){
    return -1;
  }
  buf[1] |= 0x30; // set 0x30 REG_CHPS
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  buf[0] = NAU7802_DEVICE_REV;
  e = i2c_master_transmit_receive(i2c, buf, 1, &buf[1], 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error %d reading device revision code", e);
    return -1;
  }
  buf[1] &= 0xf;
  if(buf[1] != 0xf){
    ESP_LOGW(TAG, "unexpected revision id 0x%x", buf[1]);
  }else{
    ESP_LOGI(TAG, "device revision code: 0x%x", buf[1]);
  }
  if(nau7802_internal_calibrate(i2c)){
    return -1;
  }
  return 0;
}

// FIXME also have to cut PGA <= 2
int nau7802_set_therm(i2c_master_dev_handle_t i2c, bool enabled){
  uint8_t buf[] = {
    NAU7802_I2C_CONTROL,
    0x0
  };
  if(nau7802_readreg(i2c, buf[0], "I2C_CONTROL", &buf[1])){
    return -1;
  }
  if(enabled){
    buf[1] |= 0x02; // set 0x02 TS
  }else{
    buf[1] &= 0xfd; // clear 0x02 TS
  }
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "set temperature sensor bit");
  // shouldn't need to calibrate here
  return 0;
}

int nau7802_set_bandgap_chop(i2c_master_dev_handle_t i2c, bool enabled){
  uint8_t buf[] = {
    NAU7802_I2C_CONTROL,
    0x0
  };
  if(nau7802_readreg(i2c, buf[0], "I2C_CONTROL", &buf[1])){
    return -1;
  }
  if(enabled){ // disabled is 1
    buf[1] &= NAU7802_PGA_CHPDIS; // clear 0x01 BGPCP
  }else{
    buf[1] |= NAU7802_PGA_CHPDIS; // set 0x01 BGPCP
  }
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "set bandgap chopper bit");
  // shouldn't need to calibrate here
  return 0;
}

int nau7802_set_pga_cap(i2c_master_dev_handle_t i2c, bool enabled){
  uint8_t buf[] = {
    NAU7802_PGA_PWR,
    0x0
  };
  if(nau7802_readreg(i2c, buf[0], "PGA_PWR", &buf[1])){
    return -1;
  }
  if(enabled){
    buf[1] |= 0x80; // set 0x80 PGA_CAP_EN
  }else{
    buf[1] &= 0x7f; // clear 0x80 PGA_CAP_EN
  }
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  ESP_LOGI(TAG, "set pga cap bit");
  if(nau7802_internal_calibrate(i2c)){
    return -1;
  }
  return 0;
}

static int
nau7802_set_pgabypass(i2c_master_dev_handle_t i2c, bool bypass){
  uint8_t buf[] = {
    NAU7802_PGA,
    0xff
  };
  if(nau7802_pga(i2c, &buf[1])){
    return -1;
  }
  if(bypass){
    buf[1] |= NAU7802_PGA_BYPASS;
  }else{
    buf[1] &= ~NAU7802_PGA_BYPASS;
  }
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  return 0;
}

int nau7802_set_gain(i2c_master_dev_handle_t i2c, unsigned gain){
  uint8_t rbuf;
  if(gain > 128 || (gain != 0 && (gain & (gain - 1)))){
    ESP_LOGE(TAG, "illegal gain value %u", gain);
    return -1;
  }
  if(gain == 0){
    return nau7802_set_pgabypass(i2c, true);
  }
  // pga bypass is disabled by default, so maybe just track this rather than
  // writing it every time? eh, setting gain is an infrequent operation.
  if(nau7802_set_pgabypass(i2c, false)){
    return -1;
  }
  uint8_t buf[] = {
    NAU7802_CTRL1,
    0xff
  };
  if(nau7802_ctrl1(i2c, &buf[1])){
    return -1;
  }
  buf[1] &= 0xf8;
  ESP_LOGI(TAG, "read ctrl1 0x%02x", buf[1]);
  if(gain >= 16){
    buf[1] |= 0x4;
  }
  if(gain > 32 || (gain < 16 && gain > 2)){
    buf[1] |= 0x2;
  }
  if(gain == 128 || gain == 32 || gain == 8 || gain == 2){
    buf[1] |= 0x1;
  }
  ESP_LOGI(TAG, "writing ctrl1 with 0x%02x", buf[1]);
  esp_err_t e = i2c_master_transmit_receive(i2c, buf, 2, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error %s writing CTRL1", esp_err_to_name(e));
    return -1;
  }
  if(rbuf != buf[1]){
    ESP_LOGE(TAG, "CTRL1 reply 0x%02x didn't match 0x%02x", rbuf, buf[1]);
    return -1;
  }
  ESP_LOGI(TAG, "set gain");
  if(nau7802_internal_calibrate(i2c)){
    return -1;
  }
  return 0;
}

int nau7802_set_sample_rate(i2c_master_dev_handle_t i2c, unsigned rate){
  uint8_t rbuf;
  if(rate != 10 && rate != 20 && rate != 40 && rate != 80 && rate != 320){
    ESP_LOGE(TAG, "illegal rate value %u", rate);
    return -1;
  }
  uint8_t buf[] = {
    NAU7802_CTRL2,
    0xff
  };

  if(nau7802_ctrl2(i2c, &buf[1])){
    return -1;
  }
  buf[1] &= 0x10001111;
  if(rate == 10){
    buf[1] |= 0b000 << 4;
  }else if(rate == 20){
    buf[1] |= 0b001 << 4;
  }else if(rate == 40){
    buf[1] |= 0b010 << 4;
  }else if(rate == 80){
    buf[1] |= 0b011 << 4;
  }else if(rate == 320){
    buf[1] |= 0b111 << 4;
  }
  ESP_LOGI(TAG, "writing ctrl2 with 0x%02x", buf[1]);
  esp_err_t e = i2c_master_transmit_receive(i2c, buf, 2, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error %s writing CTRL2", esp_err_to_name(e));
    return -1;
  }
  if(rbuf != buf[1]){
    ESP_LOGE(TAG, "CTRL2 reply 0x%02x didn't match 0x%02x", rbuf, buf[1]);
    return -1;
  }
  ESP_LOGI(TAG, "set rate");
  if(nau7802_internal_calibrate(i2c)){
    return -1;
  }
  return 0;
}

// set the PGA LDOMODE (*not* the master AVDDS/LDO switch)
static int
nau7802_set_pgaldomode(i2c_master_dev_handle_t i2c, bool ldomode){
  uint8_t buf[] = {
    NAU7802_PGA,
    0xff
  };
  if(nau7802_pga(i2c, &buf[1])){
    return -1;
  }
  if(ldomode){
    buf[1] |= NAU7802_PGA_LDOMODE;
  }else{
    buf[1] &= ~NAU7802_PGA_LDOMODE;
  }
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  return 0;
}

// select whether AVDD is sourced from the internal LDO, or the AVDD pin
// input (default configuration). false for AVDD pin.
static int
nau7802_set_ldo(i2c_master_dev_handle_t i2c, bool ldomode){
  uint8_t buf[] = {
    NAU7802_PU_CTRL,
    0xff
  };
  if(nau7802_pu_ctrl(i2c, &buf[1])){
    return -1;
  }
  if(ldomode){
    buf[1] |= NAU7802_PU_CTRL_AVDDS;
  }else{
    buf[1] &= ~NAU7802_PU_CTRL_AVDDS;
  }
  if(nau7802_xmit(i2c, buf, sizeof(buf))){
    return -1;
  }
  return 0;
}

int nau7802_disable_ldo(i2c_master_dev_handle_t i2c){
  if(nau7802_set_ldo(i2c, false)){
    return -1;
  }
  ESP_LOGI(TAG, "enabled avdd pin input");
  if(nau7802_internal_calibrate(i2c)){
    return -1;
  }
  return 0;
}

int nau7802_enable_ldo(i2c_master_dev_handle_t i2c, nau7802_ldo_level mode,
                       bool pga_ldomode){
  if(mode > NAU7802_LDO_24V || mode < NAU7802_LDO_45V){
    ESP_LOGW(TAG, "illegal LDO mode %d", mode);
    return -1;
  }
  uint8_t buf[] = {
    NAU7802_CTRL1, // we need first set the LDO voltage in CTRL1 (VLDO)
    0xff
  };
  uint8_t rbuf;
  if(nau7802_ctrl1(i2c, &rbuf)){
    return -1;
  }
  buf[1] = rbuf & 0xc7; // VLDO is bits 5, 4, and 3 (0x34)
  buf[1] |= mode << 3u;
  ESP_LOGI(TAG, "requesting VLDO mode 0x%02x (0x%02x)", (rbuf & 0xc7) | (mode << 3u), buf[1]);
  esp_err_t e = i2c_master_transmit_receive(i2c, buf, 2, &rbuf, 1, TIMEOUT_MS);
  if(e != ESP_OK){
    ESP_LOGE(TAG, "error (%s) writing CTRL1", esp_err_to_name(e));
    return -1;
  }
  if(rbuf != buf[1]){
    ESP_LOGE(TAG, "CTRL1 reply 0x%02x didn't match 0x%02x", rbuf, buf[1]);
    return -1;
  }
  if(nau7802_set_pgaldomode(i2c, pga_ldomode)){
    return -1;
  }
  if(nau7802_set_ldo(i2c, true)){
    return -1;
  }
  ESP_LOGI(TAG, "enabled internal ldo");
  if(nau7802_internal_calibrate(i2c)){
    return -1;
  }
  return 0;
}

int nau7802_read_scaled(i2c_master_dev_handle_t i2c, float* val, uint32_t scale){
  int32_t v;
  if(nau7802_read(i2c, &v)){
    return -1;
  }
  const float ADCMAX = 1u << 23u; // can be represented perfectly in 32-bit float
  const float adcper = ADCMAX / scale;
  *val = v / adcper;
  ESP_LOGD(TAG, "converted raw %lu to %f", v, *val);
  return 0;
}

static esp_err_t
nau7802_read_internal(i2c_master_dev_handle_t i2c, int32_t* val, bool lognodata){
  uint8_t r0, r1, r2;
  esp_err_t e;
  if((e = nau7802_pu_ctrl(i2c, &r0)) != ESP_OK){
    return e;
  }
  if(!(r0 & NAU7802_PU_CTRL_CR)){
    if(lognodata){
      ESP_LOGE(TAG, "data not yet ready at ADC (0x%02x)", r0);
    }
    return ESP_ERR_NOT_FINISHED;
  }
  if((e = nau7802_readreg(i2c, NAU7802_ADCO_B2, "ADCO_B2", &r2)) != ESP_OK){
    return e;
  }
  if((e = nau7802_readreg(i2c, NAU7802_ADCO_B1, "ADCO_B1", &r1)) != ESP_OK){
    return e;
  }
  if((e = nau7802_readreg(i2c, NAU7802_ADCO_B0, "ADCO_B0", &r0)) != ESP_OK){
    return e;
  }
  // FIXME chop to noise_free_bits according to AVDD and PGA. we never have
  // more than 20 noise free bits nor less than 16, so modifications are
  // restricted to r0.
  const int32_t mask = 0xf0;
  *val = (r2 << 16u) + (r1 << 8u) + (r0 & mask);
  // if the most significant bit of the 24-bit output is set, then propagate
  // it to the 32-bit return value.
  if (*val & 0x800000) {
    *val |= 0xFF000000;
  }
  ESP_LOGD(TAG, "ADC reads: %u %u %u full %lu 0x%08x", r0, r1, r2, *val, *val);
  return ESP_OK;
}

int nau7802_read(i2c_master_dev_handle_t i2c, int32_t* val){
  return nau7802_read_internal(i2c, val, true);
}

esp_err_t nau7802_multisample(i2c_master_dev_handle_t i2c, float* val, unsigned n){
  float sum = 0;
  for(unsigned z = 0 ; z < n ; ++z){
    int32_t v;
    esp_err_t e;
    do{
      e = nau7802_read_internal(i2c, &v, false);
      if(e != ESP_OK && e != ESP_ERR_NOT_FINISHED){
        return e;
      }
    }while(e != ESP_OK);
    // accumulating into this 32-bit float can result in inaccurate maths if we
    // go beyond 1 << 24u
    sum += v;
  }
  *val = sum / n;
  return ESP_OK;
}

int nau7802_set_deepsleep(i2c_master_dev_handle_t i2c, bool powerdown){
  uint8_t buf[] = {
    NAU7802_PU_CTRL,
    0xff
  };
  if(nau7802_pu_ctrl(i2c, &buf[1])){
    return -1;
  }
  const uint8_t mask = (NAU7802_PU_CTRL_PUD | NAU7802_PU_CTRL_PUA);
  if(powerdown){
    if(buf[1] & mask){
      buf[1] &= ~mask;
      if(nau7802_xmit(i2c, buf, sizeof(buf))){
        return -1;
      }
      // we ought also "wait through six cycles of data conversion" (1.14), but
      // are not yet doing so.
    }else{
      ESP_LOGE(TAG, "analog is already powered down");
    }
  }else{
    if((buf[1] & mask) != mask){
      buf[1] |= mask;
      if(nau7802_xmit(i2c, buf, sizeof(buf))){
        return -1;
      }
    }else{
      ESP_LOGE(TAG, "analog is already powered up");
    }
  }
  return 0;
}
