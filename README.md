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
