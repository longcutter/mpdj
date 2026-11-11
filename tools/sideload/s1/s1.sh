#!/bin/sh
# MPDJ sideload test S1: JACK + SuperCollider. Run inside the Debian chroot.
# Usage: sh /root/s1.sh [period frames, 128] [periods, 3] [seconds, 300] [device, hw:1,0]
P=${1:-128}; N=${2:-3}; SECS=${3:-300}; DEV=${4:-hw:1,0}
export JACK_NO_AUDIO_RESERVATION=1 QT_QPA_PLATFORM=offscreen S1_SECS=$SECS
export QTWEBENGINE_DISABLE_SANDBOX=1 QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox --disable-gpu"
export SC_JACK_DEFAULT_INPUTS="system:capture_1,system:capture_2"
export SC_JACK_DEFAULT_OUTPUTS="system:playback_1,system:playback_2"
pkill -x sclang; pkill -x scsynth; pkill -x jackd; sleep 1
cd /root
if [ "$DEV" = dummy ]; then D="-d dummy -r 44100 -p $P"; else D="-d alsa -d $DEV -r 44100 -p $P -n $N"; fi
echo "=== S1: jackd $D, $SECS s"
jackd -R -P 80 $D > jackd.log 2>&1 &
JP=$!
sleep 3
if ! kill -0 $JP 2>/dev/null; then echo "=== jackd did not start:"; cat jackd.log; exit 1; fi
jack_lsp
(T=0; while true; do sleep 30; T=$((T + 30)); echo "--- ${T} s, jackd xrun lines so far: $(grep -ci xrun jackd.log)"; top -b -n 1 | grep -E "^%Cpu|^MiB Mem|jackd|scsynth|sclang"; done) > top.log &
TP=$!
timeout $((SECS + 120)) sclang s1.scd 2>&1 | grep -E "^S1|ERROR|rror|JackDriver|could not|fail" | tee s1.log
kill $TP 2>/dev/null; kill $JP; sleep 1
echo "=== jackd xrun lines: $(grep -ci xrun jackd.log)"
grep -i xrun jackd.log | head -5
echo "=== every 30 s (xruns so far, top):"
cat top.log
