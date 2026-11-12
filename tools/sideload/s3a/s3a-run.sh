#!/bin/sh
# MPDJ sideload test S3, option A (JACK hub): runs jackd and MPDJ inside the
# Debian root, MPDJ as a JACK client (MPDJ_AUDIO=jack), and stops jackd after.
# Stop the MPC app first. Ctrl+C quits MPDJ as usual.
#
#   s3a-run.sh            MPDJ as a JACK client (pass MPDJ_* settings as usual)
#   s3a-run.sh latency    the round-trip probe through JACK (cable outs -> ins)
#
#   S3A_PERIOD (128), S3A_PERIODS (3): jackd's buffer, as in S1
#   S3A_ROOT: Debian root (default /media/az01-internal/mpdj/debian)
# Copies /media/az01-internal/mpdj/tracker-mpc into the root as /root/mpdj/tracker-mpc
# and bind-mounts the SD card at the same path inside, so MPDJ finds its files.
R=${S3A_ROOT:-/media/az01-internal/mpdj/debian}
D=/media/az01-internal/mpdj
CARD="/media/MPC SDC"

grep -q " $R/proc " /proc/mounts || for d in dev dev/pts dev/shm proc sys; do mount --bind /$d $R/$d; done
mkdir -p "$R$CARD" "$R/root/mpdj"
grep -q " $R/media/MPC\\\\040SDC " /proc/mounts || mount --bind "$CARD" "$R$CARD"
cp "$D/tracker-mpc" "$R/root/mpdj/tracker-mpc" || exit 1

inroot() { PATH=/usr/sbin:/usr/bin:/sbin:/bin JACK_NO_AUDIO_RESERVATION=1 chroot "$R" "$@"; }

if inroot jack_lsp >/dev/null 2>&1; then
    echo "s3a: jackd is already running in the root; using it"
    JACKPID=
else
    echo "s3a: starting jackd: -R -P 80 -d alsa -d hw:1,0 -r 44100 -p ${S3A_PERIOD:-128} -n ${S3A_PERIODS:-3} (log: /tmp/s3a-jackd.log)"
    # own session, so Ctrl+C reaches MPDJ only; we stop jackd ourselves afterwards
    setsid sh -c "PATH=/usr/sbin:/usr/bin:/sbin:/bin JACK_NO_AUDIO_RESERVATION=1 exec chroot '$R' jackd -R -P 80 -d alsa -d hw:1,0 -r 44100 -p ${S3A_PERIOD:-128} -n ${S3A_PERIODS:-3}" > /tmp/s3a-jackd.log 2>&1 &
    JACKPID=$!
    i=0
    until inroot jack_lsp >/dev/null 2>&1; do
        i=$((i + 1))
        if [ $i -gt 50 ]; then echo "s3a: jackd didn't start:"; cat /tmp/s3a-jackd.log; exit 1; fi
        sleep 0.1
    done
fi

trap '' INT # the script itself ignores Ctrl+C and waits for MPDJ
if [ "$1" = latency ]; then
    inroot /bin/sh -c 'cd /root/mpdj && MPDJ_AUDIO=jack MPDJ_LATENCY_TEST=1 exec ./tracker-mpc'
else
    inroot /bin/sh -c 'cd /root/mpdj && MPDJ_AUDIO=${MPDJ_AUDIO:-jack} exec ./tracker-mpc'
fi
RC=$?

if [ -n "$JACKPID" ]; then
    echo "s3a: stopping jackd; its xrun lines:"
    kill "$JACKPID" 2>/dev/null
    pid=$(pidof jackd); [ -n "$pid" ] && kill $pid 2>/dev/null
    sleep 0.5
    grep -ci xrun /tmp/s3a-jackd.log | sed 's/^/s3a: jackd xrun lines: /'
fi
exit $RC
