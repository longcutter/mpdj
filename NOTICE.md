# Third-Party Notices & Attributions

Polyend Tracker Community Edition (CE) incorporates or links against the following third-party open-source and binary components:

---

## 1. ARM CMSIS & CMSIS-DSP
* **Location**: `platforms/imx/CMSIS/`, `platforms/mk66/3rdparty/teensy_core/`
* **License**: Apache License 2.0 / BSD-3-Clause
* **Copyright**: Copyright (c) ARM Limited. All rights reserved.

## 2. NXP MCUXpresso SDK (Kinetis K66 & i.MX RT1050/RT1060)
* **Location**: `platforms/imx/drivers/`, `platforms/imx/device/`, `platforms/imx/usb/`, `platforms/imx/sdmmc/`, `platforms/mk66/3rdparty/SD/`, `platforms/mk66/3rdparty/I2C/`, `platforms/mk66/3rdparty/SDRAM/`
* **License**: BSD-3-Clause
* **Copyright**: Copyright (c) Freescale Semiconductor, Inc. / Copyright 2016-2022 NXP.

## 3. FatFs - Generic FAT Filesystem Module
* **Location**: `platforms/imx/fatfs/`, `platforms/mk66/3rdparty/SD/fatfs/`
* **License**: FatFs License (BSD-style)
* **Copyright**: Copyright (C) ChaN, 2019. All rights reserved.

## 4. Teensy Cores & Teensy Audio Library (PJRC)
* **Location**: `platforms/mk66/3rdparty/teensy_core/`, `platforms/mk66/3rdparty/Audio/`, `core/libs/Audio/`
* **License**: MIT / PJRC Permissive License
* **Copyright**: Copyright (c) 2014-2018 Paul Stoffregen, PJRC.COM, LLC.

## 5. Liberation Sans Fonts (UI Bitmap Font Source)
* **Location**: `core/libs/fonts/LiberationSans-*_L4.{h,cpp}` (4-bit `L4` antialiased glyph bitmaps)
* **License**: SIL Open Font License 1.1 (OFL-1.1)
* **Copyright**: Digitized data copyright (c) 2010 Google Corporation; Copyright (c) 2012 Red Hat, Inc.

## 6. Embedded Template Library (ETL)
* **Location**: `util/3rdparty/etl/`
* **License**: MIT License
* **Copyright**: Copyright (c) 2016 jwellbelove (John Wellbelove).

## 7. magic_enum
* **Location**: `util/3rdparty/magic_enum/`
* **License**: MIT License
* **Copyright**: Copyright (c) 2019 - 2022 Daniil Goncharov.

## 8. FastCRC
* **Location**: `platforms/mk66/3rdparty/FastCRC/`, `platforms/imx/3rdparty/FastCRC/`
* **License**: MIT / GPLv3 dual-licensed (used under MIT/BSD terms where applicable)
* **Copyright**: Copyright (c) 2014-2016 Frank Boesing.

## 9. Polyend Precompiled Synth & DSP Libraries (`libPolySynth.a`, `libPolyDSP.a`)
* **Location**: `synth/lib/`
* **License**: Polyend Precompiled Synthesizer & DSP Library License (`synth/lib/LICENSE-PROPRIETARY.txt`)
* **Copyright**: Copyright (c) 2023-2026 Polyend Sp. z o.o. All rights reserved.

---

# MPDJ additions

MPDJ (the `mpc-port` branch) adds the Akai MPC One+ platform layer in `platforms/mpc/` and small, marked changes (`MPDJ` / `PLATFORM_MPC`) to the shared code. MPDJ's own additions: Copyright (c) 2026 MPDJ contributors, released under the MIT License (Part 1 of `LICENSE`); the project as a whole remains under `LICENSE`, including Part 2.

MPDJ is not affiliated with Polyend, Akai Professional or inMusic Brands. "Polyend" and "Polyend Tracker" are trademarks of Polyend Sp. z o.o.; "MPC" and "Akai Professional" are trademarks of inMusic Brands, Inc.

Third-party components used by MPDJ:

## 10. stb_image
* **Location**: `platforms/mpc/source/3rdparty/stb_image.h`
* **License**: Public domain or MIT (dual-licensed, at the user's choice)
* **Copyright**: Sean Barrett and contributors

## 11. JACK (optional, not linked)
* **Use**: MPDJ can run as a JACK client (`platforms/mpc/source/audio/JackClient.*`). `libjack` is loaded at run time with `dlopen` only when that mode is enabled; MPDJ ships no JACK code and the normal build has no JACK dependency
* **License of libjack**: LGPL-2.1

Planned components are credited here as they are added (e.g. zepto8 for the PICO-8-style engine on the `p8sound` branch, Mutable Instruments, CHOMPI Club / Chase Bliss DSP, Signalsmith Stretch), each under its own license.
