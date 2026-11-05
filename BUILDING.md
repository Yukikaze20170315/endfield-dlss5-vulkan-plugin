# unified-20261103l: source and how to rebuild

This tree (also published as `unified-20261103l-source.zip`) holds the complete source from which the three add-ons in `unified-20261103l-candidate.zip` were built, the build, test and packaging scripts, and the NVAPI SDK that the bridge links. The folder layout is the one the scripts expect; keep it.

| Folder | Content |
| --- | --- |
| `src/unified-20261011/bridge/` | `dlss5-bridge.addon64`, version `1.4.13-pre8-unified2d-20261101`. Based on [NIGos/dlss5-bridge](https://github.com/NIGos/dlss5-bridge) (MIT, see `bridge/LICENSE`). |
| `src/unified-20261011/generic/` | `renodx-dlss5.addon64` (RenoDX DLSS 5 Generic), version `v8.5.0-rc10-unified2l-20261103`. `src/` is the part of a RenoDX checkout that the add-on needs; `generated/` holds the compiled shaders and the version resource. RenoDX is MIT licensed (`generic/src/addons/dlss5/LICENSE`). |
| `src/unified-20261011/mfg/` | `renodx-mfgunlock.addon64` (MFG Unlock), version `0.11+unified2j-20261102`. Based on [nefh/MFGAmpereUnlock-RenoDx](https://github.com/nefh/MFGAmpereUnlock-RenoDx) tag `0.11` (MIT, see `mfg/LICENSE`). `deps/` holds the headers and the static Detours library it builds against, except the NGX headers (see Requirements). |
| `src/unified-20261011/common/` | Headers shared by the three add-ons (deployment state, driver-profile handling, Windows VRAM budget, logging, panel language, hook mode, payload table). |
| `tools/unified-20261011/` | Build script, offline test fixtures, packaging scripts of every intermediate build, the external uninstaller (`DLSS5-Clean-Recovery.ps1`, `DLSS5-Recovery.cmd`) and the restore-helper source (`restore-driver.cpp`). |
| `upstream/NVIDIA-nvapi-87dca62/` | NVAPI SDK (MIT), linked by the bridge and the restore helper. |
| `tools/release-unified-20261103l/` | The script that staged this repository and the release assets, and the secret scan run on them. |

## Requirements

- Windows 10 or 11, x64.
- Visual Studio 2022 Build Tools with the x64 C++ tools and a Windows 10/11 SDK. `tools/unified-20261011/build-unified1s.cmd` calls `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat`; edit that line if Visual Studio is installed elsewhere. The shipped build used MSVC 14.44.35207 and Windows SDK 10.0.26100.0.
- For MFG Unlock: the NGX headers (`nvsdk_ngx*.h`) from the `include` folder of the NVIDIA DLSS SDK (https://github.com/NVIDIA/DLSS), copied to `src/unified-20261011/mfg/deps/ngx/`. They carry NVIDIA's proprietary licence notice and are therefore not included.
- For Generic only: the `external` folder of a RenoDX checkout (Detours, nlohmann json, frozen, gtl, Streamline and DLSS headers, ReShade headers) and the NVIDIA DLSS SDK `include` folder. Point the environment variables `GDEPS` and `NRSDK` to them before building; without them the script falls back to the developer's own paths and stops with exit code 2. The exact RenoDX commit of that `external` folder was not recorded.

## Build

From the repository root:

```bat
set GDEPS=<RenoDX checkout>\external
set NRSDK=<DLSS SDK>\include
tools\unified-20261011\build-unified1s.cmd all unified2l
```

The first argument is `bridge`, `mfg`, `generic`, `tests` or `all`; the second chooses the output folder `builds\unified-20261011\<name>\` (default `unified1s`; the name is historical, the script builds whatever is in the tree). The bridge and MFG Unlock are built with `/W4 /WX`. The Generic link prints the warning LNK4229 about `/DELAYLOAD:winhttp.dll`; it is present in every build of this series and does not stop the build. The `mfg` step also runs the policy fixture (326 checks) and the start-up policy check (10 checks). The `tests` step runs the event-log fixture (67), the Vulkan GPU-name unlock fixture (15) and a fixture that loads the built bridge in a temporary game folder (48).

Rebuilt DLLs do not have the same SHA-256 as the shipped ones, because the PE time stamp changes with every build. The shipped bridge and MFG Unlock were built from this same source in earlier builds of the series (unified2d and unified2j); their source has not changed since, which is why their version strings differ from Generic's.

Use `build-unified1s.cmd`, not `src/unified-20261011/mfg/build.cmd` or `src/unified-20261011/generic/build.cmd`: those are left over from earlier experiments.

## Offline tests

| Script | What it checks |
| --- | --- |
| `test.cmd` | language strings of all 14 panel languages, the crash-handler chain, deployment state, layer plans, safe mode and start-up failure counting (`test-state`); the Streamline entry hooks under 2,000 concurrent calls, the NGX feature path and the bridge panel (`test-loader`) |
| `test-generic-small-stack.cmd` | Generic hooks on 64 KiB thread stacks |
| `test-small-stack.cmd` | bridge hooks on 64 KiB thread stacks |
| `test-health-vram.cmd` | VRAM notice hysteresis |
| `test-drs.cmd` | driver-profile transactions against a stand-in NVAPI session, the maintenance entry started through the system `rundll32`, and the Windows VRAM budget backup/restore (temporary HKCU keys only) |
| `build-nr-selection-test.cmd` | NR runtime selection by GPU architecture and hash |
| `test-vulkan-nr-resolution.py`, `test-vulkan-replacement-serial.py`, `test-nr-demand.py`, `test-nr-replacement-serial.py` | extracted Generic functions for NR working resolution, release-before-create replacement and NR demand |
| `test-clean-recovery.ps1` | the external uninstaller against stand-in registry, process and driver functions |
| `test-deploy.ps1` | the developer install/rollback script (`deploy.ps1`) in a temporary folder; refuses to run while `Endfield.exe` is running |
| `test-fence-deadline-wait.cpp` | the fence wait used by the Present-stage NR recovery, on a real D3D12 fence (WARP) |
| `test-mfg-unload-race.cpp`, `run-mfg-unload-race.ps1` | reproduces the MFG Unlock unload race; needs a pre-fix and a fixed DLL |

None of these fixtures loads the game, changes the real NVIDIA driver profile, the real Windows VRAM budget or the ReShade registration.

## Packaging

Each intermediate package was produced by a `package-unified*.py` script that takes the previous package, replaces only the files that changed, checks every input hash and refuses to overwrite an existing output. `package-unified2l.py` produced `unified-20261103l-candidate` from the unified2k package (new Generic and usage document). `make-payload.py` generates `common/payload.hpp` (the runtime file table compiled into the bridge) from the runtime files. To check an installed copy, compare its files with `SHA256SUMS.txt` in the package.

## Left out

- Build outputs, git metadata and Python caches.
- `src/unified-20261011/mfg/deps/ngx/` (NVIDIA NGX SDK headers; see Requirements).
- `generic/src/addons/dlss5/contrib/dlss5-bridge-vulkan-fg-relay/`: a copy of earlier release material that the build does not use; it is in the git history of this repository.
- The runtime DLLs (`nvngx_*.dll`, `sl.*.dll`); they are in `unified-20261103l-candidate.zip`.
