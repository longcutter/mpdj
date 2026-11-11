#!/bin/sh
# MPDJ sideload test S2: what MPDJ runs as the sideload (MPDJ_SIDELOAD).
# Starts the test card inside the Debian root, passing on the MIDI fds MPDJ set up.
# Usage: MPDJ_SIDELOAD=/media/az01-internal/mpdj/s2-launch.sh ./tracker-mpc
#   TESTCARD_SECONDS=N   the card exits by itself after N s (for MPDJ_SIDELOAD_CYCLE)
#   S2_ROOT=<dir>        Debian root (default /media/az01-internal/mpdj/debian);
#                        S2_ROOT=none runs the card without the chroot
R=${S2_ROOT:-/media/az01-internal/mpdj/debian}
COUNT=/tmp/mpdj-s2-run
RUN=$(( $(cat $COUNT 2>/dev/null || echo 0) + 1 ))
echo $RUN > $COUNT
if [ "$R" = none ]; then
    exec env TESTCARD_RUN=$RUN TESTCARD_SECONDS=${TESTCARD_SECONDS:-0} "$(dirname "$0")/testcard"
fi
grep -q " $R/proc " /proc/mounts || for d in dev dev/pts dev/shm proc sys; do mount --bind /$d $R/$d; done
exec chroot "$R" /usr/bin/env -i HOME=/root PATH=/usr/sbin:/usr/bin:/sbin:/bin \
    MPDJ_MIDI_IN_FD=${MPDJ_MIDI_IN_FD:-3} MPDJ_MIDI_OUT_FD=${MPDJ_MIDI_OUT_FD:-4} \
    TESTCARD_RUN=$RUN TESTCARD_SECONDS=${TESTCARD_SECONDS:-0} TESTCARD_TONE=${TESTCARD_TONE:-1} \
    /root/s2/testcard
