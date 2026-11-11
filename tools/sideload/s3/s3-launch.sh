#!/bin/sh
# MPDJ sideload test S3: what MPDJ runs as the engine (MPDJ_ENGINE).
# Starts the tone engine inside the Debian root, so the ring has to cross the
# chroot through /dev/shm (bind-mounted), as a real engine's would.
# Usage: MPDJ_ENGINE=/media/az01-internal/mpdj/s3-launch.sh ./tracker-mpc
#   TONE_* settings pass through (see s3tone.c)
#   S3_ROOT=<dir>   Debian root (default /media/az01-internal/mpdj/debian);
#                   S3_ROOT=none runs the engine without the chroot
R=${S3_ROOT:-/media/az01-internal/mpdj/debian}
if [ "$R" = none ]; then
    exec "$(dirname "$0")/s3tone"
fi
grep -q " $R/proc " /proc/mounts || for d in dev dev/pts dev/shm proc sys; do mount --bind /$d $R/$d; done
exec chroot "$R" /usr/bin/env -i HOME=/root PATH=/usr/sbin:/usr/bin:/sbin:/bin \
    MPDJ_RING="$MPDJ_RING" TONE_HZ=${TONE_HZ:-440} TONE_DB=${TONE_DB:--18} TONE_BPM=${TONE_BPM:-120} \
    TONE_VOICES=${TONE_VOICES:-0} TONE_PRIO=${TONE_PRIO:-60} TONE_SECONDS=${TONE_SECONDS:-0} \
    /root/s3/s3tone
