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

static constexpr float  kNearClip      = 0.5f;    // world units
static constexpr float  kFarClip       = 8000.f;  // world units (well beyond max render dist)
static constexpr float  kDefaultIconSz = 32.f;    // screen pixels for iconSize=1.0
static constexpr float  kDefaultFOV    = 1.222f;  // ~70° fallback if MumbleIdent unavailable
static constexpr ImU32  kDefaultColor  = 0xFFFFFFFF;

static Mat4 BuildViewProj(float screenW, float screenH)
{
    Vec3 camPos{ MumbleLink->CameraPosition.X,
                 MumbleLink->CameraPosition.Y,
                 MumbleLink->CameraPosition.Z };
    Vec3 f     { MumbleLink->CameraFront.X,
                 MumbleLink->CameraFront.Y,
                 MumbleLink->CameraFront.Z };
    Vec3 topHint{ MumbleLink->CameraTop.X,
                  MumbleLink->CameraTop.Y,
                  MumbleLink->CameraTop.Z };

    f = f.Normalised();
    Vec3 worldUp = (topHint.LengthSq() > 0.01f) ? topHint.Normalised()
                                                  : Vec3{0.f, 1.f, 0.f};

    Vec3 r = worldUp.Cross(f).Normalised();
    Vec3 u = f.Cross(r).Normalised();
    Mat4 view{};
    view.m[0][0] = r.x; view.m[1][0] = r.y; view.m[2][0] = r.z; view.m[3][0] = -r.Dot(camPos);
    view.m[0][1] = u.x; view.m[1][1] = u.y; view.m[2][1] = u.z; view.m[3][1] = -u.Dot(camPos);
    view.m[0][2] = f.x; view.m[1][2] = f.y; view.m[2][2] = f.z; view.m[3][2] = -f.Dot(camPos);
    view.m[3][3] = 1.f;

    float fov    = (MumbleIdent && MumbleIdent->FOV > 0.01f)
                   ? MumbleIdent->FOV : kDefaultFOV;
    float aspect = (screenH > 0.f) ? screenW / screenH : 1.7778f;
    float tanHalfFov = std::tan(fov * 0.5f);

    Mat4 proj{};
    proj.m[0][0] = 1.f / (aspect * tanHalfFov);
    proj.m[1][1] = 1.f / tanHalfFov;
    proj.m[2][2] = kFarClip / (kFarClip - kNearClip);
    proj.m[2][3] = 1.f;   // w_clip = z_view
    proj.m[3][2] = -(kNearClip * kFarClip) / (kFarClip - kNearClip);

    return proj * view;
}

static ImU32 ToImColor(uint32_t argb, float globalAlpha)
{
    uint8_t a = (uint8_t)((argb >> 24) & 0xFF);
    uint8_t r = (uint8_t)((argb >> 16) & 0xFF);
    uint8_t g = (uint8_t)((argb >>  8) & 0xFF);
    uint8_t b = (uint8_t)( argb        & 0xFF);
    a = (uint8_t)(a * globalAlpha);
    return IM_COL32(r, g, b, a);
}

static float FadeAlpha(float dist, float fadeNear, float fadeFar,
                       float globalFadeStart, float globalMaxDist)
{
    float distFar  = (fadeFar  >= 0.f) ? std::min(fadeFar,  globalMaxDist) : globalMaxDist;
    float distNear = (fadeNear >= 0.f) ? std::min(fadeNear, distFar)       : std::min(globalFadeStart, distFar);
    if (dist >= distFar  || distFar <= 0.f) return 0.f;
    if (dist <= distNear || distFar <= distNear) return 1.f;
    return 1.f - (dist - distNear) / (distFar - distNear);
}

static void DrawMarkers(ImDrawList* dl, const Mat4& viewProj,
                        const Vec3& camPos,
                        float screenW, float screenH,
                        const std::vector<const Poi*>& pois)
{
    for (const Poi* poi : pois)
    {
        Vec3 worldPos{ poi->x,
                       poi->y + poi->attribs.heightOffset,
                       poi->z };

        float distSq = DistSq(camPos, worldPos);
        float dist   = std::sqrt(distSq);

        if (dist > g_Settings.MaxRenderDist) continue;

        float sx, sy, depth;
        if (!WorldToScreen(worldPos, viewProj, screenW, screenH, sx, sy, depth))
            continue;

        float fov = (MumbleIdent && MumbleIdent->FOV > 0.01f) ? MumbleIdent->FOV : kDefaultFOV;
        float pixelsPerUnit = (screenH * 0.5f) / (std::tan(fov * 0.5f) * std::max(dist, 0.1f));
        float halfSz = (kDefaultIconSz * poi->attribs.iconSize * g_Settings.MarkerScale
                        * pixelsPerUnit) * 0.02f;

        float minSz = (poi->attribs.minSize >= 0.f) ? poi->attribs.minSize : g_Settings.MinScreenSize;
        float maxSz = (poi->attribs.maxSize >= 0.f) ? poi->attribs.maxSize : g_Settings.MaxScreenSize;
        halfSz = std::clamp(halfSz, minSz * 0.5f, maxSz * 0.5f);

        float fadeAlpha = FadeAlpha(dist,
                                    poi->attribs.fadeNear,
                                    poi->attribs.fadeFar,
                                    g_Settings.FadeStartDist,
                                    g_Settings.MaxRenderDist);
        float alpha = poi->attribs.alpha * g_Settings.MarkerOpacity * fadeAlpha;

        if (alpha < 0.01f || halfSz < 1.f) continue;

        ImVec2 p0{ sx - halfSz, sy - halfSz };
        ImVec2 p1{ sx + halfSz, sy + halfSz };

        void* texRes = !poi->texId.empty() ? GetTexResource(poi->texId.c_str()) : nullptr;
        if (texRes)
        {
            ImU32 tint = IM_COL32(255, 255, 255, (uint8_t)(alpha * 255.f));
            dl->AddImage((ImTextureID)(void*)texRes, p0, p1,
                         ImVec2(0, 0), ImVec2(1, 1), tint);
        }
        else
        {
            ImU32 fillCol   = ToImColor(poi->attribs.color, alpha);
            ImU32 borderCol = IM_COL32(255, 255, 255, (uint8_t)(alpha * 200.f));
            dl->AddCircleFilled(ImVec2(sx, sy), halfSz, fillCol, 16);
            dl->AddCircle(      ImVec2(sx, sy), halfSz, borderCol, 16, 1.5f);
        }
    }
}

