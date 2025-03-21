#ifndef DANKDRYER_NAU7802
#define DANKDRYER_NAU7802

// ESP-IDF component for working with Nuvatron NAU7802 ADCs.

#include <driver/i2c_master.h>

// probe the I2C bus for an NAU7802. if it is found, configure i2cnau
// as a device handle for it, and return 0. return non-zero on error.
int nau7802_detect(i2c_master_bus_handle_t i2c, i2c_master_dev_handle_t* i2cnau);

// send the reset command w/ timeout. returns non-zero on error.
int nau7802_reset(i2c_master_dev_handle_t i2c);

// send the poweron command w/ timeout. returns non-zero on error.
// this sets PUA+PUD+CS, verifies PUR after a short delay, sets REG_CHPS, and
// runs an internal offset calibration (as recommended by the datasheet).
// it would not be unwise to call nau7802_reset() first. afterwards, it might
// be desirable to call one or more of:
//
//  nau7802_enable_ldo() if you wish to use the internal LDO instead
//   of the value read on AVDD.
//  nau7802_set_gain()
//
int nau7802_poweron(i2c_master_dev_handle_t i2c);

// set the gain (default 1). can be any power of 2 from 1 to 128.
// confirms the set and returns 0 on success, non-zero on failure.
// triggers an internal calibration.
int nau7802_setgain(i2c_master_dev_handle_t i2c, unsigned gain);

// the NAU7802 ought receive on DVDD the same power source as that used
// by the host MCU (so long as it's not over 5.5V). AVDD can either
// receive external power (which must not exceed DVDD's voltage), or
// receive power from an internal LDO driven from DVDD. the default
// configuration is external AVDD. the LDO cannot drive higher than
// DVDD - 0.3.
//
// the greatest precision will be had by supplying a large AVDD (though
// this requires an equally large DVDD) with minimal noise. theoretically,
// the LDO might quiet a noisy DVDD compared to bringing DVDD directly
// into AVDD, but it might not. if using an ESP32 with a 3.3 switching
// input, either tie DVDD to AVDD (and leave the LDO disabled), or leave
// AVDD disconnected, and set NAU7802_LDO_30V.

// disable the internal LDO and use the value on the AVDD pin. this is the
// default configuration. triggers an internal calibration.
int nau7802_disable_ldo(i2c_master_dev_handle_t i2c);

// these need to map to exactly the VLDO values for CTRL1.
typedef enum {
  NAU7802_LDO_45V = 0,
  NAU7802_LDO_42V = 1,
  NAU7802_LDO_39V = 2,
  NAU7802_LDO_36V = 3,
  NAU7802_LDO_33V = 4,
  NAU7802_LDO_30V = 5,
  NAU7802_LDO_27V = 6,
  NAU7802_LDO_24V = 7,
} nau7802_ldo_level;

// enable the internal LDO, and ignore the value on the AVDD pin. the
// level must be at least 0.3V less than DVDD (this function sadly
// cannot verify that you've selected a valid level, because we don't
// know DVDD). pga_ldomode refers to the LDOMODE bit (0x40) in PGA. by default,
// a capacitor with subohm ESR ought be used, for higher DC gain / improved
// accuracy. instead, a capacitor with less than 5Ω ESR can be used, with
// improved stability / lower DC gain. indicate the second case with this
// function. this is only relevant if the internal LDO is being used for AVDD.
// triggers an internal calibration.
int nau7802_enable_ldo(i2c_master_dev_handle_t i2c, nau7802_ldo_level level,
                       bool pga_ldomode);

//  manage PGA_CAP_EN bit (0x80) in PWR_CTRL. in single-channel applications,
//  a capacitor can connect Vin2P and Vin2N for greater ENOB (the capacitance
//  is a function of AVDD; 330pF is recommended for 3.3V). if such a capacitor
//  is present, enable it with this function. triggers an internal calibration.
int nau7802_set_pga_cap(i2c_master_dev_handle_t i2c, bool enabled);

// read the 24-bit ADC into val. returns non-zero on error, in which case
// *val is undefined. this is the raw ADC value.
int nau7802_read(i2c_master_dev_handle_t i2c, int32_t* val);

// read the 24-bit ADC, interpreting it using some maximum value scale.
// i.e. if scale is 5000 (representing e.g. a small bar load cell of 5kg),
// the raw ADC value will be divided by 3355.4432 (1 << 24 / 5000).
// on success, val will hold some value less than scale.
int nau7802_read_scaled(i2c_master_dev_handle_t i2c, float* val, uint32_t scale);

#endif
