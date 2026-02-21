# Pathing — Nexus Addon

A [Nexus](https://raidcore.gg/Nexus) addon for Guild Wars 2 that renders TacO / BlishHUD
compatible pathing packs directly in-game — no BlishHUD required.

---

## Features

- Loads **.taco** pathing packs (the same format used by TacO and BlishHUD)
- Full **MarkerCategory** tree with per-category enable/disable
- World-space **POI billboard** rendering projected from MumbleLink camera data
- World-space **trail breadcrumb** rendering from `.trl` binary data
- Distance-based **fade** and global **opacity / scale** controls
- **Background loading** — packs load on a worker thread so the game never freezes
- Per-pack and per-category enabled state **persisted to disk**
- Nexus **quick-access bar** icon and keybinds
- GitHub Actions CI — builds a release DLL on every push to `main`

---

## Installation

1. Install [Nexus](https://raidcore.gg/Nexus) if you haven't already.
2. Download `Pathing.dll` from the [Releases](../../releases) page.
3. Place it in `<GW2 folder>/addons/`.
4. Launch Guild Wars 2.
5. Load the addon from your Nexus library

---

## Adding Pathing Packs

1. In-game, click the Pathing icon in the Nexus quick-access bar, or assign a
   keybind via Nexus → Keybinds → "Pathing: Toggle Window".
2. In the Pathing window click **Open Dir**. This opens the packs folder:
   `<GW2>/addons/Pathing/packs/`
3. Drop any `.taco` pack file into that folder.
4. Click **Reload** in the Pathing window.

### Where to find packs

| Pack | URL |
|------|-----|
| Tekkit's Workshop (the most popular) | https://blish-hud.com/module/pathing |
| Other BlishHUD packs | search "gw2 taco pack" |

> **Note:** The same `.taco` files that work with TacO and the BlishHUD Pathing
> module work here.

---

## Keybinds

Register these in Nexus → Options → Keybinds:

| Identifier | Default | Action |
|---|---|---|
| `KB_PATHING_TOGGLEWIN` | (none) | Toggle the pack manager window |
| `KB_PATHING_TOGGLEMARKERS` | (none) | Toggle all marker rendering on/off |
| `KB_PATHING_TOGGLETRAILS` | (none) | Toggle all trail rendering on/off |

---

## Building from Source

### Prerequisites

- Visual Studio 2022 (MSVC) or Build Tools with the MSVC compiler
- CMake 3.20+
- Ninja (included with VS installer)
- Git

### Steps

```powershell
git clone https://github.com/<you>/pathing.git
cd pathing

# Configure
cmake -B build -G "Ninja" `
  -DCMAKE_BUILD_TYPE=RelWithDebInfo `
  -DCMAKE_CXX_COMPILER=cl `
  -DCMAKE_C_COMPILER=cl

# Build
cmake --build build --parallel

# Output: build/Pathing.dll
```

CMake automatically fetches all dependencies (Nexus API header, ImGui v1.80,
nlohmann/json, pugixml, miniz) on first configure.

### Releasing

```bash
# Tag and push (triggers GitHub Actions)
./release.sh v1.0.0
```

The Actions workflow builds the DLL and attaches it to every push as a
"latest" pre-release.  Tagged releases get a proper versioned release.

---

## Architecture

```
entry.cpp           DllMain + GetAddonDef + AddonLoad/Unload
Shared.h/.cpp       Global pointers: APIDefs, Self, MumbleLink, MumbleIdent
Settings.h/.cpp     Persistent settings (JSON)
TacoPack.h          Data structures: MarkerCategory, Poi, Trail, TacoPack
TacoParser.h/.cpp   TacO XML + .trl binary parsing (via pugixml)
PackManager.h/.cpp  ZIP extraction (via miniz), background loading, texture registration
MarkerRenderer.h/.cpp  World-to-screen projection + ImGui DrawList rendering
MathUtils.h         Inline Vec3/Mat4/projection math
UI.h/.cpp           Pack manager window + Nexus options panel
```

### World-space projection

MumbleLink provides the camera position, forward vector, and up vector every
frame.  `MumbleIdent` provides the vertical FOV.  These are used to build a
standard view + perspective projection matrix, which projects each POI and trail
point from GW2 world-space into ImGui screen-space coordinates.  Markers are
drawn onto `ImGui::GetBackgroundDrawList()` so they appear behind game UI but in
front of the game world.

### Pack file layout (TacO format)

```
pack.taco   (ZIP archive)
 ├─ *.xml                     marker definitions (XML)
 │     <OverlayData>
 │       <MarkerCategory ...>
 │       <POIs>
 │         <POI MapID="..." xpos="..." ypos="..." zpos="..." type="..." .../>
 │         <Trail MapID="..." trailData="path.trl" type="..." .../>
 │       </POIs>
 │     </OverlayData>
 ├─ *.trl                     trail binary: uint32 mapId, then N×float[3] points
 └─ *.png / *.jpg             marker icon textures
```

---

## Customising the Quick-Access Icon

Replace `src/icon.png` with your own 64×64 or 128×128 RGBA PNG, then rebuild.

---

## Known Limitations / Future Work

- **No minimap overlay** — the in-game compass overlay is not yet implemented.
- **No behaviour actions** — TacO "behavior" codes (countdown timers, auto-hide on
  trigger, etc.) are parsed but not yet acted upon.
- **No animated trail textures** — trails render as coloured dots/lines; texture
  scrolling is not yet implemented.
- **Pack directory is not watched** — you must click Reload after dropping a new
  pack; file-system watching is a planned improvement.
- **Large packs load slowly** — Tekkit's Workshop has ~100k POIs; initial extract
  takes several seconds.  Subsequent loads use the cached extracted folder.

---

## Contributing

Pull requests welcome.  Please open an issue first for anything large.

---

## License

MIT