static void DrawTrails(ImDrawList* dl, const Mat4& viewProj,
                       const Vec3& camPos,
                       float screenW, float screenH,
                       const std::vector<const Trail*>& trails)
{
    float fov = (MumbleIdent && MumbleIdent->FOV > 0.01f) ? MumbleIdent->FOV : kDefaultFOV;

    for (const Trail* trail : trails)
    {
        if (trail->points.empty()) continue;

        float trailAlpha = trail->attribs.alpha * g_Settings.TrailOpacity;
        if (trailAlpha < 0.01f) continue;

        void* texRes = !trail->texId.empty() ? GetTexResource(trail->texId.c_str()) : nullptr;
        // tileSize in world units: one UV tile = one trail-diameter wide.
        // Computed here so it's consistent between prevIdx and curIdx lookups.
        float tileSize = g_Settings.TrailWidth * trail->attribs.trailScale * 2.f;
        if (tileSize < 0.001f) tileSize = 0.001f;

        ImVec2 prevScreen{};
        float  prevHalfW = 0.f;
        float  prevA     = 1.f;
        bool   hasPrev   = false;
        size_t prevIdx   = 0;

        for (size_t ptIdx = 0; ptIdx < trail->points.size(); ++ptIdx)
        {
            const TrailPoint& tp = trail->points[ptIdx];
            Vec3 worldPos{ tp.x, tp.y, tp.z };
            float dist = std::sqrt(DistSq(camPos, worldPos));

            if (dist > g_Settings.MaxRenderDist) { hasPrev = false; continue; }

            float sx, sy, depth;
            if (!WorldToScreen(worldPos, viewProj, screenW, screenH, sx, sy, depth))
            {
                hasPrev = false;
                continue;
            }

            float halfW;
            if (g_Settings.TrailPerspectiveScale)
            {
                float ppu = (screenH * 0.5f) / (std::tan(fov * 0.5f) * std::max(dist, 0.1f));
                halfW = g_Settings.TrailWidth * trail->attribs.trailScale * ppu;
            }
            else
            {
                halfW = g_Settings.TrailWidth * trail->attribs.trailScale * 3.f;
            }
            halfW = std::max(halfW, 1.f);

            float fadeA = FadeAlpha(dist,
                                    trail->attribs.fadeNear,
                                    trail->attribs.fadeFar,
                                    g_Settings.FadeStartDist,
                                    g_Settings.MaxRenderDist);
            float pointA = trailAlpha * fadeA;

            ImVec2 cur{ sx, sy };

            if (hasPrev && pointA > 0.01f)
            {
                float dx = cur.x - prevScreen.x;
                float dy = cur.y - prevScreen.y;
                float len = std::sqrt(dx * dx + dy * dy);

                if (len > 0.5f && len < screenW * 0.5f)
                {
                    float invLen = 1.f / len;
                    float px = -dy * invLen;
                    float py =  dx * invLen;

                    ImVec2 p1{ prevScreen.x + px * prevHalfW, prevScreen.y + py * prevHalfW };
                    ImVec2 p2{ cur.x        + px * halfW,     cur.y        + py * halfW     };
                    ImVec2 p3{ cur.x        - px * halfW,     cur.y        - py * halfW     };
                    ImVec2 p4{ prevScreen.x - px * prevHalfW, prevScreen.y - py * prevHalfW };

                    float avgA = (prevA + pointA) * 0.5f;

                    // UV V-coords come directly from the precomputed arc length
                    // table.  These are anchored to world positions and are
                    // completely independent of camera, culling, or frame order.
                    float uvV     = trail->arcLengths[prevIdx] / tileSize;
                    float uvVNext = trail->arcLengths[ptIdx]   / tileSize;

                    if (texRes)
                    {
                        ImU32 tint = IM_COL32(255, 255, 255, (uint8_t)(avgA * 255.f));
                        dl->AddImageQuad(
                            (ImTextureID)(void*)texRes,
                            p1, p2, p3, p4,
                            ImVec2(0.f, uvV),     ImVec2(0.f, uvVNext),
                            ImVec2(1.f, uvVNext), ImVec2(1.f, uvV),
                            tint);
                    }
                    else
                    {
                        ImU32 col = ToImColor(trail->attribs.trailColor, avgA);
                        dl->AddQuadFilled(p1, p2, p3, p4, col);
                    }
                }
            }

            prevScreen = cur;
            prevIdx    = ptIdx;
            prevHalfW  = halfW;
            prevA      = pointA;
            hasPrev    = (pointA > 0.01f);
        }
    }
}

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

void MarkerRenderer::Render()
{
    PackManager::FlushPendingTextures();

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

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    auto pois   = g_Settings.RenderMarkers ? PackManager::GetPoisForMap(mapId)   : std::vector<const Poi*>{};
    auto trails = g_Settings.RenderTrails  ? PackManager::GetTrailsForMap(mapId) : std::vector<const Trail*>{};

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
