# GhostLock 5.10

This is a kernel root exploit for the Amazon Fire Max 11 (`sunstone`) and the Fire TV Stick 4K Max 2nd Gen / 2023 (`karat`), adapted from CVE-2026-43499 in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

## Supported devices

Two devices are supported, both on Fire OS 8. Each has its own build target.

### Fire Max 11 (`sunstone`)

| Fire OS | Build | Incremental | Kernel |
|---|---|---|---|
| 8.3.3.8 | RS8338.3339N | `0030132734852` | `-ge7a6aa0ea53f` |
| 8.3.3.7 | RS8337.3186N | `0029461607044` | `-gee3b4a65f51d` |
| 8.3.3.6 | RS8336.3103N | `0028656279428` | `-gda010c50409f` |
| 8.3.3.5 | RS8335.2972N | `0027716721796` | `-g7ec28efd738a` |
| 8.3.3.3 | RS8333.2734N | `0026441592452` | `-g76402f9d634d` |
| 8.3.3.2 / 8.3.3.1 / 8.3.3.0 / 8.3.2.7 | RS8332.3115N | `0025837710212` | `-gf66ba269681b` |
| 8.3.2.4 | RS8324.2314N | `0023153150596` | `-g82c06325e9f8` |
| 8.3.2.2 | RS8322.2053N | `0022045787524` | `-g89f110867b4a` |
| 8.3.2.1 | RS8321.1924N | `0021911536772` | `-g096b58f0a5d7` |
| 8.3.2.0 | RS8320.1807N | `0021777289092` | `-g66ce35ca5a96` |
| 8.3.1.9 | RS8319.1664N | `0021508817028` | `-gec87eda1378d` |

### Fire TV Stick 4K Max 2nd Gen (`karat`)

| Fire OS | Build | Incremental | Kernel |
|---|---|---|---|
| 8.1.8.5 | RS8185.3891N | `0033018557316` | `-g8b4baa819147` |
| 8.1.8.2 | RS8182.3811N | `0032548774788` | `-g76bdd754c12c` |
| 8.1.8.0 | RS8180.3739N | `0032280320768` | `-g76bdd754c12c` |
| 8.1.8.0 | RS8180.3729N | `0032280318208` | `-g76bdd754c12c` |
| 8.1.7.4 | RS8174.3648N | `0031877644288` | `-ge1cb3cb5ad98` |
| 8.1.7.4 | RS8174.3641N | `0031877642496` | `-ge1cb3cb5ad98` |
| 8.1.6.9 | RS8169.3556N | `0031139423232` | `-ge1cb3cb5ad98` |
| 8.1.6.6 | RS8166.3482N | `0030736751236` | `-gff609fc8f789` |
| 8.1.6.0 | RS8160.3380N | `0030501844100` | `-ga8c852b26125` |
| 8.1.6.0 | RS8160.3372N | `0030501842052` | `-ga8c852b26125` |
| 8.1.5.8 | RS8158.4105N | `0030468475268` | `-gb7ca23cc70e6` |
| 8.1.5.5 | RS8155.3474N | `0029998551684` | `-g05616ee8f7f7` |
| 8.1.5.3 | RS8153.3202N | `0029528720004` | `-g05616ee8f7f7` |
| 8.1.4.9 | RS8149.3133N | `0028723395972` | `-gb0c9d6e017fb` |
| 8.1.4.5 | RS8145.3070N | `0028186508932` | `-gabbd3a8e8dee` |


## Building

Requires GNU Make and Android NDK r29.

```sh
make                 # Fire Max 11 (sunstone), the default
make PROJECT=karat   # Fire TV Stick 4K Max 2nd Gen
```

Or build a distributable zip, which handles everything for you:

```sh
./makedist.sh                 # sunstone
PROJECT=karat ./makedist.sh   # karat
```

The result is written to `dist/ghostlock-<project>-v<version>.zip`, where the
version comes from the `VERSION` file (override with `VERSION=x.y.z ./makedist.sh`).

## Usage

```sh
./root.sh
```

On Windows, run `root.bat`.

The script reboots the device, runs the exploit, and drops you into a root shell when it finishes. If it fails, it will automatically retry up to 8 times.

If the device is attached over USB, the script also turns Wi-Fi off on each attempt, which makes the exploit noticeably more reliable. 

If successful, you will see a root shell prompt like this:

```
root@sunstone:/ #
```

On newer versions of Fire OS, Amazon added KASLR, so it might take a few minutes to fully run. Be patient. Once it is up, `su` works from any `adb shell` until the next reboot.

There is also a bind shell on `127.0.0.1:9999`, kept open for as long as root lasts. The easiest way in is to run `nc` on the device itself:

```sh
adb shell -t "nc 127.0.0.1 9999"
```

Or forward the port and connect from the host:

```sh
adb forward tcp:9999 tcp:9999
stty raw -echo; nc 127.0.0.1 9999; stty sane
```

> [!CAUTION]
> While you are root, our device still uses dm-verity, so any modification to the system/vendor/etc. partitions will result in a brick.
>
> Damaging critical partitions such as LK, TEE, or Preloader will also result in a brick. Use this exploit at your own risk.

### LM shell

On the Fire TV Stick, a second raw root shell is opened on `127.0.0.1:9060` for [Launcher Manager](https://xdaforums.com/t/app-firetv-noroot-launcher-manager-change-launcher-without-root.4176349/).

LM normally spawns its own as the system user, but it reuses the port if it is already open, so if the exploit is holding `9060` with a root shell, it should operate as root instead.

Run the exploit first so it claims the port, then (re)launch LM.

## OTAs

OTA updates are disabled automatically as soon as root succeeds. A single OTA can move you to a build that is not supported here, or that fixes the exploit.

The exact packages differ between the tablet and the Fire TV Stick, the exploit picks the right set based on the device. Look for `OTA: pm ... ok` lines in the output to confirm.

If any step reports `not applied`, run it yourself from the root shell.

On the Fire Max 11 (`sunstone`):

```sh
pm disable com.amazon.device.software.ota
pm disable com.amazon.kindle.otter.oobe.forced.ota
sync
```

On the Fire TV Stick (`karat`):

```sh
pm disable com.amazon.device.software.ota
pm clear com.amazon.device.software.ota
pm disable com.amazon.device.software.ota.override
pm disable-user com.amazon.sneakpeek
pm disable com.amazon.client.metrics
sync
```

This survives reboots but not a factory reset. To re-enable, `pm enable` (or `pm enable-user`) the same packages from a root shell.

## Credits

- [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia): original IonStack (CVE-2026-43499) exploit
- [IonStackQuest3](https://github.com/F-19-F/IonStackQuest3): first public port for 5.10 kernels
- [Pro-me3us](https://github.com/Pro-me3us): karat firmware URL collection
- [sweenwolf](https://github.com/sweenwolf): Launcher Manager, used on FTVs