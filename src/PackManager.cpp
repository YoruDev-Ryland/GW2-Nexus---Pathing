#include "PackManager.h"
#include "TacoParser.h"
#include "Shared.h"

#include <miniz.h>
#include <nlohmann/json.hpp>

#include <windows.h>
#include <shlwapi.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <mutex>
#include <thread>
#include <atomic>
#include <sstream>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Internal state
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    std::vector<TacoPack>  g_Packs;
    std::mutex             g_PacksMutex;
    std::atomic<bool>      g_Loading{false};
    std::atomic<int>       g_TotalPois{0};
    std::atomic<int>       g_TotalTrails{0};

    // Background loading thread handle (joined on reload/shutdown)
    std::thread            g_LoadThread;

    // Textures that need to be registered from the main / render thread.
    // Background loader populates this; FlushPendingTextures drains it.
    struct PendingTex { std::string texId; std::string absPath; };
    std::vector<PendingTex> g_PendingTextures;
    std::mutex              g_PendingTexMutex;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string AddonDataDirStatic()
{
    if (!APIDefs) return "";
    std::string dir = APIDefs->Paths_GetAddonDirectory("Pathing");
    // Ensure the directory exists
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

static std::string PacksDirStatic()
{
    std::string dir = AddonDataDirStatic();
    if (dir.empty()) return "";
    dir += "\\packs";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

static std::string ExtractDirForPack(const std::string& packFile)
{
    // Extract to <addondir>/extract/<packname_no_ext>/
    std::string base = packFile;
    // Grab just the filename without extension
    std::string filename = base;
    auto backslash = base.rfind('\\');
    auto fwdslash  = base.rfind('/');
    size_t sep = std::string::npos;
    if (backslash != std::string::npos) sep = backslash;
    if (fwdslash  != std::string::npos) sep = (sep == std::string::npos) ? fwdslash : std::max(sep, fwdslash);
    if (sep != std::string::npos) filename = base.substr(sep + 1);
    // Remove extension
    auto dot = filename.rfind('.');
    if (dot != std::string::npos) filename = filename.substr(0, dot);

    // Sanitise filename for use as directory name
    for (char& c : filename)
        if (c == ' ' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|') c = '_';

    std::string dir = AddonDataDirStatic() + "\\extract\\" + filename;
    // Create nested directories
    CreateDirectoryA((AddonDataDirStatic() + "\\extract").c_str(), nullptr);
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

// Recursively create directories for a file path.
static void EnsureDirectoryForFile(const std::string& filePath)
{
    std::string dir = filePath;
    auto sep = dir.rfind('\\');
    if (sep == std::string::npos) sep = dir.rfind('/');
    if (sep != std::string::npos)
    {
        dir = dir.substr(0, sep);
        // Create all parent directories
        for (size_t i = 0; i < dir.size(); ++i)
        {
            if (dir[i] == '\\' || dir[i] == '/')
            {
                std::string partial = dir.substr(0, i);
                if (!partial.empty()) CreateDirectoryA(partial.c_str(), nullptr);
            }
        }
        CreateDirectoryA(dir.c_str(), nullptr);
    }
}

// Extract all files from a .taco (ZIP) archive into extractDir.
// Fills pack.extractedFiles.  Returns false if the archive can't be opened.
static bool ExtractTacoPack(const std::string& tacoPath, const std::string& extractDir, TacoPack& pack)
{
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, tacoPath.c_str(), 0))
        return false;

    mz_uint fileCount = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < fileCount; ++i)
    {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

        std::string entryName = stat.m_filename;
        std::string normed    = TacoParser::NormalisePath(entryName);
        std::string destPath  = extractDir + "\\" + normed;
        // Replace forward slashes with backslashes for Win32 directory creation
        std::replace(destPath.begin(), destPath.end(), '/', '\\');

        EnsureDirectoryForFile(destPath);

        // Only extract if not already extracted (avoids re-writing unchanged files)
        if (GetFileAttributesA(destPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            if (!mz_zip_reader_extract_to_file(&zip, i, destPath.c_str(), 0))
            {
                // If file extraction failed, skip it
                continue;
            }
        }

        pack.extractedFiles[normed] = destPath;
    }

    mz_zip_reader_end(&zip);
    return true;
}

// Parse all XML files within an extracted pack directory.
// Uses a two-pass strategy so that MarkerCategory definitions from any file
// are always available when POIs and Trails in other files are resolved —
// regardless of the iteration order of the unordered_map.
static void ParseExtractedXmls(TacoPack& pack)
{
    // Collect XML contents up-front (avoids re-opening files for pass 2).
    std::vector<std::string> xmlContents;
    xmlContents.reserve(32);

    for (const auto& [normPath, absPath] : pack.extractedFiles)
    {
        if (normPath.size() < 4) continue;
        std::string ext = normPath.substr(normPath.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        if (ext != ".xml") continue;

        std::ifstream f(absPath);
        if (!f.is_open()) continue;

        xmlContents.emplace_back((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());
    }

    // Pass 1 — build the complete category tree from every XML file.
    for (const auto& content : xmlContents)
        TacoParser::ParseXmlCategories(content, pack);

    // Pass 2 — parse POIs and Trails (category tree is now fully populated).
    for (const auto& content : xmlContents)
        TacoParser::ParseXmlPois(content, pack);
}

// Collect all icon / trail texture paths from a pack into the pending queue.
// Called from the background loader — does NOT touch the Nexus API.
static void QueuePackTextures(TacoPack& pack)
{
    std::lock_guard<std::mutex> lock(g_PendingTexMutex);

    for (auto& poi : pack.pois)
    {
        if (poi.attribs.iconFile.empty() || !poi.texId.empty()) continue;

        std::string normIcon = TacoParser::NormalisePath(poi.attribs.iconFile);
        std::string absPath  = pack.ResolveFile(normIcon);
        if (absPath.empty()) continue;

        std::string texId = "PATHING_" + pack.name + "_" + normIcon;
        std::replace(texId.begin(), texId.end(), '/', '_');
        std::replace(texId.begin(), texId.end(), '\\', '_');
        std::replace(texId.begin(), texId.end(), '.', '_');
        std::replace(texId.begin(), texId.end(), ' ', '_');

        poi.texId = texId;
        g_PendingTextures.push_back({texId, absPath});
    }

    for (auto& trail : pack.trails)
    {
        if (trail.attribs.texture.empty() || !trail.texId.empty()) continue;

        std::string normTex = TacoParser::NormalisePath(trail.attribs.texture);
        std::string absPath = pack.ResolveFile(normTex);
        if (absPath.empty()) continue;

        std::string texId = "PATHING_" + pack.name + "_" + normTex;
        std::replace(texId.begin(), texId.end(), '/', '_');
        std::replace(texId.begin(), texId.end(), '\\', '_');
        std::replace(texId.begin(), texId.end(), '.', '_');
        std::replace(texId.begin(), texId.end(), ' ', '_');

        trail.texId = texId;
        g_PendingTextures.push_back({texId, absPath});
    }
}

// Derive a friendly pack name from the file path.
static std::string PackNameFromPath(const std::string& path)
{
    std::string name = path;
    auto sep = name.rfind('\\');
    if (sep == std::string::npos) sep = name.rfind('/');
    if (sep != std::string::npos) name = name.substr(sep + 1);
    auto dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

// Find all .taco files in a directory (non-recursive).
static std::vector<std::string> FindTacoFiles(const std::string& dir)
{
    std::vector<std::string> files;
    std::string pattern = dir + "\\*.taco";

    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return files;

    do {
        files.push_back(dir + "\\" + fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return files;
}

// ─────────────────────────────────────────────────────────────────────────────
// Category state persistence
// ─────────────────────────────────────────────────────────────────────────────

static std::string CategoryStatePath()
{
    std::string dir = AddonDataDirStatic();
    if (dir.empty()) return "";
    return dir + "\\category_state.json";
}

static void CollectCategoryState(const std::vector<MarkerCategory>& cats,
                                 const std::string& prefix, json& j)
{
    for (const auto& cat : cats)
    {
        std::string key = prefix.empty() ? cat.name : prefix + "." + cat.name;
        j[key] = cat.enabled;
        CollectCategoryState(cat.children, key, j);
    }
}

static void ApplyCategoryState(std::vector<MarkerCategory>& cats,
                               const std::string& prefix, const json& j)
{
    for (auto& cat : cats)
    {
        std::string key = prefix.empty() ? cat.name : prefix + "." + cat.name;
        auto it = j.find(key);
        if (it != j.end() && it->is_boolean())
            cat.enabled = it->get<bool>();
        ApplyCategoryState(cat.children, key, j);
    }
}

void PackManager::SaveCategoryState()
{
    std::string path = CategoryStatePath();
    if (path.empty()) return;

    json state;
    std::lock_guard<std::mutex> lock(g_PacksMutex);
    for (auto& pack : g_Packs)
    {
        json packState;
        packState["_enabled"] = pack.enabled;
        CollectCategoryState(pack.categories, "", packState["categories"]);
        state[pack.name] = packState;
    }
    std::ofstream(path) << state.dump(2);
}

void PackManager::LoadCategoryState()
{
    std::string path = CategoryStatePath();
    if (path.empty()) return;

    std::ifstream f(path);
    if (!f.is_open()) return;

    json state;
    try { state = json::parse(f); }
    catch (...) { return; }

    std::lock_guard<std::mutex> lock(g_PacksMutex);
    for (auto& pack : g_Packs)
    {
        auto it = state.find(pack.name);
        if (it == state.end()) continue;
        const json& ps = *it;
        if (ps.contains("_enabled") && ps["_enabled"].is_boolean())
            pack.enabled = ps["_enabled"].get<bool>();
        if (ps.contains("categories"))
            ApplyCategoryState(pack.categories, "", ps["categories"]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Background loading
// ─────────────────────────────────────────────────────────────────────────────

static void LoadThread()
{
    std::string packsDir = PacksDirStatic();
    if (packsDir.empty()) { g_Loading = false; return; }

    auto files = FindTacoFiles(packsDir);
    std::vector<TacoPack> loaded;
    int totalPois = 0, totalTrails = 0;

    for (const auto& tacoFile : files)
    {
        TacoPack pack;
        pack.filePath = tacoFile;
        pack.name     = PackNameFromPath(tacoFile);

        std::string extractDir = ExtractDirForPack(tacoFile);
        if (!ExtractTacoPack(tacoFile, extractDir, pack))
        {
            if (APIDefs)
                APIDefs->Log(LOGL_WARNING, "Pathing",
                    ("Failed to extract pack: " + pack.name).c_str());
            continue;
        }

        ParseExtractedXmls(pack);
        QueuePackTextures(pack);  // actual registration happens on render thread

        totalPois   += (int)pack.pois.size();
        totalTrails += (int)pack.trails.size();

        loaded.push_back(std::move(pack));

        if (APIDefs)
            APIDefs->Log(LOGL_INFO, "Pathing",
                ("Loaded pack: " + pack.name).c_str());
    }

    {
        std::lock_guard<std::mutex> lock(g_PacksMutex);
        g_Packs = std::move(loaded);
    }
    g_TotalPois   = totalPois;
    g_TotalTrails = totalTrails;

    // Restore enabled/disabled state from disk after packs are loaded
    PackManager::LoadCategoryState();

    g_Loading = false;

    if (APIDefs)
    {
        std::string msg = "All packs loaded. POIs: " + std::to_string(totalPois) +
                         "  Trails: " + std::to_string(totalTrails);
        APIDefs->Log(LOGL_INFO, "Pathing", msg.c_str());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void PackManager::Init()
{
    // Make sure the packs directory exists so users know where to drop files
    PacksDirStatic();

    g_Loading = true;
    g_LoadThread = std::thread(LoadThread);
}

void PackManager::Shutdown()
{
    if (g_LoadThread.joinable())
        g_LoadThread.join();
    SaveCategoryState();
}

void PackManager::Reload()
{
    if (g_Loading) return;

    if (g_LoadThread.joinable())
        g_LoadThread.join();

    g_Loading = true;
    g_LoadThread = std::thread(LoadThread);
}

const std::vector<TacoPack>& PackManager::GetPacks()
{
    return g_Packs;
}

std::vector<TacoPack>& PackManager::GetPacksMutable()
{
    return g_Packs;
}

std::vector<const Poi*> PackManager::GetPoisForMap(uint32_t mapId)
{
    std::vector<const Poi*> result;
    std::lock_guard<std::mutex> lock(g_PacksMutex);
    for (const auto& pack : g_Packs)
    {
        if (!pack.enabled) continue;
        for (const auto& poi : pack.pois)
        {
            if (poi.mapId == mapId && pack.IsCategoryEnabled(poi.type))
                result.push_back(&poi);
        }
    }
    return result;
}

std::vector<const Trail*> PackManager::GetTrailsForMap(uint32_t mapId)
{
    std::vector<const Trail*> result;
    std::lock_guard<std::mutex> lock(g_PacksMutex);
    for (const auto& pack : g_Packs)
    {
        if (!pack.enabled) continue;
        for (const auto& trail : pack.trails)
        {
            if (trail.mapId == mapId && pack.IsCategoryEnabled(trail.type))
                result.push_back(&trail);
        }
    }
    return result;
}

bool PackManager::IsLoading()   { return g_Loading.load(); }
int  PackManager::LoadedPackCount() { return (int)g_Packs.size(); }
int  PackManager::TotalPoiCount()   { return g_TotalPois.load(); }
int  PackManager::TotalTrailCount() { return g_TotalTrails.load(); }

std::string PackManager::AddonDataDir() { return AddonDataDirStatic(); }
std::string PackManager::PacksDir()     { return PacksDirStatic(); }

void PackManager::FlushPendingTextures()
{
    if (!APIDefs) return;

    std::vector<PendingTex> batch;
    {
        std::lock_guard<std::mutex> lock(g_PendingTexMutex);
        if (g_PendingTextures.empty()) return;
        batch.swap(g_PendingTextures);
    }

    // Deduplicate — many POIs share the same icon texture
    std::sort(batch.begin(), batch.end(),
        [](const PendingTex& a, const PendingTex& b){ return a.texId < b.texId; });
    auto last = std::unique(batch.begin(), batch.end(),
        [](const PendingTex& a, const PendingTex& b){ return a.texId == b.texId; });
    batch.erase(last, batch.end());

    for (const auto& pt : batch)
    {
        if (!APIDefs->Textures_Get(pt.texId.c_str()))
            APIDefs->Textures_LoadFromFile(pt.texId.c_str(), pt.absPath.c_str(), nullptr);
    }
}
