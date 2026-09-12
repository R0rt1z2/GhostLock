# GhostLock 4.14

This is a kernel root exploit for the Amazon Fire TV Stick 4K Max 1st Gen (`kara`, AFTKA), adapted from CVE-2026-43499 in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

## Supported devices

Only `kara` is supported for now, on Fire OS 7 (based on Android 9), MediaTek MT8696.

### Fire TV Stick 4K Max 1st Gen (`kara`)

| Fire OS | Build | Incremental | Kernel |
|---|---|---|---|
| 7.7.1.3 | PS7713.5443N | `0035334210436` | `4.14.87+` |
| 7.7.1.2 | PS7712.5372N | `0035199974532` | `4.14.87+` |
| 7.7.1.1 | PS7711.5272N | `0034763741316` | `4.14.87+` |
| 7.7.0.8 | PS7708.5501N | `0034394701188` | `4.14.87+` |
| 7.7.0.6 | PS7706.5106N | `0033924838020` | `4.14.87+` |
| 7.7.0.4 | PS7704.5024N | `0033723490436` | `4.14.87+` |
| 7.7.0.2 | PS7702.4965N | `0033455039876` | `4.14.87+` |
| 7.6.9.9 | PS7699.4896N | `0033052369028` | `4.14.87+` |
| 7.6.9.9 | PS7699.4894N | `0033052368516` | `4.14.87+` |
| 7.6.9.7 | PS7697.4800N | `0032783908996` | `4.14.87+` |
| 7.6.9.0 | PS7690.4716N | `0032381234308` | `4.14.87+` |
| 7.6.8.8 | PS7688.4591N | `0032112766852` | `4.14.87+` |
| 7.6.8.5 | PS7685.4486N | `0031642977924` | `4.14.87+` |
| 7.6.8.1 | PS7681.4384N | `0031106080900` | `4.14.87+` |
| 7.6.5.2 | PS7652.3564N | `0028488625284` | `4.14.87+` |
| 7.2.7.3 | PS7273.????? | `0022851240324` | `4.14.87+` |

## Building

Requires GNU Make and Android NDK r29.

```sh
make
```

Or build a distributable zip, which handles everything for you:

```sh
./makedist.sh
```

The result is written to `dist/ghostlock-kara-v<version>.zip`, where the version comes from the `VERSION` file (override with `VERSION=x.y.z ./makedist.sh`).

## Usage

```sh
./root.sh
```

On Windows, run `root.bat`.

The zip bundles `adb` for Linux, macOS and Windows under `bin/`. Both scripts use
`adb` from `PATH` when it is there and fall back to the bundled one otherwise, so
platform-tools is not a prerequisite. Set `ADB=/path/to/adb` to force a specific one.

The script reboots the device, runs the exploit, and drops you into a root shell when it finishes.

The heap reclaim is probabilistic, so it retries automatically (up to 15 times), each failed attempt either retries or reboots the device on its own.

If the device is attached over USB, the script also turns Wi-Fi off on each attempt, which makes the exploit noticeably more reliable.

If successful, you will see a root shell prompt like this:

```
kara:/ #
```

You get UID 0 with a full capability set, and SELinux is switched to Permissive automatically. `su` works from any `adb shell` until the next reboot.

Root is temporary and is lost on reboot. It is also best used promptly: the exploit holds kernel state that decays over time, so do your work soon after it lands rather than leaving the device idle in the rooted state.

> [!CAUTION]
> This device uses dm-verity, so any modification to the system/vendor/etc. partitions will result in a brick. Damaging critical partitions such as Preloader, LK, or TEE will also result in a brick. Use this exploit at your own risk.

## OTAs

OTA updates are disabled automatically as soon as root succeeds. A single OTA could otherwise move you to a build this exploit does not support, or one that fixes it.

The exploit runs:

```sh
pm disable-user com.amazon.device.software.ota
pm clear com.amazon.device.software.ota
pm disable com.amazon.device.software.ota.override
pm disable-user com.amazon.sneakpeek
pm disable com.amazon.client.metrics
sync
```

Each one reports its own result, so the output carries a line per package:

```
[ota] pm disable-user com.amazon.device.software.ota ok
[ota] pm disable com.amazon.sneakpeek not applied
[ota] done
```

`ok` means `pm` accepted the command; because `pm disable` is idempotent it also
means `ok` on a package that was already disabled. `not applied` means `pm` itself
failed, so run that one by hand from a root shell.

PackageManager takes a couple of seconds to write `package-restrictions.xml`, so the
exploit waits before syncing, and `root.sh` waits for `[ota] done` before handing you
the shell. Set `POSTWAIT=0` to skip that wait; the full log is on the device at
`/data/local/tmp/gl/run.log` either way.

This survives reboots but not a factory reset. To re-enable, `pm enable` (or `pm enable-user`) the same packages from a root shell.

## Credits

- [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia): original IonStack (CVE-2026-43499) exploit
- [IonStackQuest3](https://github.com/F-19-F/IonStackQuest3): ARM32 `setsockopt` stack-stamping reference
- [gitchw/ghostlock-cve-2026-43499](https://github.com/gitchw/ghostlock-cve-2026-43499): ARM32 futex-PI UAF reference
