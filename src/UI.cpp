#include "UI.h"
#include "Shared.h"
#include "Settings.h"
#include "PackManager.h"
#include "TacoPack.h"

#include <imgui.h>
#include <string>
#include <algorithm>
#include <cctype>
#include <windows.h>   // ShellExecuteA

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Recursively draw a category tree with enable/disable checkboxes.
// Returns true if any state changed.
static bool DrawCategoryTree(std::vector<MarkerCategory>& cats,
                             bool parentEnabled,
                             int depth = 0)
{
    bool changed = false;
    for (auto& cat : cats)
    {
        bool nodeEnabled = parentEnabled && cat.enabled;

        // Build a unique imgui ID
        std::string label = cat.displayName + "##pcat_" + cat.name;

        // Indent for depth
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + depth * 12.f);

        // Checkbox
        bool nodeCb = cat.enabled;
        if (!parentEnabled)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
        if (ImGui::Checkbox(("##cb_" + cat.name + std::to_string(depth)).c_str(), &nodeCb))
        {
            cat.enabled = nodeCb;
            changed = true;
        }
        if (!parentEnabled) ImGui::PopStyleVar();

        ImGui::SameLine();

        bool hasChildren = !cat.children.empty();
        if (hasChildren)
        {
            // Draw as a tree node
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
            if (cat.expanded) flags |= ImGuiTreeNodeFlags_DefaultOpen;

            bool open = ImGui::TreeNodeEx(label.c_str(), flags);
            cat.expanded = open;
            if (open)
            {
                if (DrawCategoryTree(cat.children, nodeEnabled, depth + 1))
                    changed = true;
                ImGui::TreePop();
            }
        }
        else
        {
            // Leaf node — just show the label
            ImGui::TextUnformatted(cat.displayName.c_str());
        }
    }
    return changed;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pack manager window
// ─────────────────────────────────────────────────────────────────────────────

