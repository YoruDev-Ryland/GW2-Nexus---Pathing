#include "MarkerRenderer.h"
#include "Shared.h"
#include "Settings.h"
#include "PackManager.h"
#include "MathUtils.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>

using namespace Math;

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

static constexpr float  kNearClip      = 0.5f;    // world units
static constexpr float  kFarClip       = 8000.f;  // world units (well beyond max render dist)
static constexpr float  kDefaultIconSz = 32.f;    // screen pixels for iconSize=1.0
static constexpr float  kDefaultFOV    = 1.222f;  // ~70° fallback if MumbleIdent unavailable
static constexpr ImU32  kDefaultColor  = 0xFFFFFFFF;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Build the combined view-projection matrix from current MumbleLink camera data.
static Mat4 BuildViewProj(float screenW, float screenH)
{
    // Camera vectors from MumbleLink
    Vec3 camPos  { MumbleLink->CameraPosition.X,
                   MumbleLink->CameraPosition.Y,
                   MumbleLink->CameraPosition.Z };
    Vec3 camFront{ MumbleLink->CameraFront.X,
                   MumbleLink->CameraFront.Y,
                   MumbleLink->CameraFront.Z };
    Vec3 camTop  { MumbleLink->CameraTop.X,
                   MumbleLink->CameraTop.Y,
                   MumbleLink->CameraTop.Z };

    // GW2 world up is Y+; if CameraTop is degenerate fall back to it
    Vec3 worldUp = (camTop.LengthSq() > 0.01f) ? camTop : Vec3{0.f, 1.f, 0.f};

    float fov    = (MumbleIdent && MumbleIdent->FOV > 0.01f)
                   ? MumbleIdent->FOV : kDefaultFOV;
    float aspect = (screenH > 0.f) ? screenW / screenH : 1.7778f;

    Mat4 view = LookAt(camPos, camFront, worldUp);
    Mat4 proj = Perspective(fov, aspect, kNearClip, kFarClip);
    return proj * view;
}

// Convert a TacO ARGB uint32 + global alpha to an ImU32 (ABGR for ImGui).
static ImU32 ToImColor(uint32_t argb, float globalAlpha)
{
    uint8_t a = (uint8_t)((argb >> 24) & 0xFF);
    uint8_t r = (uint8_t)((argb >> 16) & 0xFF);
    uint8_t g = (uint8_t)((argb >>  8) & 0xFF);
    uint8_t b = (uint8_t)( argb        & 0xFF);
    a = (uint8_t)(a * globalAlpha);
    return IM_COL32(r, g, b, a);
}

