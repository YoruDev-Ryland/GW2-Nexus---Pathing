#pragma once

namespace UI
{

// Called every frame (RT_Render) — draws the Pathing pack/category manager window.
void RenderWindow();

// Called when the Nexus Options panel is open (RT_OptionsRender).
// Do NOT call ImGui::Begin/End here — Nexus owns the surrounding window.
void RenderOptions();

} // namespace UI
