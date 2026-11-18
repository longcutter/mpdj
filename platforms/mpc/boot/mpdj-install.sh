#!/bin/sh
# MPDJ startup service for the MPC One+ (firmware 2.14), 2026-11-17.
#
#   sh mpdj-install.sh install     start MPDJ instead of the MPC app at power-on
#   sh mpdj-install.sh uninstall   back to the stock start-up (removes everything it added)
#   sh mpdj-install.sh status      what's installed, the boot mode, the fail count
#
# How it works: a drop-in for inmusic-mpc.service adds an ExecCondition that runs once per
# boot (marker in /run). If <mpdj>/boot-mode isn't "mpc", fewer than 3 MPDJ starts in a
# row have failed and tracker-mpc is there, it starts mpdj.service and skips the MPC app.
# Any later "systemctl start inmusic-mpc" (System menu > Switch to the MPC app, SSH) runs
# the MPC app as usual. mpdj.service waits up to 30 s for the SD card, counts the start in
# <mpdj>/boot-fails (MPDJ sets it back to 0 after 30 s up) and starts the MPC app if MPDJ
# crashes or the card never shows up. Nothing on the read-only system partition changes:
# the two unit files go in /etc/systemd/system (an overlay on the internal drive).

D=/media/az01-internal/mpdj
UNIT=/etc/systemd/system/mpdj.service
DROPIN_DIR=/etc/systemd/system/inmusic-mpc.service.d
DROPIN=$DROPIN_DIR/mpdj.conf

install_it() {
    [ -x "$D/tracker-mpc" ] || { echo "no $D/tracker-mpc: copy MPDJ there first"; exit 1; }
    cat > "$D/mpdj-boot.sh" <<'EOF'
#!/bin/sh
# MPDJ start-up (written by mpdj-install.sh). check: inmusic-mpc's ExecCondition (exit 0 =
# start the MPC app, 1 = skip it); run: mpdj.service
D=/media/az01-internal/mpdj
case "$1" in
check)
    [ -e /run/mpdj-decided ] && exit 0 # only the first start after boot is decided here
    touch /run/mpdj-decided
    [ "$(cat "$D/boot-mode" 2>/dev/null)" = mpc ] && exit 0
    fails=$(cat "$D/boot-fails" 2>/dev/null)
    fails=${fails:-0}
    if [ "$fails" -ge 3 ]; then
        echo 0 > "$D/boot-fails"
        echo "mpdj: 3 failed starts in a row, starting the MPC app instead"
        exit 0
    fi
    [ -x "$D/tracker-mpc" ] || exit 0
    systemctl --no-block start mpdj.service
    exit 1
    ;;
run)
    cd "$D" || exit 1
    i=0
    while ! grep -q 'MPC\\040SDC' /proc/mounts && [ $i -lt 30 ]; do
        sleep 1
        i=$((i + 1))
    done
    if ! grep -q 'MPC\\040SDC' /proc/mounts; then
        echo "mpdj: no SD card after 30 s, starting the MPC app"
        systemctl --no-block start inmusic-mpc
        exit 0
    fi
    fails=$(cat "$D/boot-fails" 2>/dev/null)
    echo $((${fails:-0} + 1)) > "$D/boot-fails"
    ./tracker-mpc
    rc=$?
    if [ $rc -ne 0 ]; then
        echo "mpdj: tracker-mpc ended with $rc, starting the MPC app"
        systemctl --no-block start inmusic-mpc
    fi
    exit 0
    ;;
esac
EOF
    chmod 755 "$D/mpdj-boot.sh"
    cat > "$UNIT" <<EOF
[Unit]
Description=MPDJ (started instead of the MPC app; see $D/mpdj-install.sh)
After=az01-network-midi.service az01-script-runner.service az01-usbsata-fixer.service touch-fw-update.service xmos-update.service

[Service]
ExecStart=$D/mpdj-boot.sh run
WorkingDirectory=$D
EOF
    mkdir -p "$DROPIN_DIR"
    cat > "$DROPIN" <<EOF
# MPDJ: decides once per boot whether MPDJ starts instead of the MPC app
[Service]
ExecCondition=$D/mpdj-boot.sh check
EOF
    [ -e "$D/boot-mode" ] || echo mpdj > "$D/boot-mode"
    echo 0 > "$D/boot-fails"
    systemctl daemon-reload
    sync
    echo "installed: MPDJ starts at power-on (boot-mode: $(cat "$D/boot-mode"))"
}

uninstall_it() {
    rm -f "$DROPIN" "$UNIT"
    rmdir "$DROPIN_DIR" 2>/dev/null
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
