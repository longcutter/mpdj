# Contributing to MPDJ

Thanks for trying MPDJ. Please read this first, because it sets expectations.

## What to expect

MPDJ is a one-person hobby project built in spare time. There's no support, no schedule and no promise of a reply or a merge. Bug reports are read and appreciated; they may not be fixed quickly, or at all. If you want MPDJ to go a different way, forking it is welcome (see [License](README.md#license)).

## Where things go

| You have | Go to |
| --- | --- |
| A bug you can reproduce | [Issues](../../issues/new/choose), with the bug template |
| A question, an idea or a feature request | [Discussions](../../discussions) |
| Something you made with MPDJ | [Discussions](../../discussions) (Show and tell) |
| A fix to the shared Tracker code that isn't MPC-specific | Upstream: [polyend/tracker-ce](https://github.com/polyend/tracker-ce) (MPDJ merges upstream changes by review) |

Issues without the template's details (MPDJ version, steps, log) may be closed without a reply. Feature requests opened as issues get moved to Discussions.

## Pull requests

- **Small fixes are welcome** (a crash, a wrong label, a typo in the docs). Open the PR against `mpc-port`.
- **For anything bigger, start a Discussion first.** MPDJ follows a control map and a set of design decisions that aren't all written down in the repo yet, so an unannounced large PR will probably not be merged as it is.
- One change per PR. Say what it fixes, how you tested it, and on which MPC and firmware.
- MPDJ only builds for the MPC: `make -C platforms/mpc -j4 strip` must build cleanly. Test on hardware if you can; say so if you couldn't.

## Code notes

- The MPC platform layer lives in `platforms/mpc/`. MPDJ features prefer new files (`core/source/mpdj*.cpp`, `core/interface/...`) over edits to Polyend's code; edits to shared Tracker code are kept small and marked `MPDJ` or `PLATFORM_MPC`, so upstream merges stay easy.
- Many core files use CRLF line endings; keep them as they are.
- The audio thread must not allocate, block or touch files.
- Project, pad and config data are saved to the card: never reorder or resize existing fields. MPDJ's project data has its own versioned block (`strMpdjProjectData`); bump its version for a new field and give older files a default.

## License

By opening a pull request you agree that your contribution is released under the MIT License, as MPDJ's own additions are (see [`NOTICE.md`](NOTICE.md), "MPDJ additions"); the project as a whole stays under [`LICENSE`](LICENSE). If you add third-party code, it must be under a permissive license (MIT, BSD, Apache, public domain) and credited in `NOTICE.md`.
