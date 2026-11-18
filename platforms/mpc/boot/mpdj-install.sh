#!/bin/sh
# MPDJ startup service for the MPC One+ (firmware 2.14), 2026-11-17.
#
#   sh mpdj-install.sh install     start MPDJ instead of the MPC app at power-on
#   sh mpdj-install.sh uninstall   back to the stock start-up (removes everything it added)
#   sh mpdj-install.sh status      what's installed, the boot mode, the fail count
#
# How it works (v2, 2026-11-17): /etc is an overlay mounted early in boot, after systemd has
# read its unit files, so units added there are only seen when systemd loads a unit for
# the first time later on. The SD card (/media/MPC SDC) is mounted by the MPC app after it
# starts, which makes systemd create media-MPC\x20SDC.mount then: a drop-in there
# (Wants=mpdj.service) starts MPDJ. mpdj.service, once per boot (marker in /run): unless
# <mpdj>/boot-mode is "mpc" or 3 starts in a row failed (<mpdj>/boot-fails; MPDJ sets it
# back to 0 after 30 s up), it stops the MPC app and runs tracker-mpc (holding the power
# button like the MPC app does); if MPDJ crashes, the MPC app starts again. The MPC app's
# own service isn't touched. Nothing on the read-only system partition changes.

D=/media/az01-internal/mpdj
UNIT=/etc/systemd/system/mpdj.service
DROPIN_DIR='/etc/systemd/system/media-MPC\x20SDC.mount.d'
DROPIN=$DROPIN_DIR/mpdj.conf
OLD_DROPIN_DIR=/etc/systemd/system/inmusic-mpc.service.d # v1, removed on install

install_it() {
    [ -x "$D/tracker-mpc" ] || { echo "no $D/tracker-mpc: copy MPDJ there first"; exit 1; }
    cat > "$D/mpdj-boot.sh" <<'EOF'
#!/bin/sh
# MPDJ start-up (written by mpdj-install.sh), run by mpdj.service when the SD card mounts
D=/media/az01-internal/mpdj
[ -e /run/mpdj-decided ] && exit 0 # once per boot (a card re-inserted later doesn't count)
touch /run/mpdj-decided
[ "$(cat "$D/boot-mode" 2>/dev/null)" = mpc ] && exit 0
fails=$(cat "$D/boot-fails" 2>/dev/null)
fails=${fails:-0}
if [ "$fails" -ge 3 ]; then
    echo 0 > "$D/boot-fails"
    echo "mpdj: 3 failed starts in a row, leaving the MPC app running"
    exit 0
fi
[ -x "$D/tracker-mpc" ] || exit 0
cd "$D" || exit 0
echo "mpdj: stopping the MPC app"
systemctl stop inmusic-mpc
echo $((fails + 1)) > "$D/boot-fails"
if type systemd-inhibit > /dev/null 2>&1; then
    systemd-inhibit --what=handle-power-key --who=MPDJ --why="MPDJ handles the power button" ./tracker-mpc
else
    ./tracker-mpc
fi
rc=$?
if [ $rc -ne 0 ]; then
    echo "mpdj: tracker-mpc ended with $rc, starting the MPC app"
    systemctl --no-block start inmusic-mpc
fi
exit 0
EOF
    chmod 755 "$D/mpdj-boot.sh"
    cat > "$UNIT" <<EOF
[Unit]
Description=MPDJ (takes over from the MPC app once the SD card is mounted; see $D/mpdj-install.sh)

[Service]
ExecStart=$D/mpdj-boot.sh
WorkingDirectory=$D
EOF
    rm -rf "$OLD_DROPIN_DIR"
    mkdir -p "$DROPIN_DIR"
    cat > "$DROPIN" <<EOF
# MPDJ: start MPDJ once the SD card is mounted (mpdj-boot.sh decides, once per boot)
[Unit]
Wants=mpdj.service
EOF
    [ -e "$D/boot-mode" ] || echo mpdj > "$D/boot-mode"
    echo 0 > "$D/boot-fails"
    systemctl daemon-reload
    sync
    echo "installed: MPDJ takes over at power-on once the SD card is mounted (boot-mode: $(cat "$D/boot-mode"))"
}

uninstall_it() {
    rm -f "$DROPIN" "$UNIT"
    rmdir "$DROPIN_DIR" 2>/dev/null
    rm -rf "$OLD_DROPIN_DIR"
    rm -f "$D/mpdj-boot.sh" "$D/boot-fails"
    systemctl daemon-reload
    sync
    echo "uninstalled: the MPC app starts at power-on as before"
}

case "$1" in
install) install_it ;;
uninstall) uninstall_it ;;
status)
    for f in "$UNIT" "$DROPIN" "$D/mpdj-boot.sh"; do
        [ -e "$f" ] && echo "present: $f" || echo "missing: $f"
    done
    echo "boot-mode: $(cat "$D/boot-mode" 2>/dev/null || echo '(none: mpdj)')"
    echo "boot-fails: $(cat "$D/boot-fails" 2>/dev/null || echo '(none)')"
    systemctl is-active mpdj.service inmusic-mpc.service
    ;;
*) echo "usage: sh $0 install | uninstall | status" ;;
esac
