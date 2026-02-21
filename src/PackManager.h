#pragma once
#include "TacoPack.h"
#include <string>
#include <vector>
#include <functional>
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
// PackManager
//
// Responsibilities:
//   •  Discover .taco files (and pack directories) under the Pathing addon dir
//   •  Extract .taco archives to a per-pack temp directory
//   •  Parse the XML + trail binaries into TacoPack structs
//   •  Register all pack textures with the Nexus texture API
//   •  Expose the loaded packs and provide a fast per-map filtered view
//   •  Persist per-category enable/disable state
//   •  Run loading on a background thread to avoid hitching the game
// ─────────────────────────────────────────────────────────────────────────────
namespace PackManager
{

// ── Lifecycle ─────────────────────────────────────────────────────────────────

// Call once from AddonLoad (after APIDefs is set).
// Begins background scan & load of all packs found under the addon directory.
void Init();

// Call from AddonUnload.  Waits for any in-flight background work to finish.
void Shutdown();

// ── Pack access ───────────────────────────────────────────────────────────────

// Returns all loaded packs (main-thread read; loading thread posts completed
// packs under a lock).
const std::vector<TacoPack>& GetPacks();

// Returns mutable access (used by UI to toggle enabled state).
std::vector<TacoPack>& GetPacksMutable();

// ── Filtered data for the current map ────────────────────────────────────────

// Returns pointers to all enabled POIs for the given map ID, sorted by
// distance for front-to-back draw ordering.  The pointers are into the
// TacoPack data and remain valid until next Reload().
std::vector<const Poi*>   GetPoisForMap(uint32_t mapId);
std::vector<const Trail*> GetTrailsForMap(uint32_t mapId);

// ── Operations ────────────────────────────────────────────────────────────────

// Reload all packs from disk (async).  Used after the user drops a new pack.
void Reload();

// Return loading status for display in the UI.
bool   IsLoading();
int    LoadedPackCount();
int    TotalPoiCount();
int    TotalTrailCount();

// Call once per frame from the RT_Render callback (main thread).
// Drains any pending texture-registration requests posted by the background
// loader — Nexus texture API calls must be made on the render thread.
void   FlushPendingTextures();

// Returns the root addon data directory, e.g. "<GW2>/addons/Pathing/"
std::string AddonDataDir();

// Returns the packs sub-directory where .taco files should be placed.
std::string PacksDir();

// ── Category state persistence ────────────────────────────────────────────────

// Save/load which categories are enabled for each pack.
void SaveCategoryState();
void LoadCategoryState();

} // namespace PackManager
