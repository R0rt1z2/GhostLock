# GhostLock 4.4

This is a kernel root exploit for the Amazon `sheldon` family, the Fire TV Stick 3rd Gen (`sheldonp`) and the Fire TV Stick Lite 2020 (`sheldon`), adapted from CVE-2026-43499 in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

## Supported devices

Only the `sheldon` family is supported for now, on Fire OS 7 (based on Android 9).

### Fire TV Stick 3rd Gen (`sheldonp`)

| Fire OS | Build | Incremental | Kernel |
|---|---|---|---|
| 7.7.1.6 * | PS7716.5666N | `0036005356164` | `4.4.162+` |
| 7.7.1.5 | PS7715.5586N | `0035736900228` | `4.4.162+` |
| 7.7.1.5 | PS7715.5585N | `0035736899972` | `4.4.162+` |
| 7.7.1.4 | PS7714.5507N | `0035602662276` | `4.4.162+` |
| 7.7.1.4 | PS7714.5506N | `0035602662020` | `4.4.162+` |
| 7.7.1.4 | PS7714.5504N | `0035602661508` | `4.4.162+` |
| 7.7.1.3 | PS7713.5443N | `0035334210436` | `4.4.162+` |
| 7.7.1.2 | PS7712.5370N | `0035199974020` | `4.4.162+` |
| 7.7.1.1 | PS7711.5273N | `0034763741572` | `4.4.162+` |
| 7.6.9.7 | PS7697.4800N | `0032783908996` | `4.4.162+` |
| 7.6.7.1 | PS7671.4097N | `0029965156740` | `4.4.162+` |
| 7.6.6.9 | PS7669.4007N | `0029696698244` | `4.4.162+` |
| 7.6.6.4 | PS7664.3772N | `0029159767172` | `4.4.162+` |
| 7.6.5.2 | PS7652.3564N | `0028488625284` | `4.4.162+` |
| 7.6.4.6 | PS7646.3560N | `0028085971076` | `4.4.162+` |
| 7.6.3.3 | PS7633.3445N | `0027347744132` | `4.4.162+` |
| 7.6.2.4 | PS7624.3337N | `0026810845572` | `4.4.162+` |
| 7.6.1.4 | PS7614.3227N | `0025938402180` | `4.4.162+` |
| 7.2.9.2 | PS7292.2982N | `0024126400132` | `4.4.162+` |
| 7.2.7.3 | PS7273.2625N | `0022851240324` | `4.4.162+` |
| 7.2.4.2 | PS7242.2907N | `0021710461828` | `4.4.162+` |
| 7.2.4.9 | PS7249.2719N | `0021039325060` | `4.4.162+` |
| 7.2.3.4 | PS7234.2042N | `0020166736516` | `4.4.162+` |

### Fire TV Stick Lite 2020 (`sheldon`)

| Fire OS | Build | Incremental | Kernel |
|---|---|---|---|
| 7.7.1.5 | PS7715.5586N | `0035736900228` | `4.4.162+` |
| 7.7.1.5 | PS7715.5585N | `0035736899972` | `4.4.162+` |
| 7.7.1.4 | PS7714.5507N | `0035602662276` | `4.4.162+` |
| 7.7.1.4 | PS7714.5506N | `0035602662020` | `4.4.162+` |
| 7.7.1.4 | PS7714.5504N | `0035602661508` | `4.4.162+` |
| 7.7.1.2 | PS7712.5370N | `0035199974020` | `4.4.162+` |
| 7.7.1.1 | PS7711.5273N | `0034763741572` | `4.4.162+` |
| 7.7.0.8 | PS7708.5501N | `0034394701188` | `4.4.162+` |
| 7.7.0.6 | PS7706.5106N | `0033924838020` | `4.4.162+` |
| 7.7.0.4 | PS7704.5029N | `0033723491716` | `4.4.162+` |
| 7.7.0.4 | PS7704.5024N | `0033723490436` | `4.4.162+` |
| 7.6.9.9 | PS7699.4896N | `0033052369028` | `4.4.162+` |
| 7.6.9.7 | PS7697.4800N | `0032783908996` | `4.4.162+` |
| 7.6.7.1 | PS7671.4097N | `0029965156740` | `4.4.162+` |
| 7.6.6.9 | PS7669.4007N | `0029696698244` | `4.4.162+` |
| 7.6.6.4 | PS7664.3772N | `0029159767172` | `4.4.162+` |
| 7.6.5.2 | PS7652.3564N | `0028488625284` | `4.4.162+` |
| 7.6.5.2 | PS7652.3556N | `0028488623236` | `4.4.162+` |
| 7.6.4.6 | PS7646.3550N | `0028085968516` | `4.4.162+` |
| 7.6.3.3 | PS7633.3445N | `0027347744132` | `4.4.162+` |
| 7.6.2.4 | PS7624.3337N | `0026810845572` | `4.4.162+` |
| 7.6.1.4 | PS7614.3227N | `0025938402180` | `4.4.162+` |
| 7.6.0.8 | PS7608.3614N | `0025468739204` | `4.4.162+` |
| 7.2.8.5 | PS7285.2880N | `0023723720836` | `4.4.162+` |
| 7.2.8.5 | PS7285.2877N | `0023723720068` | `4.4.162+` |
| 7.2.7.3 | PS7273.2622N | `0022851239556` | `4.4.162+` |
| 7.2.4.2 | PS7242.2907N | `0021710461828` | `4.4.162+` |
| 7.2.4.2 | PS7242.2896N | `0021710459012` | `4.4.162+` |
| 7.2.4.9 | PS7249.2719N | `0021039325060` | `4.4.162+` |

## Building

Requires GNU Make and Android NDK r29.

```sh
make
```

Or build a distributable zip, which handles everything for you:

```sh
./makedist.sh
```

The result is written to `dist/ghostlock-sheldon-v<version>.zip`, where the version comes from the `VERSION` file (override with `VERSION=x.y.z ./makedist.sh`).

## Usage

```sh
./root.sh
```

The script reboots the device, runs the exploit, and drops you into a root shell when it finishes. The heap reclaim is probabilistic, so it retries automatically (up to 15 times); each failed attempt either retries or reboots the device on its own.

If the device is attached over USB, the script also turns Wi-Fi off on each attempt, which makes the exploit noticeably more reliable.

If successful, you will see a root shell prompt like this:

```
sheldon:/ #
```

You get UID 0 with a full capability set, and SELinux is switched to Permissive automatically. `su` works from any `adb shell` until the next reboot.

Root is temporary and is lost on reboot. It is also best used promptly: the exploit holds kernel state that decays over time, so do your work soon after it lands rather than leaving the device idle in the rooted state.

> [!CAUTION]
> This device uses dm-verity, so any modification to the system/vendor/etc. partitions will result in a brick. Damaging critical partitions such as Preloader, LK, or TEE will also result in a brick. Use this exploit at your own risk.

## OTAs

OTA updates are disabled automatically as soon as root succeeds; the exploit disables `com.amazon.device.software.ota` and its override package. A single OTA could otherwise move you to a build this exploit does not support, or one that fixes it.

This survives reboots but not a factory reset. To re-enable, `pm enable` the same packages from a root shell.

## Credits

- [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia): original IonStack (CVE-2026-43499) exploit
- [IonStackQuest3](https://github.com/F-19-F/IonStackQuest3): ARM32 `setsockopt` stack-stamping reference
- [gitchw/ghostlock-cve-2026-43499](https://github.com/gitchw/ghostlock-cve-2026-43499): ARM32 futex-PI UAF reference
