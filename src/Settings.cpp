#include "Settings.h"
#include "Shared.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

using json = nlohmann::json;

Settings g_Settings;

static std::string SettingsPath()
{
    if (!APIDefs || !APIDefs->Paths_GetAddonDirectory)
        return "";
    std::string dir = APIDefs->Paths_GetAddonDirectory("Pathing");
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir + "\\settings.json";
}

void Settings::Load()
{
    std::string path = SettingsPath();
    if (path.empty()) return;

    std::ifstream f(path);
    if (!f.is_open()) return;

    try
    {
        json j = json::parse(f);
        ShowWindow       = j.value("ShowWindow",        ShowWindow);
        ShowOnMap        = j.value("ShowOnMap",          ShowOnMap);
        RenderMarkers    = j.value("RenderMarkers",      RenderMarkers);
        RenderTrails     = j.value("RenderTrails",       RenderTrails);
        MarkerOpacity    = j.value("MarkerOpacity",      MarkerOpacity);
        TrailOpacity     = j.value("TrailOpacity",       TrailOpacity);
        MarkerScale      = j.value("MarkerScale",        MarkerScale);
        MaxRenderDist    = j.value("MaxRenderDist",      MaxRenderDist);
        FadeStartDist    = j.value("FadeStartDist",      FadeStartDist);
        MinScreenSize    = j.value("MinScreenSize",      MinScreenSize);
        MaxScreenSize    = j.value("MaxScreenSize",      MaxScreenSize);
        ShowDebugInfo    = j.value("ShowDebugInfo",      ShowDebugInfo);
        AutoHideInCombat = j.value("AutoHideInCombat",   AutoHideInCombat);
        AutoHideOnMount  = j.value("AutoHideOnMount",    AutoHideOnMount);
    }
    catch (...) { /* malformed JSON — keep defaults */ }
}

void Settings::Save() const
{
    std::string path = SettingsPath();
    if (path.empty()) return;

    json j;
    j["ShowWindow"]       = ShowWindow;
    j["ShowOnMap"]        = ShowOnMap;
    j["RenderMarkers"]    = RenderMarkers;
    j["RenderTrails"]     = RenderTrails;
    j["MarkerOpacity"]    = MarkerOpacity;
    j["TrailOpacity"]     = TrailOpacity;
    j["MarkerScale"]      = MarkerScale;
    j["MaxRenderDist"]    = MaxRenderDist;
    j["FadeStartDist"]    = FadeStartDist;
    j["MinScreenSize"]    = MinScreenSize;
    j["MaxScreenSize"]    = MaxScreenSize;
    j["ShowDebugInfo"]    = ShowDebugInfo;
    j["AutoHideInCombat"] = AutoHideInCombat;
    j["AutoHideOnMount"]  = AutoHideOnMount;

    std::ofstream(path) << j.dump(4);
}
