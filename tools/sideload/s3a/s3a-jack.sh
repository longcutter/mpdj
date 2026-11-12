#!/bin/sh
# Runs a JACK tool inside the Debian root, next to the jackd s3a-run.sh started.
#   s3a-jack.sh jack_lsp -c
#   s3a-jack.sh jack_metro -b 120 -f 880 -A 0.3          (a second JACK client)
#   s3a-jack.sh jack_connect metro:120_bpm mpdj:aux_1     (route it into MPDJ)
R=${S3A_ROOT:-/media/az01-internal/mpdj/debian}
PATH=/usr/sbin:/usr/bin:/sbin:/bin JACK_NO_AUDIO_RESERVATION=1 exec chroot "$R" "$@"
