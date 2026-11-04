#!/bin/sh
EXPLOIT_NAME=preload
EXPLOIT_BUILD=build/${PROJECT:-sunstone}/bin/preload
REMOTE_DIR=/data/local/tmp
LOG=$REMOTE_DIR/ghostlock.log
CLEAN="$REMOTE_DIR/ghostlock.proof $LOG"
CAN_EXEC=0
IS_ROOTED="grep -q ROOTED $REMOTE_DIR/ghostlock.proof"
IS_READY="grep -aqE 'Su daemon ready|Su install failed' $LOG"
HAS_FAILED="grep -aqE 'Slide kaslr leak failed|abandoning boot|no ionroot near newest|write did not land' $LOG"
ROOT_SHELL="if command -v su >/dev/null 2>&1; then exec su; fi; echo 'su is unavailable, using the bind shell on 127.0.0.1:9999'; stty raw -echo 2>/dev/null; exec nc 127.0.0.1 9999"
DEFAULT_TRIES=8
DEFAULT_SETTLE=3
ROOT_TIMEOUT=180

HERE=$(cd "$(dirname "$0")" && pwd)
POSTWAIT=${POSTWAIT:-30}
SETTLE=${SETTLE:-$DEFAULT_SETTLE}
EXEC_TIMEOUT=${EXEC_TIMEOUT:-1800}

usage() {
  if [ "$CAN_EXEC" = 1 ]; then
    echo "Usage: $0 [SCRIPT.tar] [TRIES]"
  else
    echo "Usage: $0 [TRIES]"
  fi
}

case "${1:-}" in
  -h|--help) usage; exit 0 ;;
esac

