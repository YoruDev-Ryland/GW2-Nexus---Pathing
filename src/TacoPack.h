#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// TacoPack data structures
//
// The TacO / BlishHUD pathing pack format:
//   .taco file  —  a ZIP archive containing:
//     ├─ One or more .xml files  (OverlayData with MarkerCategory + POIs)
//     ├─ .trl binary trail files
//     └─ Texture files (.png / .jpg referenced by markers)
//
// XML schema overview:
//   <OverlayData>
//     <MarkerCategory name="..." displayName="..." iconFile="..." ...>
//       <MarkerCategory .../> ...
//     </MarkerCategory>
//     <POIs>
//       <POI  MapID="..." xpos="..." ypos="..." zpos="..." type="..." GUID="..." .../>
//       <Trail MapID="..." trailData="path/to/file.trl" type="..." .../>
//     </POIs>
//   </OverlayData>
//
// TRL binary format:
//   uint32_t  version     (4 bytes — always 0, must be skipped)
//   uint32_t  mapId       (4 bytes)
//   float[3]  point[0]   (12 bytes each)
//   float[3]  point[1]
//   ...
// ─────────────────────────────────────────────────────────────────────────────

// ── Display attributes that can cascade down category trees ──────────────────
// Absent/unset values are indicated by sentinel values (NaN for floats, -1 for int).
struct MarkerAttribs
{
    std::string iconFile;               // path within pack, e.g. "Data/icon.png"
    float       iconSize    = 1.0f;     // multiplied by global MarkerScale
    float       alpha       = 1.0f;     // 0.0–1.0
    uint32_t    color       = 0xFFFFFFFF; // ARGB
    float       heightOffset = 1.5f;    // world-units above ground
    float       fadeNear    = -1.0f;    // world-units; negative = use global
    float       fadeFar     = -1.0f;    // world-units; negative = use global
    float       minSize     = -1.0f;    // screen pixels; negative = use global
    float       maxSize     = -1.0f;    // screen pixels; negative = use global
    int         behavior    = 0;        // 0 = always visible; others = TacO spec
    bool        canFade     = true;     // allow distance fade
    bool        autoTrigger = false;    // auto-trigger (countdown) markers
    float       triggerRange = 2.0f;   // meters — auto-trigger radius
    int         resetLength  = 0;       // seconds; 0 = no reset

    // Trail-specific
    uint32_t    trailColor  = 0xFFFFFFFF;
    float       trailScale  = 1.0f;
    float       animSpeedMult = 1.0f;
    std::string texture;                // trail texture override

    // ── Merge: fill in any unset fields from a parent ───────────────────────
    // Only overrides fields that are still at their "unset" sentinel.
    void InheritFrom(const MarkerAttribs& parent);
};

// ── Category tree node ────────────────────────────────────────────────────────
struct MarkerCategory
{
    std::string name;           // e.g. "tw_dungeons" (last path segment)
    std::string displayName;    // human-readable; falls back to name
    MarkerAttribs attribs;

    bool enabled = true;        // user can toggle categories on/off
    bool expanded = false;      // UI tree node state

    std::vector<MarkerCategory> children;

    // Navigate the category tree by a dot-separated type string.
    // Returns nullptr if not found.
    MarkerCategory* Find(const std::string& path);
    const MarkerCategory* Find(const std::string& path) const;

    // Walk the type path and create any missing nodes, returning the leaf.
    MarkerCategory* FindOrCreate(const std::string& path);
};

// ── POI (Point of Interest) ───────────────────────────────────────────────────
struct Poi
{
    uint32_t    mapId  = 0;
    float       x = 0.f, y = 0.f, z = 0.f;
    std::string type;       // dot-separated category path e.g. "tw_meta.alliances"
    std::string guid;
    MarkerAttribs attribs;  // already merged with category attribs at load time

    // Runtime — Nexus texture ID string (registered by PackManager)
    std::string texId;
};

// ── Trail ─────────────────────────────────────────────────────────────────────
struct TrailPoint { float x, y, z; };

struct Trail
{
    uint32_t    mapId = 0;
    std::string type;
    std::string trailDataFile;          // path within pack
    MarkerAttribs attribs;

    std::vector<TrailPoint> points;     // loaded from .trl binary

    // Runtime
    std::string texId;
};

// ── A single loaded pack ───────────────────────────────────────────────────────
struct TacoPack
{
    std::string name;           // derived from filename, e.g. "Tekkit's Workshop"
    std::string filePath;       // absolute path to .taco or directory
    bool        enabled = true; // user-level enable/disable for the whole pack

    std::vector<MarkerCategory> categories;  // root-level category nodes
    std::vector<Poi>            pois;
    std::vector<Trail>          trails;

    // Extracted files live here (temp dir or addon data dir subfolder)
    // key = lowercase normalised path within zip, value = absolute path on disk
    std::unordered_map<std::string, std::string> extractedFiles;

    // Resolve a pack-relative path to the extracted absolute path.
    // Returns empty string if not found.
    std::string ResolveFile(const std::string& packRelPath) const;

    // True if a category at the given dot-path is enabled by the user.
    bool IsCategoryEnabled(const std::string& typePath) const;
};
