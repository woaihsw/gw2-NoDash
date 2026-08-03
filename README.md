# gw2-NoDash

Nexus addon for disabling the local received-chat filter in the Guild Wars 2
CN client.

The addon does not rewrite chat text or replace the client filter. It changes
the filter's level argument to the native value `0`, which follows the
client's reviewed early-return path.

## Install

Download `gw2-NoDash.dll` from a release or GitHub Actions artifact and place it
in the Nexus `addons` directory. The first load creates:

```text
addons\
  gw2-NoDash.dll
  gw2-NoDash\
    gw2-NoDash.ini
```

The default configuration leaves the client unchanged:

```ini
[NoDash]
mode=-1
```

The file accepts exactly one `[NoDash]` section and one `mode` entry. Duplicate
or unknown entries are rejected and leave native filtering unchanged.

Set `mode=0` to disable local filtering, then restart the complete game process.
The addon disables Nexus hotloading because the hook remains installed until
process shutdown.

This is an unofficial client modification and may carry account risk under the
CN service rules. Use it at your own discretion.

## Runtime Design

Hook installation requires one candidate to pass every check:

1. The exact machine-code sequence selected from the reviewed UTF-16 dash
   replacement loop must occur once across all executable PE sections.
2. The match must belong to a valid `RUNTIME_FUNCTION` range in the PE
   Exception Directory (`.pdata`), and that complete range must remain inside
   an executable section.
3. `filterLevelIndex < marrsize(FilterRec, level)` must occur once in read-only
   data and be referenced from the candidate function by a RIP-relative `LEA`.
4. MinHook is installed at the recovered `RUNTIME_FUNCTION` entry, never at
   the interior byte match.

Configuration and resolver failures occur before MinHook creates a hook. If
enabling fails after creation, the addon immediately attempts to remove the
hook record and logs a critical error when cleanup cannot be confirmed. With
`mode=0`, the detour still calls the original function and changes only its
level argument to `0`.

## Scope

The verified scope is ordinary received chat rendered by the local CN client.
The addon does not change data sent to the server and does not cover mail, LFG,
character names, or other text systems.

## Build

The DLL is built for x64 Windows with CMake and MSVC:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Release builds use the static MSVC runtime and retain optimized code while
emitting source-level debug information to a PDB. The output is
`build/bin/Release/gw2-NoDash.dll`; GitHub Actions publishes the DLL and PDB as
the `gw2-NoDash-dll` artifact.

## License

[MIT](LICENSE)
