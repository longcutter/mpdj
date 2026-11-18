# Polyend Tracker Community Edition (CE) - Documentation & Wiki

Welcome to the official documentation for **Polyend Tracker Community Edition (CE)** (`1.0.0`), covering the **Original Polyend Tracker** (`MK66`), **Polyend Tracker+** and **Polyend Tracker Mini** (`IMX`).

* **[Release Changelog (`CHANGELOG.md`)](CHANGELOG-tracker-ce.md)** - Full list of new features, improvements, and fixes in each release.

---

## Musician & User Guides

* **[Custom Themes & Fonts (`docs/custom_themes_and_fonts.md`)](custom_themes_and_fonts.md)**
  * How to install and create custom JSON color themes (`/Themes/*.json`) and custom antialiased bitmap fonts (`/Fonts/*.ptfont`) using the [Web Theme & Font Editor](https://polyend.github.io/tracker-theme-editor/) ([`polyend/tracker-theme-editor`](https://github.com/polyend/tracker-theme-editor)), plus full color key reference tables.
* **[Wavetable Envelopes, LFOs & Step FX (`docs/wavetable_envelopes.md`)](wavetable_envelopes.md)**
  * Complete guide to the 256-shape `WT Env` and `WT LFO` instrument modulators, dual-axis `Warp X/Y` shaping, `w` / `y` pattern step FX, and loading custom `.wav` contours from `/Envelopes/`.
* **[Modular Sample Editor Effects - User & Architecture Guide (`docs/modular_effects_system.md`)](modular_effects_system.md)**
  * Overview of hot-loadable `.ptfx` Sample Editor effects and generators (`/Effects/*.ptfx`), bundled reference plugins, and hardware memory/cache architecture.
* **[SD Card Image Viewer (`docs/image_viewer.md`)](image_viewer.md)**
  * How to prepare and view up to 48 baseline JPEGs (`800 × 390` px) per folder in `/Images/Reference/`, `/Images/Notes/`, and `/Images/Custom/` under `Config > Info`.

---

## Developer & SDK Documentation

* **[Firmware Architecture, Hardware Limits & Developer Gotchas (`docs/developer_guide.md`)](developer_guide.md)**
  * Hardware memory budgets (`FLASH`, `SRAM_DTC`, `SRAM_ITC`, `EXTERNAL_RAM`, plus theoretically reclaimable `SAMPLE_MEMORY_SIZE` headroom), the shared `768 KB` scratch buffer layout (`externalRamScratchBuff`), the Copy-on-Write `/Workspace/instruments/` fast project loader, Hybrid SD Card Sample Streaming (`VoiceStreamManager`), FT812 display/bootloader constraints, and platform preprocessor guards (`PLATFORM_IMX`, `TRACKER_MINI`, `GUI_HANDLE_SYNTH`).
* **[Custom Synth Engine SDK (`docs/custom_synth_sdk.md`)](custom_synth_sdk.md)**
  * Step-by-step C++20 developer tutorial and API reference (`synth/include/PolySynth/sdk/`) for building and registering new polyphonic synthesizer engines on **Tracker+** and **Tracker Mini**, including a walkthrough of the open-source **`CHIP`** (`ChiptuneSynthVoice`) engine.
* **[Modular Sample Editor Effects & `.ptfx` Developer SDK (`docs/modular_effects_system.md`)](modular_effects_system.md)**
  * Complete C/C++ developer guide (`editor-fx-sdk/`), parameter/host API reference, 472-byte `PtfxHeader` binary spec, and firmware cache/buffer architecture for `.ptfx` Sample Editor effects and generators across all 3 Tracker devices.
* **[Repository README - Build Instructions & PR Guidelines (`README.md`)](../README.md)**
  * Top-level build instructions, flashing steps, project philosophy, and contribution rules.

---

## Polyend Tracker Open-Source Ecosystem

* **[`polyend/tracker-ce`](https://github.com/polyend/tracker-ce)** - Polyend Tracker Community Edition firmware, Open Synth SDK, and `.ptfx` Sample Editor Effects SDK.
* **[`polyend/tracker-theme-editor`](https://github.com/polyend/tracker-theme-editor)** ([Live Web App](https://polyend.github.io/tracker-theme-editor/)) - Visual browser-based Theme Editor (`/Themes/*.json`) and Font Editor (TTF/OTF to `/Fonts/*.ptfont`).
* **[`polyend/tracker-lib`](https://github.com/polyend/tracker-lib)** - Open-source library for reading, writing, and inspecting Polyend Tracker project (`.mt`), pattern (`.mtp`), and instrument (`.pti`) files.
* **[Polyend Backstage Forum](https://backstage.polyend.com/)** - Community discussions, custom themes, fonts, and plugins.
* **[The Tracker Overflow (Discord)](https://discord.com/invite/uEPTnwckJV)** - Unofficial Polyend Tracker Discord where users and developers can join and find like-minded people.