#include "Shared.h"
#include "Settings.h"
#include "PackManager.h"
#include "MarkerRenderer.h"
#include "UI.h"

#include <imgui.h>
#include <windows.h>
#include <cstring>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason, LPVOID /*reserved*/)
{
    switch (ul_reason)
    {
        case DLL_PROCESS_ATTACH:
            Self = hModule;
            DisableThreadLibraryCalls(hModule);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}

static void ProcessKeybind(const char* aIdentifier, bool aIsRelease)
{
    if (aIsRelease) return;

    if (strcmp(aIdentifier, "KB_PATHING_TOGGLEWIN") == 0)
    {
        g_Settings.ShowWindow = !g_Settings.ShowWindow;
        g_Settings.Save();
    }
    else if (strcmp(aIdentifier, "KB_PATHING_TOGGLEMARKERS") == 0)
    {
        g_Settings.RenderMarkers = !g_Settings.RenderMarkers;
        g_Settings.Save();
    }
    else if (strcmp(aIdentifier, "KB_PATHING_TOGGLETRAILS") == 0)
    {
        g_Settings.RenderTrails = !g_Settings.RenderTrails;
        g_Settings.Save();
    }
}

static void Render()
{
    MarkerRenderer::Render();

    UI::RenderWindow();
}

static void RenderOptions()
{
    UI::RenderOptions();
}

static void RenderQAContextMenu()
{
    if (ImGui::MenuItem("Show Markers", nullptr, g_Settings.RenderMarkers))
    {
        g_Settings.RenderMarkers = !g_Settings.RenderMarkers;
        g_Settings.Save();
    }
    if (ImGui::MenuItem("Show Trails", nullptr, g_Settings.RenderTrails))
    {
        g_Settings.RenderTrails = !g_Settings.RenderTrails;
        g_Settings.Save();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Show All"))
    {
        g_Settings.RenderMarkers = true;
        g_Settings.RenderTrails  = true;
        g_Settings.Save();
    }
    if (ImGui::MenuItem("Hide All"))
    {
        g_Settings.RenderMarkers = false;
        g_Settings.RenderTrails  = false;
        g_Settings.Save();
    }
}

static void AddonLoad(AddonAPI_t* aApi)
{
    APIDefs = aApi;

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(aApi->ImguiContext));
    ImGui::SetAllocatorFunctions(
        reinterpret_cast<void*(*)(size_t, void*)>(aApi->ImguiMalloc),
        reinterpret_cast<void(*)(void*, void*)>(aApi->ImguiFree));

    MumbleLink  = static_cast<Mumble::LinkedMem*>(aApi->DataLink_Get(DL_MUMBLE_LINK));
    MumbleIdent = static_cast<Mumble::Identity*>(aApi->DataLink_Get(DL_MUMBLE_LINK_IDENTITY));

    g_Settings.Load();

    aApi->GUI_Register(RT_Render,        Render);
    aApi->GUI_Register(RT_OptionsRender, RenderOptions);

    aApi->InputBinds_RegisterWithString("KB_PATHING_TOGGLEWIN",     ProcessKeybind, "(null)");
    aApi->InputBinds_RegisterWithString("KB_PATHING_TOGGLEMARKERS", ProcessKeybind, "(null)");
    aApi->InputBinds_RegisterWithString("KB_PATHING_TOGGLETRAILS",  ProcessKeybind, "(null)");

    aApi->Textures_GetOrCreateFromResource("ICON_PATHING",       104, Self);
    aApi->Textures_GetOrCreateFromResource("ICON_PATHING_HOVER", 104, Self);
    aApi->QuickAccess_Add("QA_PATHING",
                          "ICON_PATHING",
                          "ICON_PATHING_HOVER",
                          "KB_PATHING_TOGGLEWIN",
                          "Pathing");
    aApi->QuickAccess_AddContextMenu("QA_PATHING_CTX", "QA_PATHING", RenderQAContextMenu);

    PackManager::Init();

    aApi->Log(LOGL_INFO, "Pathing", "Loaded.");
}

static void AddonUnload()
{
    if (!APIDefs) return;

    g_Settings.Save();
    PackManager::Shutdown();

    APIDefs->GUI_Deregister(Render);
    APIDefs->GUI_Deregister(RenderOptions);

    APIDefs->InputBinds_Deregister("KB_PATHING_TOGGLEWIN");
    APIDefs->InputBinds_Deregister("KB_PATHING_TOGGLEMARKERS");
    APIDefs->InputBinds_Deregister("KB_PATHING_TOGGLETRAILS");

    APIDefs->QuickAccess_Remove("QA_PATHING");
    APIDefs->QuickAccess_RemoveContextMenu("QA_PATHING_CTX");

    APIDefs    = nullptr;
    MumbleLink = nullptr;
    MumbleIdent= nullptr;
}
static AddonDefinition_t s_AddonDef{};

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    s_AddonDef.Signature   = 0x50415448;
    s_AddonDef.APIVersion  = NEXUS_API_VERSION;
    s_AddonDef.Name        = "Pathing";
    s_AddonDef.Version     = { 1, 0, 0, 2 };
    s_AddonDef.Author      = "YoruDev-Ryland";
    s_AddonDef.Description = "TacO / BlishHUD compatible pathing pack renderer for Nexus.";
    s_AddonDef.Load        = AddonLoad;
    s_AddonDef.Unload      = AddonUnload;
    s_AddonDef.Flags       = AF_None;
    s_AddonDef.Provider    = UP_GitHub;
    s_AddonDef.UpdateLink  = "https://github.com/YoruDev-Ryland/GW2-Nexus---Pathing";
    return &s_AddonDef;
}
