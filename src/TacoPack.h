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

struct MarkerAttribs
{
    std::string iconFile;
    float       iconSize    = 1.0f;
    float       alpha       = 1.0f;
    uint32_t    color       = 0xFFFFFFFF;
    float       heightOffset = 1.5f;
    float       fadeNear    = -1.0f;
    float       fadeFar     = -1.0f;
    float       minSize     = -1.0f;
    float       maxSize     = -1.0f;
    int         behavior    = 0;
    bool        canFade     = true;
    bool        autoTrigger = false;
    float       triggerRange = 2.0f;
    int         resetLength  = 0;

    uint32_t    trailColor  = 0xFFFFFFFF;
    float       trailScale  = 1.0f;
    float       animSpeedMult = 1.0f;
    std::string texture;

    void InheritFrom(const MarkerAttribs& parent);
};

struct MarkerCategory
{
    std::string name;
    std::string displayName;
    MarkerAttribs attribs;

    bool enabled = true;
    bool expanded = false;

    std::vector<MarkerCategory> children;

    MarkerCategory* Find(const std::string& path);
    const MarkerCategory* Find(const std::string& path) const;

    MarkerCategory* FindOrCreate(const std::string& path);
};

struct Poi
{
    uint32_t    mapId  = 0;
    float       x = 0.f, y = 0.f, z = 0.f;
    std::string type;
    std::string guid;
    MarkerAttribs attribs;

    std::string texId;
};

struct TrailPoint { float x, y, z; };

struct Trail
{
    uint32_t    mapId = 0;
    std::string type;
    std::string trailDataFile;
    MarkerAttribs attribs;
    std::vector<TrailPoint> points;
    // Cumulative world-space arc length from point 0 to point i.
    // arcLengths[0] == 0.  Populated once at load time so the renderer can
    // compute stable UVs without accumulating per-frame.
    std::vector<float> arcLengths;
    std::string texId;
};

struct TacoPack
{
    std::string name;
    std::string filePath;
    bool        enabled = true;

    std::vector<MarkerCategory> categories;
    std::vector<Poi>            pois;
    std::vector<Trail>          trails;
    std::unordered_map<std::string, std::string> extractedFiles;
    std::string ResolveFile(const std::string& packRelPath) const;
    bool IsCategoryEnabled(const std::string& typePath) const;
};
