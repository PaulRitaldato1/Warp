#pragma once

class ImGuiBackend;
class SwapChain;
class RenderGraph;

// Draws the editor UI over the tonemapped back buffer. The backend owns its own
// pipeline, so there is nothing to create at init.
void AddImGuiPass(RenderGraph& graph, ImGuiBackend* backend, SwapChain* swapChain);
