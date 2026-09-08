#!/bin/sh
set +e

HERE=$(cd "$(dirname "$0")" && pwd)
if [ -z "${ADB:-}" ]; then
  if command -v adb >/dev/null 2>&1; then
    ADB=adb
  else
    case "$(uname -s)" in
      Linux) ADB="$HERE/bin/linux/adb" ;;
      Darwin) ADB="$HERE/bin/mac/adb" ;;
      *) ADB=adb ;;
    esac
    [ -f "$ADB" ] && chmod 755 "$ADB" 2>/dev/null
  fi
fi
if ! command -v "$ADB" >/dev/null 2>&1; then
  echo "adb not found: install platform-tools, set ADB=/path/to/adb, or provide bin/<os>/adb"
  exit 1
fi
REMOTE=/data/local/tmp/gl
LOG=$REMOTE/run.log

TARBALL=
if [ -n "$1" ] && [ -f "$1" ]; then
  TARBALL=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
  shift
fi
TRIES=${1:-15}
SETTLE=${SETTLE:-25}
POSTWAIT=${POSTWAIT:-30}

REMOTE_TAR=
REMOTE_SCRIPT=
EXEC_ARG=
[ -n "$TARBALL" ] && REMOTE_TAR="$REMOTE/$(basename "$TARBALL")"

if [ -x "$HERE/ghostlock_root" ]; then
  BIN="$HERE/ghostlock_root"
elif [ -x "$HERE/build/ghostlock_root" ]; then
  BIN="$HERE/build/ghostlock_root"
else
  exit 1
fi

if ! command -v seq >/dev/null 2>&1; then
  seq() { n=1; while [ "$n" -le "$1" ]; do echo "$n"; n=$((n + 1)); done; }
fi

TAIL=
stop_tail() { [ -n "$TAIL" ] && kill "$TAIL" 2>/dev/null; TAIL=; }
trap 'stop_tail' EXIT
trap 'stop_tail; exit 130' INT TERM

adb_state() { "$ADB" get-state 2>/dev/null | tr -d '\r'; }

disable_wifi() {
  case "$("$ADB" get-serialno 2>/dev/null | tr -d '\r')" in
    *:[0-9]*|*._tcp) : ;;
    *) "$ADB" shell svc wifi disable >/dev/null 2>&1 || true ;;
  esac
}

wait_boot() {
  "$ADB" wait-for-device 2>/dev/null
  n=0
  while [ "$n" -lt 120 ]; do
    [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && break
    n=$((n + 1))
    sleep 2
  done
  sleep "$SETTLE"
}

stage() {
  if [ "$(adb_state)" != device ]; then echo "  [stage] device not online"; return 1; fi
  "$ADB" shell "rm -rf $REMOTE; mkdir -p $REMOTE" >/dev/null 2>&1 || { echo "  [stage] mkdir failed"; return 1; }
  "$ADB" push "$BIN" "$REMOTE/ghostlock_root" >/dev/null 2>&1 || { echo "  [stage] push binary failed"; return 1; }
  "$ADB" shell "chmod 755 $REMOTE/ghostlock_root" >/dev/null 2>&1 || { echo "  [stage] chmod failed"; return 1; }
  if [ -n "$TARBALL" ]; then
    base=$(basename "$TARBALL")
    "$ADB" push "$TARBALL" "$REMOTE_TAR" >/dev/null 2>&1 || { echo "  [stage] push $base failed"; return 1; }
    "$ADB" shell "cd $REMOTE && { tar xf '$base' || gzip -dc '$base' | tar xf - || gunzip -c '$base' | tar xf -; } 2>/dev/null; true" >/dev/null 2>&1
    REMOTE_SCRIPT=$("$ADB" shell "find $REMOTE -type f -name '*.sh' | LC_ALL=C sort | head -1" 2>/dev/null | tr -d '\r')
    if [ -z "$REMOTE_SCRIPT" ]; then
      echo "  [stage] extract produced no .sh inside $base; device tar said:"
      "$ADB" shell "cd $REMOTE && tar xf '$base'" 2>&1 | sed 's/^/    /'
      return 1
    fi
    "$ADB" shell "chmod 755 '$REMOTE_SCRIPT'" >/dev/null 2>&1
    EXEC_ARG="--exec $REMOTE_SCRIPT"
  fi
  return 0
}

for i in $(seq "$TRIES"); do
  echo "Attempt $i/$TRIES"
  if [ "$(adb_state)" = device ]; then
    "$ADB" reboot >/dev/null 2>&1 || true
  fi
  wait_boot
  disable_wifi
  stage || continue

  "$ADB" shell "cd $REMOTE && (nohup ./ghostlock_root $EXEC_ARG >run.log 2>&1 &); sleep 1" >/dev/null 2>&1
  "$ADB" shell "sleep 1; tail -f -n +1 $LOG" 2>/dev/null &
  TAIL=$!

  result=timeout
  rooted_seen=0
  postn=0
  downs=0
  n=0
  MAXN=120
  [ -n "$TARBALL" ] && MAXN=1800
  while [ "$n" -lt "$MAXN" ]; do
    n=$((n + 1))
    sleep 1
    if [ -n "$TARBALL" ] && "$ADB" shell "grep -aq 'exited status=' $LOG" 2>/dev/null; then
      result=execdone
      break
    fi
    if [ "$rooted_seen" = 0 ] && "$ADB" shell "grep -aq 'ROOT] uid=0 daemon' $LOG" 2>/dev/null; then
      rooted_seen=1
      result=rooted
    fi
    if [ "$rooted_seen" = 1 ] && [ -z "$TARBALL" ]; then
      postn=$((postn + 1))
      if [ "$postn" -ge "$POSTWAIT" ] || "$ADB" shell "grep -aq 'ota] done' $LOG" 2>/dev/null; then
        break
      fi
    fi
    if [ "$rooted_seen" = 0 ] && "$ADB" shell "grep -aqE 'preloaded read slot failed|holding reclaim' $LOG" 2>/dev/null; then
      result=failed
      break
    fi
    if [ "$(adb_state)" != device ]; then
      downs=$((downs + 1))
      [ "$downs" -ge 3 ] && { result=down; break; }
    else
      downs=0
    fi
  done
  stop_tail

  if [ "$result" = execdone ]; then
    status=$("$ADB" shell "grep -a 'exited status=' $LOG | tail -1 | sed 's/.*status=//' | tr -dc '0-9-'" 2>/dev/null)
    exit "${status:-0}"
  fi

  if [ "$result" = rooted ]; then
    if [ -n "$TARBALL" ]; then
      echo "Rooted, but $(basename "$REMOTE_SCRIPT") did not report completion within the wait window; see $LOG."
      exit 1
    fi
    exec "$ADB" shell -t "su || $REMOTE/su || $REMOTE/ghostlock_root --su"
  fi
done

exit 1
