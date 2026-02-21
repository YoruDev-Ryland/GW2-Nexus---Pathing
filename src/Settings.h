#pragma once
#include <string>

// ── Persistent settings for the Pathing addon ─────────────────────────────────
// Serialised to <addondir>/settings.json via nlohmann/json.
struct Settings
{
    // ── Visibility ────────────────────────────────────────────────────────────
    bool ShowWindow       = false;  // main pack-manager / category window
    bool ShowOnMap        = false;  // show minimap overlay (future)
    bool RenderMarkers    = true;   // render POI markers in world space
    bool RenderTrails     = true;   // render trail breadcrumbs in world space

    // ── Rendering ─────────────────────────────────────────────────────────────
    float MarkerOpacity   = 1.0f;   // global opacity multiplier for markers
    float TrailOpacity    = 0.8f;   // global opacity multiplier for trails
    float MarkerScale     = 1.0f;   // global size multiplier
    float MaxRenderDist   = 5000.f; // world-units — don't draw beyond this
    float FadeStartDist   = 3000.f; // begin fading at this distance
    float MinScreenSize   = 8.f;    // px — don't render icons smaller than this
    float MaxScreenSize   = 64.f;   // px — clamp icon screen size to this
    bool  ShowDebugInfo   = false;  // overlay debug text (fps, marker counts)

    // ── Behaviour ─────────────────────────────────────────────────────────────
    bool  AutoHideInCombat = false; // future: hide when in combat
    bool  AutoHideOnMount  = false; // future: hide when mounted

    void Load();
    void Save() const;
};

extern Settings g_Settings;
