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
