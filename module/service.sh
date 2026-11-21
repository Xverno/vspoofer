#!/system/bin/sh
MODDIR=${0%/*}
until [ "$(getprop sys.boot_completed)" = "1" ]; do sleep 3; done
sleep 5
mkdir -p /data/adb/vspoofer
: > /data/adb/vspoofer/log.txt
nohup sh "$MODDIR/vspoof.sh" daemon >/dev/null 2>&1 &
