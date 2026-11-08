# GhostLock 5.15

Temporary root for TCL Google TVs on the T653T02 platform (MediaTek MT5896) on 5.15 kernels, adapted from CVE-2026-43499 in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

One firmware serves the whole lineup, so every one of these reports `ro.product.device=G08` and uses the same offsets:

- `QM6K`, `QM67K`, `QM7K`, `QM8K`, `X11K`, `Q77K`
- `RM7L`, `QM7L`, `Q77L`
- North America: `QM891G`, `QM851G`, `QM751G`, `Q651G`, `Q750G`, `S551G`

The right profile is picked automatically from the build fingerprint:

| Build | Kernel | Status |
| --- | --- | --- |
| `UTT2.250416.001/AU14` | 5.15.180 | tested on a QM6K Pro 65" |
| `UTT2.250416.001/AU22` (V339) | 5.15.192 | tested on a QM6K Pro 65" |

Other regional firmwares for this platform are very likely vulnerable too, but each build needs its own entry in `src/registry.c`.

## Building

Requires GNU Make and Android NDK r29 (`makedist.sh` downloads it if it is missing).

```sh
./makedist.sh
```

The zip is written to `dist/`.

## Credits

- [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia): original IonStack (CVE-2026-43499) exploit
- [IonStackQuest3](https://github.com/F-19-F/IonStackQuest3): first public port for 5.10 kernels
- [sshell](https://github.com/sshell/GhostLock): 5.4 kernel port
- [ghostlock-sabrina](https://github.com/k-o-n-t-o-r/ghostlock-sabrina): 5.15 kernel port
