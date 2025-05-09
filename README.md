An [esp-idf](https://github.com/espressif/esp-idf) component for interfacing
with the [NAU7802](https://www.nuvoton.com/export/resource-files/en-us--DS_NAU7802_DataSheet_EN_Rev2.6.pdf)
24-bit ADC, especially as packaged on e.g. the
[Adafruit NAU7802](https://www.adafruit.com/product/4538) and
[SparkFun Qwiic Scale](https://www.sparkfun.com/products/15242). This is
similar to the [HX711](https://www.digikey.com/htmldatasheets/production/1836471/0/0/1/hx711.html),
and usually used for working with load cells.

The NAU7802 is available as a discrete component in both
[KGI](https://www.nuvoton.com/products/smart-home-audio/audio-converters/precision-adc-series/nau7802kgi)
(PDIP16L through-hole) and
[SGI](https://www.nuvoton.com/products/smart-home-audio/audio-converters/precision-adc-series/nau7802sgi/)
(16L SOP SMD) form factors.
It operates over I²C. It can accept between 2.7V and 5.5V, and operates
between -40C and 85C.

This is present in the [Component Registry](https://components.espressif.com/)
as [dankamongmen/nau7802](https://components.espressif.com/components/dankamongmen/nau7802).

[![Component Registry](https://components.espressif.com/components/dankamongmen/nau7802/badge.svg)](https://components.espressif.com/components/dankamongmen/nau7802)

## Using the NAU7802

The NAU7802 exposes a two-channel differential 24-bit ADC over an
I²C-compatible wire interface (plus optional extensions).

A load cell will connect to the NAU7802 using four wires: A+/A- and E+/E-. The
latter are outputs from the NAU7802 carrying the excitation voltage to the load
cell, and ought be as constant as possible. E- will usually be connected to
ground, and E+ to the NAU7802's REFP or AVDD pins. The former are outputs
from the load cell, and connect to VinXP and VinXN, respectively.

A differential ADC measures the difference between two signals. Though both the
signals are positive voltages, the result will be negative if the N line is
higher than the P line (though the lines are "negative" and "positive", they
are carrying positive signals). The smallest negative number corresponds to a
0 on the positive line and a maximum signal on the negative line. The largest
positive number corresponds to a maximum signal on the positive line and a 0
on the negative line. A 0 corresponds to equal signals of any value.

The primary advantage of a properly routed differential signal is resistance to
path noise and common-mode noise, which ought affect both signals equally.
Other benefits include easy handling of large DC offsets.

A load cell typically provides its ground to the negative input, and is thus
a single-ended sensor. A single-ended sensor ought never generate a negative
value in normal use. Zero load will ideally result in a zero signal. One
effect is that our 24-bit differential ADC immediately loses a bit, since
the negative dynamic range is unused.

### Channels

The NAU7802 presents two channels, but switching between them is an expensive
operation (including an internal calibration). The channels have different
input capacitance. If you only need one channel, a capacitor to ground can tie
together Vin2N and Vin2P to eliminate some noise; suggested values are 330pF
for 3.3V AVDD and 680pF for 4.5V AVDD. If this is done, be sure to call
`nau7802_set_pga_cap(i2ch, true)`.

### Power

The NAU7802 can accept between 2.7V and 5.5V for its digital input DVDD. It
should generally be the same power source as your MCU (e.g. 3.3V for ESP32).
AVDD powers the analog components, and must not exceed DVDD. It can either
come from an external power source, or be converted from DVDD using an
internal LDO. By default, the NAU7802 expects a voltage on AVDD. To instead
use the LDO, call `nau7802_enable_ldo()` with the greatest `nau7802_ldo_level`
*less than* your DVDD (e.g. `NAU7802_LDO_30V` when powered by a 3.3V DVDD).
`pga_ldomode` sets the `LDOMODE` bit of the `PGA` register, allowing use
of a higher ESR capacitor…but I'm not quite sure what capacitor it refers to.
