# GhostLock 5.10

Temporary root for Amazon devices on 5.10 kernels, adapted from CVE-2026-43499 in [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia).

Supported builds, downloads, instructions and warnings are all in the XDA threads. Read the one for your device before running anything:

- Fire Max 11 (2023), `sunstone`: <https://xdaforums.com/t/temp-root-fire-max-11-2023-sunstone.4798590/>
- Fire TV Stick 4K 2nd Gen series, `karat` / `mantra`: <https://xdaforums.com/t/temp-root-fire-tv-stick-4k-2nd-gen-series-karat-mantra.4798627/>

## Building

Requires GNU Make and Android NDK r29 (`makedist.sh` downloads it if it is missing).

```sh
./makedist.sh                 # sunstone
PROJECT=karat ./makedist.sh   # karat
```

The zip is written to `dist/`.

## Credits

- [CyberMeowfia](https://github.com/NebuSec/CyberMeowfia): original IonStack (CVE-2026-43499) exploit
- [IonStackQuest3](https://github.com/F-19-F/IonStackQuest3): first public port for 5.10 kernels
- [Pro-me3us](https://github.com/Pro-me3us): karat firmware URL collection
- [sweenwolf](https://github.com/sweenwolf): Launcher Manager, used on FTVs
