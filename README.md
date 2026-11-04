# GhostLock 5.4

Temporary root for the Amazon Fire HD 8 (12th Gen) on 5.4 kernels, adapted from CVE-2026-43499 in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

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
