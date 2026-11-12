# MPDJ

**MPDJ** (MPDJ Tracker) runs a tracker natively on the **Akai MPC One+**, in place of the MPC app. It is a port of [Polyend Tracker Community Edition](https://github.com/polyend/tracker-ce) to the MPC's Linux system: the tracker's sequencer, sample engine and screens, driven by the MPC's pads, Q-Link knobs, buttons, touchscreen and audio interface. The name is a nod to LSDJ.

> **Unofficial and free.** MPDJ is a hobby project. It is not made, endorsed or supported by Polyend, Akai Professional or inMusic. "Polyend" and "Polyend Tracker" are Polyend's trademarks; "MPC" and "Akai Professional" belong to inMusic Brands. MPDJ is free and will never be sold (see [License](#license)).

## Status

Early and in active development. On an MPC One+ it currently:

- boots the tracker full screen (1280x800, the Tracker's UI scaled up), on the MPC's own sound card at 44.1 kHz with ~7 ms round trip
- plays and edits patterns and songs, loads and plays samples from the SD card (including long streamed ones), takes audio from the MPC's inputs
- maps the MPC's controls: pads, transport and mode buttons, the four Q-Links (navigation and value modes, per-screen knobs on Sample Playback and the Sample Editor), the main encoder and the touchscreen, with Q-Link LEDs following the mode

Planned: MPC-style pads mapped to tracks, open-source synth engines, insert effects, recording while the pattern plays, external resampling through pedals, USB audio to a computer, and "sideloads" (separate programs such as norns started from MPDJ). MPDJ still uses the Tracker's project format, so Tracker and Tracker+ projects should open as they are (not widely tested yet); as MPDJ's own format grows it will keep a one-way import.

Polyend's factory synth engines are closed binaries for the Tracker's chip and are not part of MPDJ, so tracks using them stay silent; sample and MIDI tracks work.

## Requirements

- An Akai MPC One+ on MPC firmware 2.14 with SSH access (a community SSH mod). Other MPC models are untested
- A computer to build on (macOS or Linux) and to copy files over SSH
- An SD card in the MPC for samples and projects

Running MPDJ stops the MPC app while it runs and doesn't change the MPC's system partition; restarting the MPC app (or the MPC) brings everything back. Still, modifying your MPC is at your own risk and may affect its warranty.

## Build

MPDJ cross-compiles for 32-bit ARM Linux with [zig](https://ziglang.org/) as the C/C++ toolchain:

```
pip3 install ziglang
make -C platforms/mpc -j4 strip
```

The binary is `platforms/mpc/build/tracker-mpc`.

## Install and run

Assuming the MPC answers to `ssh mpc`:

```
ssh mpc 'mkdir -p /media/az01-internal/mpdj "/media/MPC SDC/MPDJ"'
scp platforms/mpc/build/tracker-mpc mpc:/media/az01-internal/mpdj/tracker-mpc
ssh mpc 'chmod +x /media/az01-internal/mpdj/tracker-mpc && systemctl stop inmusic-mpc'
ssh -t mpc 'cd /media/az01-internal/mpdj && ./tracker-mpc'
```

Ctrl+C quits. Start the MPC app again with `ssh mpc 'systemctl start inmusic-mpc'`. MPDJ's files (samples, projects) live in `MPDJ` on the SD card.

## Repository

- `mpc-port` (default branch): MPDJ. The MPC platform layer is in `platforms/mpc/`; changes to the shared tracker code are kept small and marked `MPDJ` or `PLATFORM_MPC`
- `main`: Polyend's Tracker CE as forked; upstream changes are reviewed and merged into `mpc-port` by hand
- Polyend's original README: [`docs/README-tracker-ce.md`](docs/README-tracker-ce.md); the Tracker CE developer docs are in `docs/`

## License

MPDJ is a derivative of Polyend Tracker Community Edition and follows its [`LICENSE`](LICENSE): MIT, with a restriction on selling the complete Tracker application or a port of it. That's why MPDJ is free and will stay free. Making and selling music with it is fine.

- Individual parts (drivers, DSP, the MPC platform layer) can be reused in other projects under the MIT terms, as the license allows
- MPDJ's own additions are released under the MIT License as well
- Third-party components and credits: [`NOTICE.md`](NOTICE.md)
- Separate programs that MPDJ can start ("sideloads") keep their own licenses and are not part of MPDJ's code
