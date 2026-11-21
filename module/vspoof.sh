#!/system/bin/sh
# V Spoofer core - by @Xverno
MODDIR=${0%/*}
DATA=/data/adb/vspoofer
LOG=$DATA/log.txt
RP=$(command -v resetprop)
PARTS='"" system. vendor. odm. product. system_ext. bootimage.'

log() { echo "$(date '+%H:%M:%S') $*" >> "$LOG"; }

cfg() {
  MODE=perapp; PRESET=rog7; ENGINE=zygisk
  [ -f "$DATA/config" ] && . "$DATA/config"
}

get_preset() {
  line=$(grep -m1 "^$1|" "$DATA/custom.conf" 2>/dev/null)
  [ -z "$line" ] && line=$(grep -m1 "^$1|" "$MODDIR/presets.conf" 2>/dev/null)
  [ -z "$line" ] && return 1
  IFS='|' read -r P_ID P_LABEL P_BRAND P_MANU P_MODEL P_DEV P_NAME <<EOT
$line
EOT
  return 0
}

setp() {
  [ -n "$RP" ] || { log "resetprop not found"; return 1; }
  [ -n "$(getprop "$1")" ] && "$RP" -n "$1" "$2"
}

each_prop() { # field
  for part in "" system. vendor. odm. product. system_ext. bootimage.; do
    echo "ro.product.${part}$1"
  done
}

save_orig() {
  : > "$DATA/orig.conf"
  for f in manufacturer brand model device name; do
    for n in $(each_prop $f); do
      v=$(getprop "$n"); [ -n "$v" ] && echo "$n=$v" >> "$DATA/orig.conf"
    done
  done
  v=$(getprop ro.build.product); [ -n "$v" ] && echo "ro.build.product=$v" >> "$DATA/orig.conf"
  echo "inactive|" > "$DATA/state"
}

apply() {
  cfg
  get_preset "$PRESET" || { log "preset $PRESET not found"; return 1; }
  [ -f "$DATA/orig.conf" ] || save_orig
  for n in $(each_prop manufacturer); do setp "$n" "$P_MANU"; done
  for n in $(each_prop brand);        do setp "$n" "$P_BRAND"; done
  for n in $(each_prop model);        do setp "$n" "$P_MODEL"; done
  for n in $(each_prop device);       do setp "$n" "$P_DEV"; done
  for n in $(each_prop name);         do setp "$n" "$P_NAME"; done
  setp ro.build.product "$P_DEV"
  echo "active|$P_LABEL" > "$DATA/state"
  log "applied: $P_LABEL"
}

revert() {
  [ -f "$DATA/orig.conf" ] || return 0
  while IFS= read -r l; do
    [ -n "$l" ] && "$RP" -n "${l%%=*}" "${l#*=}"
  done < "$DATA/orig.conf"
  echo "inactive|" > "$DATA/state"
  log "reverted to stock"
}

cur_state() { cut -d'|' -f1 "$DATA/state" 2>/dev/null; }

game_running() {
  while IFS= read -r p; do
    [ -n "$p" ] && pidof "$p" >/dev/null 2>&1 && return 0
  done < "$DATA/games.list"
  return 1
}

is_game() { grep -qx "$1" "$DATA/games.list" 2>/dev/null; }

sync_mode() {
  cfg
  case "$MODE" in
    global) apply ;;
    perapp) if [ "$ENGINE" = script ] && game_running; then apply; else revert; fi ;;
    *) revert ;;
  esac
}

on_fg() {
  cfg
  [ "$MODE" = perapp ] && [ "$ENGINE" = script ] || return 0
  if is_game "$1"; then
    [ "$(cur_state)" = active ] || apply
  elif [ "$(cur_state)" = active ] && ! game_running; then
    revert
  fi
}

watch_events() {
  start=$(date +%s)
  logcat -b events -v brief 'wm_set_resumed_activity:I' '*:S' 2>/dev/null | while read -r line; do
    pkg=$(echo "$line" | sed -n 's/.*wm_set_resumed_activity[^[]*\[[0-9]*,\([^/]*\)\/.*/\1/p')
    [ -n "$pkg" ] && on_fg "$pkg"
  done
  [ $(( $(date +%s) - start )) -ge 15 ]
}

watch_poll() {
  last=""
  while :; do
    pkg=$(dumpsys activity activities 2>/dev/null | grep -m1 -E 'topResumedActivity|mResumedActivity' | sed -n 's/.* \([A-Za-z0-9._]*\)\/.*/\1/p')
    if [ -n "$pkg" ] && [ "$pkg" != "$last" ]; then last=$pkg; on_fg "$pkg"; fi
    sleep 3
  done
}

daemon() {
  save_orig
  sync_mode
  watch_events || { log "event watcher unavailable, using polling"; watch_poll; }
}

case "$1" in
  apply) apply ;;
  revert) revert ;;
  sync) sync_mode ;;
  daemon) daemon ;;
esac