// Distance-based alpha fade.
static float FadeAlpha(float dist, float fadeNear, float fadeFar,
                       float globalFadeStart, float globalMaxDist)
{
    // NOTE: 'near' and 'far' are Windows macros — use different names.
    float distNear = (fadeNear >= 0.f) ? fadeNear : globalFadeStart;
    float distFar  = (fadeFar  >= 0.f) ? fadeFar  : globalMaxDist;
    if (dist >= distFar)  return 0.f;
    if (dist <= distNear) return 1.f;
    return 1.f - (dist - distNear) / (distFar - distNear);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw POI markers
// ─────────────────────────────────────────────────────────────────────────────

static void DrawMarkers(ImDrawList* dl, const Mat4& viewProj,
                        const Vec3& camPos,
                        float screenW, float screenH,
                        const std::vector<const Poi*>& pois)
{
    for (const Poi* poi : pois)
    {
        // World position — apply heightOffset on Y axis
        Vec3 worldPos{ poi->x,
                       poi->y + poi->attribs.heightOffset,
                       poi->z };

        float distSq = DistSq(camPos, worldPos);
        float dist   = std::sqrt(distSq);

        float maxDist = (poi->attribs.fadeFar >= 0.f) ? poi->attribs.fadeFar
                                                       : g_Settings.MaxRenderDist;
        if (dist > maxDist) continue;

        float sx, sy, depth;
        if (!WorldToScreen(worldPos, viewProj, screenW, screenH, sx, sy, depth))
            continue;

        // Compute screen size
        // Project a point 1 world unit to the right at the same depth to get pixel scale
        // Simple approximation: scale = (screenH / 2) / (tan(fov/2) * dist)
        float fov = (MumbleIdent && MumbleIdent->FOV > 0.01f) ? MumbleIdent->FOV : kDefaultFOV;
        float pixelsPerUnit = (screenH * 0.5f) / (std::tan(fov * 0.5f) * std::max(dist, 0.1f));
        float halfSz = (kDefaultIconSz * poi->attribs.iconSize * g_Settings.MarkerScale
                        * pixelsPerUnit) * 0.02f;  // 0.02 = tuning factor

        // Clamp screen size
        float minSz = (poi->attribs.minSize >= 0.f) ? poi->attribs.minSize : g_Settings.MinScreenSize;
        float maxSz = (poi->attribs.maxSize >= 0.f) ? poi->attribs.maxSize : g_Settings.MaxScreenSize;
        halfSz = std::clamp(halfSz, minSz * 0.5f, maxSz * 0.5f);

        // Distance fade
        float fadeAlpha = FadeAlpha(dist,
                                    poi->attribs.fadeNear,
                                    poi->attribs.fadeFar,
                                    g_Settings.FadeStartDist,
                                    g_Settings.MaxRenderDist);
        float alpha = poi->attribs.alpha * g_Settings.MarkerOpacity * fadeAlpha;

        if (alpha < 0.01f || halfSz < 1.f) continue;

        ImVec2 p0{ sx - halfSz, sy - halfSz };
        ImVec2 p1{ sx + halfSz, sy + halfSz };

        // Try to draw the icon texture; fall back to a coloured circle
        void* texRes = !poi->texId.empty() ? GetTexResource(poi->texId.c_str()) : nullptr;
        if (texRes)
        {
            ImU32 tint = IM_COL32(255, 255, 255, (uint8_t)(alpha * 255.f));
            dl->AddImage((ImTextureID)(void*)texRes, p0, p1,
                         ImVec2(0, 0), ImVec2(1, 1), tint);
        }
        else
        {
            // Fallback: coloured circle with a white border
            ImU32 fillCol   = ToImColor(poi->attribs.color, alpha);
            ImU32 borderCol = IM_COL32(255, 255, 255, (uint8_t)(alpha * 200.f));
            dl->AddCircleFilled(ImVec2(sx, sy), halfSz, fillCol, 16);
            dl->AddCircle(      ImVec2(sx, sy), halfSz, borderCol, 16, 1.5f);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw trail breadcrumbs
// ─────────────────────────────────────────────────────────────────────────────

static void DrawTrails(ImDrawList* dl, const Mat4& viewProj,
                       const Vec3& camPos,
                       float screenW, float screenH,
                       const std::vector<const Trail*>& trails)
{
    for (const Trail* trail : trails)
    {
        if (trail->points.empty()) continue;

        float trailAlpha = trail->attribs.alpha * g_Settings.TrailOpacity;
        if (trailAlpha < 0.01f) continue;

        ImU32 lineCol = ToImColor(trail->attribs.trailColor, trailAlpha);

        // Draw line segments between sequential projected trail points.
        // Also draw dots at each point so they show up even when the trail
        // segments are very far apart.
        ImVec2 prevScreen{};
        bool   hasPrev = false;

        for (const TrailPoint& tp : trail->points)
        {
            Vec3 worldPos{ tp.x, tp.y, tp.z };

            float dist = std::sqrt(DistSq(camPos, worldPos));
            float maxDist = (trail->attribs.fadeFar >= 0.f)
                            ? trail->attribs.fadeFar : g_Settings.MaxRenderDist;
            if (dist > maxDist) { hasPrev = false; continue; }

            float sx, sy, depth;
            if (!WorldToScreen(worldPos, viewProj, screenW, screenH, sx, sy, depth))
            {
                hasPrev = false;
                continue;
            }

            float fadeAlpha = FadeAlpha(dist,
                                        trail->attribs.fadeNear,
                                        trail->attribs.fadeFar,
                                        g_Settings.FadeStartDist,
                                        g_Settings.MaxRenderDist);
            float pointAlpha = trailAlpha * fadeAlpha;
            if (pointAlpha < 0.01f) { hasPrev = false; continue; }

            ImU32 col = ToImColor(trail->attribs.trailColor, pointAlpha);
            ImVec2 cur{ sx, sy };

            // Draw connecting line to previous point (if close enough in screen space)
            if (hasPrev)
            {
                float dx = cur.x - prevScreen.x, dy = cur.y - prevScreen.y;
                float screenDist = std::sqrt(dx*dx + dy*dy);
                // Don't draw lines spanning the whole screen (trail wrap-around)
                if (screenDist < screenW * 0.5f)
                {
                    float thickness = 3.f * trail->attribs.trailScale;
                    dl->AddLine(prevScreen, cur, col, thickness);
                }
            }

            // Dot at trail point
            float dotRadius = 4.f * trail->attribs.trailScale;
            dl->AddCircleFilled(cur, dotRadius, col, 8);

            prevScreen = cur;
            hasPrev    = true;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug overlay
// ─────────────────────────────────────────────────────────────────────────────

static void DrawDebugInfo(ImDrawList* dl,
                          float screenW, float screenH,
                          int nPois, int nTrails)
{
    (void)screenW; (void)screenH;

    char buf[128];
    snprintf(buf, sizeof(buf),
             "[Pathing] POIs: %d  Trails: %d  Packs: %d%s",
             nPois, nTrails,
             PackManager::LoadedPackCount(),
             PackManager::IsLoading() ? "  [loading...]" : "");

    ImVec2 pos{ 8.f, 8.f };
    dl->AddText(pos, IM_COL32(255, 220, 80, 200), buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public render entry point
// ─────────────────────────────────────────────────────────────────────────────

void MarkerRenderer::Render()
{
    // Drain any textures queued by the background loader (must happen on render thread)
    PackManager::FlushPendingTextures();

    // Nothing to draw if not in-game or rendering is disabled
    if (!IsInGame())   return;
    if (!MumbleLink)   return;
    if (!g_Settings.RenderMarkers && !g_Settings.RenderTrails) return;

    const ImGuiIO& io    = ImGui::GetIO();
    float screenW        = io.DisplaySize.x;
    float screenH        = io.DisplaySize.y;
    if (screenW < 1.f || screenH < 1.f) return;

    uint32_t mapId = CurrentMapId();
    Mat4     vp    = BuildViewProj(screenW, screenH);
    Vec3     cam   { MumbleLink->CameraPosition.X,
                     MumbleLink->CameraPosition.Y,
                     MumbleLink->CameraPosition.Z };

    // Use ImGui background draw list so markers appear under the game UI
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    auto pois   = g_Settings.RenderMarkers ? PackManager::GetPoisForMap(mapId)   : std::vector<const Poi*>{};
    auto trails = g_Settings.RenderTrails  ? PackManager::GetTrailsForMap(mapId) : std::vector<const Trail*>{};

    // Sort POIs back-to-front by distance (furthest drawn first so nearby
    // markers occlude distant ones naturally)
    std::sort(pois.begin(), pois.end(),
        [&cam](const Poi* a, const Poi* b)
        {
            float da = DistSq(cam, Vec3{a->x, a->y, a->z});
            float db = DistSq(cam, Vec3{b->x, b->y, b->z});
            return da > db;
        });

    if (!trails.empty())
        DrawTrails(dl, vp, cam, screenW, screenH, trails);

    if (!pois.empty())
        DrawMarkers(dl, vp, cam, screenW, screenH, pois);

    if (g_Settings.ShowDebugInfo)
        DrawDebugInfo(dl, screenW, screenH, (int)pois.size(), (int)trails.size());
}