void UI::RenderWindow()
{
    if (!g_Settings.ShowWindow) return;

    ImGui::SetNextWindowSize(ImVec2(480.f, 580.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320.f, 300.f), ImVec2(900.f, 1200.f));

    if (!ImGui::Begin("Pathing##main_window", &g_Settings.ShowWindow,
                      ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    // ── Header bar ────────────────────────────────────────────────────────────
    if (PackManager::IsLoading())
    {
        ImGui::TextColored(ImVec4(1.f, 0.75f, 0.f, 1.f), "Loading packs...");
    }
    else
    {
        ImGui::TextColored(ImVec4(0.5f, 1.f, 0.5f, 1.f),
            "%d pack(s)  |  %d POIs  |  %d trails",
            PackManager::LoadedPackCount(),
            PackManager::TotalPoiCount(),
            PackManager::TotalTrailCount());
    }

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 130.f);
    if (ImGui::Button("Reload##packs_reload", ImVec2(60.f, 0)))
        PackManager::Reload();

    ImGui::SameLine();
    if (ImGui::Button("Open Dir##packs_dir", ImVec2(68.f, 0)))
    {
        std::string dir = PackManager::PacksDir();
        if (!dir.empty())
            ShellExecuteA(nullptr, "explore", dir.c_str(), nullptr, nullptr, SW_SHOW);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Open the packs folder — drop .taco files here");

    ImGui::Separator();

    // ── Quick render toggles ──────────────────────────────────────────────────
    bool mChanged = false;
    mChanged |= ImGui::Checkbox("Show Markers", &g_Settings.RenderMarkers);
    ImGui::SameLine(150.f);
    mChanged |= ImGui::Checkbox("Show Trails", &g_Settings.RenderTrails);
    if (mChanged) g_Settings.Save();

    ImGui::Separator();

    // ── Pack list with collapsible category trees ─────────────────────────────
    ImGui::BeginChild("##pack_list", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    auto& packs = PackManager::GetPacksMutable();

    if (packs.empty() && !PackManager::IsLoading())
    {
        ImGui::TextDisabled("No packs loaded.");
        ImGui::TextDisabled("Drop .taco files into the packs folder and click Reload.");
    }

    for (auto& pack : packs)
    {
        bool packChanged = false;

        // Pack header — checkbox + collapsing header
        bool packEnabled = pack.enabled;
        if (ImGui::Checkbox(("##packena_" + pack.name).c_str(), &packEnabled))
        {
            pack.enabled = packEnabled;
            packChanged  = true;
        }
        ImGui::SameLine();

        ImGuiTreeNodeFlags headerFlags =
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap;

        bool open = ImGui::CollapsingHeader(
            (pack.name + "  (" + std::to_string(pack.pois.size()) + " POIs, " +
             std::to_string(pack.trails.size()) + " trails)##ph_" + pack.name).c_str(),
            headerFlags);

        if (open && !pack.categories.empty())
        {
            ImGui::Indent();
            if (DrawCategoryTree(pack.categories, pack.enabled))
                packChanged = true;
            ImGui::Unindent();
        }

        if (packChanged)
            PackManager::SaveCategoryState();
    }

    ImGui::EndChild();
    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Nexus options panel
// ─────────────────────────────────────────────────────────────────────────────

void UI::RenderOptions()
{
    bool changed = false;

    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.f, 1.f), "Pathing Options");
    ImGui::Separator();

    // ── Rendering toggles ─────────────────────────────────────────────────────
    ImGui::TextDisabled("Rendering");
    changed |= ImGui::Checkbox("Render markers in world", &g_Settings.RenderMarkers);
    changed |= ImGui::Checkbox("Render trails in world",  &g_Settings.RenderTrails);
    ImGui::Spacing();

    // ── Opacity ───────────────────────────────────────────────────────────────
    ImGui::TextDisabled("Opacity");
    changed |= ImGui::SliderFloat("Marker opacity##mrkopac", &g_Settings.MarkerOpacity, 0.f, 1.f);
    changed |= ImGui::SliderFloat("Trail opacity##trloplac",  &g_Settings.TrailOpacity,  0.f, 1.f);
    ImGui::Spacing();

    // ── Scale ─────────────────────────────────────────────────────────────────
    ImGui::TextDisabled("Scale");
    changed |= ImGui::SliderFloat("Marker scale##mrkscl", &g_Settings.MarkerScale, 0.1f, 5.f);
    ImGui::Spacing();

    // ── Distances ─────────────────────────────────────────────────────────────
    ImGui::TextDisabled("Distances (world units)");
    if (ImGui::SliderFloat("Max render distance##maxrd",
                           &g_Settings.MaxRenderDist, 100.f, 10000.f))
    {
        // Clamp fade start to at most max distance
        g_Settings.FadeStartDist = std::min(g_Settings.FadeStartDist, g_Settings.MaxRenderDist);
        changed = true;
    }
    if (ImGui::SliderFloat("Fade start distance##fadesd",
                           &g_Settings.FadeStartDist, 0.f, g_Settings.MaxRenderDist))
    {
        changed = true;
    }
    ImGui::Spacing();

    // ── Screen size limits ────────────────────────────────────────────────────
    ImGui::TextDisabled("Screen size limits (pixels)");
    changed |= ImGui::SliderFloat("Min icon size##mnicsz", &g_Settings.MinScreenSize,  1.f, 32.f);
    changed |= ImGui::SliderFloat("Max icon size##mxicsz", &g_Settings.MaxScreenSize, 16.f, 256.f);
    ImGui::Spacing();

    // ── Behaviour ─────────────────────────────────────────────────────────────
    ImGui::TextDisabled("Behaviour");
    changed |= ImGui::Checkbox("Debug overlay", &g_Settings.ShowDebugInfo);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Show marker/trail count and pack status on screen");
    ImGui::Spacing();

    ImGui::Separator();

    // ── Pack folder shortcut ──────────────────────────────────────────────────
    ImGui::TextDisabled("Packs folder:");
    ImGui::SameLine();
    std::string dir = PackManager::PacksDir();
    ImGui::TextUnformatted(dir.empty() ? "(unavailable)" : dir.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Open"))
    {
        if (!dir.empty())
            ShellExecuteA(nullptr, "explore", dir.c_str(), nullptr, nullptr, SW_SHOW);
    }
    if (ImGui::SmallButton("Reload packs"))
        PackManager::Reload();

    if (changed) g_Settings.Save();
}
