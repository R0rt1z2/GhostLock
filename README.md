# GhostLock 4.14

Temporary root for the Amazon Fire 7 (12th Gen) on 4.14 kernels, adapted from CVE-2026-43499 in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

Supported builds, downloads, instructions and warnings are all in the XDA thread. Read it before running anything:

- Fire 7 (2022), `quartz`: <https://xdaforums.com/t/temp-root-amazon-fire-7-12th-gen-quartz.4810054/>

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
- [YuKongA](https://github.com/YuKongA): kernel-profile extraction around this bug class