if [ -z "${ADB:-}" ]; then
  if command -v adb >/dev/null 2>&1; then
    ADB=adb
  else
    case "$(uname -s)/$(uname -m)" in
      Linux/x86_64|Linux/amd64) ADB="$HERE/bin/linux/adb" ;;
      Darwin/*) ADB="$HERE/bin/mac/adb" ;;
      *) ADB=adb ;;
    esac
    [ -f "$ADB" ] && chmod 755 "$ADB" 2>/dev/null
  fi
fi
if ! "$ADB" version >/dev/null 2>&1; then
  echo "adb not found or not runnable: install platform-tools (pkg install android-tools on Termux),"
  echo "set ADB=/path/to/adb, or keep bin/ next to this script"
  exit 1
fi

BIN=${EXPLOIT:-}
if [ -z "$BIN" ]; then
  for f in "$HERE/bin/$EXPLOIT_NAME" "$HERE/$EXPLOIT_BUILD"; do
    if [ -f "$f" ]; then
      BIN=$f
      break
    fi
  done
fi
if [ -z "$BIN" ] || [ ! -f "$BIN" ]; then
  echo "${BIN:-$EXPLOIT_NAME} not found: build it first, or set EXPLOIT=/path/to/$EXPLOIT_NAME"
  exit 1
fi

TARBALL=
if [ -n "${1:-}" ] && [ -f "$1" ]; then
  if [ "$CAN_EXEC" != 1 ]; then
    echo "$1: running a script after root is not supported on this branch"
    exit 1
  fi
  TARBALL=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
  shift
fi
TRIES=${1:-$DEFAULT_TRIES}
case "$TRIES" in
  ''|*[!0-9]*|0)
    usage
    exit 1
    ;;
esac

NL='
'
if [ -z "${ANDROID_SERIAL:-}" ]; then
  online=$("$ADB" devices 2>/dev/null | tr -d '\r' | sed -n 's/^\([^[:space:]]*\)[[:space:]][[:space:]]*device$/\1/p')
  case "$online" in
    *"$NL"*)
      echo "More than one device is connected; set ANDROID_SERIAL to the one to root"
      exit 1
      ;;
    ?*) ANDROID_SERIAL=$online ;;
  esac
fi
if [ -n "${ANDROID_SERIAL:-}" ]; then
  export ANDROID_SERIAL
  echo "Using device $ANDROID_SERIAL"
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

TAIL=
stop_tail() {
  if [ -n "$TAIL" ]; then
    kill "$TAIL" 2>/dev/null || true
  fi
  TAIL=
}
trap stop_tail EXIT
trap 'stop_tail; exit 130' INT TERM

adb_state() {
  "$ADB" get-state 2>/dev/null | tr -d '\r'
}

disable_wifi() {
  case "$("$ADB" get-serialno 2>/dev/null | tr -d '\r')" in
    *:[0-9]*|*._tcp) ;;
    *) "$ADB" shell svc wifi disable >/dev/null 2>&1 ;;
  esac
}

wait_offline() {
  w=0
  while [ "$w" -lt 30 ] && [ "$(adb_state)" = device ]; do
    w=$((w + 1))
    sleep 1
  done
}

wait_online() {
  r=0
  until [ "$(adb_state)" = device ]; do
    case "${ANDROID_SERIAL:-}" in
      *:[0-9]*)
        if [ $((r % 5)) = 0 ]; then
          "$ADB" disconnect "$ANDROID_SERIAL" >/dev/null 2>&1
          "$ADB" connect "$ANDROID_SERIAL" >/dev/null 2>&1
        fi
        ;;
    esac
    r=$((r + 1))
    sleep 2
  done
}

wait_boot() {
  wait_online
  n=0
  while [ "$n" -lt 120 ]; do
    [ "$(adb_state)" = device ] || wait_online
    [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = 1 ] && break
    n=$((n + 1))
    sleep 2
  done
  sleep "$SETTLE"
}

EXEC_ARG=
REMOTE_SCRIPT=
stage() {
  if [ "$(adb_state)" != device ]; then
    echo "Device is not online"
    return 1
  fi
  "$ADB" shell "rm -rf $CLEAN; mkdir -p $REMOTE_DIR" >/dev/null 2>&1 || { echo "Could not prepare $REMOTE_DIR"; return 1; }
  "$ADB" push "$BIN" "$REMOTE_DIR/$EXPLOIT_NAME" >/dev/null 2>&1 || { echo "Could not push $EXPLOIT_NAME"; return 1; }
  "$ADB" shell "chmod 755 $REMOTE_DIR/$EXPLOIT_NAME" >/dev/null 2>&1 || { echo "Could not chmod $EXPLOIT_NAME"; return 1; }
  [ -n "$TARBALL" ] || return 0

  base=$(basename "$TARBALL")
  "$ADB" push "$TARBALL" "$REMOTE_DIR/$base" >/dev/null 2>&1 || { echo "Could not push $base"; return 1; }
  "$ADB" shell "cd $REMOTE_DIR && { tar xf '$base' || gzip -dc '$base' | tar xf - || gunzip -c '$base' | tar xf -; } 2>/dev/null; true" >/dev/null 2>&1
  REMOTE_SCRIPT=$("$ADB" shell "find $REMOTE_DIR -type f -name '*.sh' | LC_ALL=C sort | head -n 1" 2>/dev/null | tr -d '\r')
  if [ -z "$REMOTE_SCRIPT" ]; then
    echo "No .sh found inside $base; the device tar said:"
    "$ADB" shell "cd $REMOTE_DIR && tar xf '$base'" 2>&1 | sed 's/^/  /'
    return 1
  fi
  "$ADB" shell "chmod 755 '$REMOTE_SCRIPT'" >/dev/null 2>&1
  EXEC_ARG="--exec $REMOTE_SCRIPT"
}

probe() {
  if [ -n "$TARBALL" ]; then
    exited="grep -aq 'exited status=' $LOG"
  else
    exited=false
  fi
  "$ADB" shell "{ if $exited; then echo exited; elif $IS_READY; then echo ready; elif $IS_ROOTED; then echo rooted; elif $HAS_FAILED; then echo failed; else echo running; fi; } 2>/dev/null" 2>/dev/null | tr -d '\r' | tail -n 1
}

for i in $(seq "$TRIES"); do
  echo "Attempt $i of $TRIES"

  if [ "$(adb_state)" = device ]; then
    "$ADB" reboot >/dev/null 2>&1
    wait_offline
  fi
  wait_boot
  disable_wifi
  if ! stage; then
    echo "Attempt $i failed"
    continue
  fi

  "$ADB" shell "cd $REMOTE_DIR && (nohup ./$EXPLOIT_NAME $EXEC_ARG >$LOG 2>&1 &); sleep 1" >/dev/null 2>&1
  "$ADB" shell "sleep 1; tail -f -n +1 $LOG" 2>/dev/null &
  TAIL=$!

  result=timeout
  rooted=0
  limit=$ROOT_TIMEOUT
  waited=0
  downs=0
  while [ "$waited" -lt "$limit" ]; do
    sleep 1
    waited=$((waited + 1))
    if [ "$(adb_state)" != device ]; then
      downs=$((downs + 1))
      if [ "$downs" -ge 3 ]; then
        result=offline
        break
      fi
      continue
    fi
    downs=0
    state=$(probe)
    case "$state" in
      exited|failed)
        result=$state
        break
        ;;
      ready|rooted)
        if [ "$rooted" = 0 ]; then
          rooted=1
          waited=0
          if [ -n "$TARBALL" ]; then
            limit=$EXEC_TIMEOUT
          else
            limit=$POSTWAIT
          fi
        fi
        if [ "$state" = ready ] && [ -z "$TARBALL" ]; then
          result=ready
          break
        fi
        ;;
    esac
  done
  stop_tail

  if [ "$result" = exited ]; then
    status=$("$ADB" shell "grep -a 'exited status=' $LOG | tail -n 1 | sed 's/.*status=//' | tr -dc '0-9-'" 2>/dev/null)
    echo "$(basename "$REMOTE_SCRIPT") exited with status ${status:-unknown}"
    case "$status" in
      ''|*[!0-9]*) exit 1 ;;
      *) exit "$status" ;;
    esac
  fi

  if [ "$rooted" = 1 ] && [ -n "$TARBALL" ]; then
    if [ "$result" = offline ]; then
      echo "Rooted, but the device went offline before $(basename "$REMOTE_SCRIPT") finished; see $LOG"
    else
      echo "Rooted, but $(basename "$REMOTE_SCRIPT") did not finish within ${EXEC_TIMEOUT}s; see $LOG"
    fi
    exit 1
  fi

  if [ "$rooted" = 1 ] && [ "$result" != offline ]; then
    exec "$ADB" shell -t "$ROOT_SHELL"
  fi

  case "$result" in
    failed) echo "Attempt $i failed: the exploit gave up" ;;
    offline) echo "Attempt $i failed: the device went offline" ;;
    *) echo "Attempt $i failed: timed out" ;;
  esac
done

echo "Failed after $TRIES attempts"
exit 1
