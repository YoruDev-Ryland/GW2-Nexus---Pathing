#pragma once
#include <windows.h>
#include <cstdint>
#include "Nexus.h"

// ── Mumble Link structs (standard GW2 memory layout) ─────────────────────────
// These match the GW2 wiki specification and what Nexus shares at DL_MUMBLE_LINK.
namespace Mumble
{
    struct Vector3 { float X, Y, Z; };

    struct Context
    {
        uint8_t  ServerAddress[28]; // sockaddr_in or sockaddr_in6
        uint32_t MapId;
        uint32_t MapType;
        uint32_t ShardId;
        uint32_t Instance;
        uint32_t BuildId;
        uint32_t UIState;           // bitfield: IsMapOpen, IsCompassTopRight, ...
        uint16_t CompassWidth;
        uint16_t CompassHeight;
        float    CompassRotation;
        float    PlayerX;
        float    PlayerY;
        float    MapCenterX;
        float    MapCenterY;
        float    MapScale;
        uint32_t ProcessId;
        uint8_t  MountIndex;
    };

    struct LinkedMem
    {
        uint32_t UIVersion;
        uint32_t UITick;
        Vector3  AvatarPosition;
        Vector3  AvatarFront;
        Vector3  AvatarTop;
        wchar_t  Name[256];         // L"Guild Wars 2" when in-game
        Vector3  CameraPosition;
        Vector3  CameraFront;
        Vector3  CameraTop;
        wchar_t  Identity[256];     // JSON: character name, map id, etc.
        uint32_t ContextLen;
        union {
            Context Context;
            uint8_t ContextRaw[256];
        };
        wchar_t  Description[2048];
    };

    // Parsed from LinkedMem::Identity JSON by Nexus
    struct Identity
    {
        char     Name[20];
        uint32_t Profession;
        uint32_t Spec;
        uint32_t Race;
        uint32_t MapID;
        uint32_t WorldID;
        uint32_t TeamColorID;
        bool     IsCommander;
        float    FOV;
        uint32_t UISize;
    };
}

// ── Global addon state shared across all translation units ────────────────────
extern AddonAPI_t*        APIDefs;
extern HMODULE            Self;
extern Mumble::LinkedMem* MumbleLink;
extern Mumble::Identity*  MumbleIdent;

// ── Convenience helpers ───────────────────────────────────────────────────────

// Returns the raw D3D11 shader-resource-view pointer for a registered texture,
// or nullptr if the texture isn't loaded yet.  Always null-check the result.
inline void* GetTexResource(const char* id)
{
    if (!APIDefs || !id || !*id) return nullptr;
    Texture_t* t = APIDefs->Textures_Get(id);
    return (t && t->Resource) ? t->Resource : nullptr;
}

// Returns true when the player is in a playable map (MapId != 0).
inline bool IsInGame()
{
    return MumbleLink && MumbleLink->Context.MapId != 0;
}

// Current map ID, or 0 if not in game.
inline uint32_t CurrentMapId()
{
    return MumbleLink ? MumbleLink->Context.MapId : 0u;
}
