* 0.5.0 (2025-04-21)
  * remove `nau7802_multisample()`, which was fundamentally unsound.

* 0.4.0 (2025-04-21)
  * add `nau7802_multisample()` and `NAU7802_MULTISAMPLE_DEFAULT` constant
  * fix scaling factor in `nau7802_read_scaled()`. this function might get
    removed in the future.

* 0.3.3 (2025-04-19)
  * renamed `nau7802_setgain()` to `nau7802_set_gain()` to match everything
    else. `nau7802_set_gain()` now accepts a gain of 0 to disable the PGA.

* 0.3.1 (2025-04-17)
  * add `nau7802_set_therm()` to enable and disable reading from the on-chip
    temperature sensor (vs. VINx).

* 0.3.0 (2025-04-17)
  * add `nau7802_set_bandgap_chop()` to enable and disable
    the, you guessed it, bandgap chopper.

* 0.2.2 (2025-03-30)
  * always enable internal LDO in `nau7802_enable_ldo()`,
    and always enable AVDD pin input in `nau7802_disable_ldo()`.

* 0.2.1 (2025-03-30)
  * corrected register reference in `nau7802_disable_ldo()`
    that led to undefined behavior.

* 0.2.0 (2025-03-30)
  * add `nau7802_set_deepsleep()` to enter and leave deep sleep

* 0.1.1 (2025-03-30)
  * corrected sense of test in `nau7802_enable_ldo()` that left
    the function unusable.
