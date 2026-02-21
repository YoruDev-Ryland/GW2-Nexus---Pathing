#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// MarkerRenderer
//
// Draws TacO/BlishHUD POI markers and trail breadcrumbs onto the screen
// using ImGui's background draw list.
//
// Projection pipeline:
//   GW2 world space  →  view matrix (from MumbleLink camera)
//                     →  perspective projection (from MumbleIdent FOV)
//                     →  screen-space ImGui coordinates
//
// All drawing happens inside the RT_Render callback (called every frame).
// ─────────────────────────────────────────────────────────────────────────────
namespace MarkerRenderer
{

// Called from the RT_Render ImGui callback.
// Reads MumbleLink/MumbleIdent for camera state, queries PackManager for the
// current map's POIs and trails, projects them and draws with ImGui.
void Render();

} // namespace MarkerRenderer
