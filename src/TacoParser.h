#pragma once
#include "TacoPack.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// TacoParser
//
// Parses TacO / BlishHUD XML overlay data and .trl binary trail files.
// The parser is deliberately permissive — missing or malformed attributes
// are silently ignored so real-world packs with quirks still load.
// ─────────────────────────────────────────────────────────────────────────────
namespace TacoParser
{

// Parse a single XML file (already read into memory as a string) and append
// all categories/POIs/trails into the provided TacoPack.
// The pack's extractedFiles map is used to resolve trail binary paths.
void ParseXml(const std::string& xmlContent, TacoPack& out);

// Two-pass helpers — use these when loading a pack with multiple XML files
// so that category definitions from one file are available to POI/trail
// parsers in all other files regardless of iteration order.

// Pass 1: build only the MarkerCategory tree; do not parse POIs or Trails.
void ParseXmlCategories(const std::string& xmlContent, TacoPack& out);

// Diagnostic counters filled by ParseXmlPois — every reason a trail
// element can be dropped is tracked separately so callers can log them.
struct TrailLoadStats
{
    int xmlTrailNodes  = 0; // total <Trail> elements seen
    int noDataAttr     = 0; // trailData attribute missing or empty
    int fileNotFound   = 0; // ResolveFile returned empty (path key mismatch)
    int binaryFailed   = 0; // LoadTrailBinary returned false
    int noMapId        = 0; // mapId == 0 after binary load
    int noPoints       = 0; // points vector empty after binary load
    int loaded         = 0; // successfully added to pack
    std::string sampleMissingPath; // first path that failed ResolveFile
};

// Pass 2: parse only POIs and Trails; assumes categories already built.
// stats is optional — pass nullptr if you don't need diagnostics.
void ParseXmlPois(const std::string& xmlContent, TacoPack& out,
                  TrailLoadStats* stats = nullptr);

// Load the binary trail data from a .trl file on disk and fill trail.points.
// Returns true on success.
bool LoadTrailBinary(const std::string& absolutePath, Trail& trail);

// Load the binary trail data from a memory buffer (for inline extraction).
// Returns true on success.
bool LoadTrailBinaryMemory(const void* data, size_t size, Trail& trail);

// Normalise a pack-relative path to a lowercase, forward-slash-only form
// suitable for use as a map key.
std::string NormalisePath(const std::string& raw);

} // namespace TacoParser
