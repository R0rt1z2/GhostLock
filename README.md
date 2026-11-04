# GhostLock 4.14

Temporary root for the Amazon Fire TV Stick 4K Max 1st Gen (`kara`) on 4.14 kernels, adapted from CVE-2026-43499 in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

Supported builds, downloads, instructions and warnings are all in the XDA thread. Read it before running anything:

- Fire TV Stick 4K Max 1st Gen, `kara`: <https://xdaforums.com/t/temp-root-fire-tv-stick-4k-max-1st-gen-kara.4801361/>

## Building

Requires GNU Make and Android NDK r29 (`makedist.sh` downloads it if it is missing).

```sh
./makedist.sh
```

The zip is written to `dist/`.

## Credits

- [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia): original IonStack (CVE-2026-43499) exploit
- [IonStackQuest3](https://github.com/F-19-F/IonStackQuest3): ARM32 `setsockopt` stack-stamping reference
- [gitchw/ghostlock-cve-2026-43499](https://github.com/gitchw/ghostlock-cve-2026-43499): ARM32 futex-PI UAF reference
