# GhostLock 4.4

Temporary root for the Amazon Fire TV Stick Lite (`sheldon`) and Fire TV Stick 3rd Gen (`sheldonp`) on 4.4 kernels, adapted from CVE-2026-43499 in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

Supported builds, downloads, instructions and warnings are all in the XDA thread. Read it before running anything:

- Fire TV Stick Lite / 3rd Gen, `sheldon` / `sheldonp`: <https://xdaforums.com/t/temp-root-downgrade-fire-tv-stick-lite-3rd-gen-sheldon-sheldonp.4800976/>

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
