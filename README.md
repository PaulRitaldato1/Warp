# Warp
A high performance, multi API, multi platform, hobby renderer focused on modern engine architecture. Featuring an abstracted RHI, asynchronous GPU execution, and a parallelized rendering pipeline targeting DX12, Vulkan, and Metal.

## Goal
The overrall goal of this project is to use it as a rendering sandbox for my own personal learnings. This is my second attempt at making a hobby renderer. See WarpOpenGL for my first noob-y attempt. The focus of this one is to have a more 'real' rendering architecture, that can support multiple APIs. I specifically want to target DX12, Vulkan, and Metal. One of the reasons for this is to learn and grow my skills with more advanced rendering techniques, and engine architecture as a whole. While this will have a lot of features of a game engine, this is almost purely a sandbox to play with rendering features and architecture. My goal is to have a fully parallel rendering architecture with the RHI abstracted away such that I can implement more advanced rendering techniques without having to worry about the specifics of each API.

## Current Status
The core architecture is in a good place and most of the foundational systems are stood up. The RHI abstraction is working across both DX12 (Windows) and Vulkan (Windows + Linux/X11), with explicit attention paid to asynchronous GPU execution — separate parallel CommandQueues for Graphics/Compute/Copy with automatic cross-queue dependency handling. An asynchronous mesh and texture loader feed into a ResourceManager that handles the full upload lifecycle from disk to GPU, built on top of a ThreadPool I wrote years ago.

On the rendering side, a deferred shading pipeline is fully working with a GBuffer geometry pass and a PBR lighting pass (Cook-Torrance BRDF). Point, directional, and spot lights are all supported through a LightComponent in the ECS. A procedural sky system was added recently via a SkyComponent — the entity's transform controls the sun direction, and the same entity drives a directional light so everything stays in sync. The sky colors, sun disc, and ground fade are all tweakable at runtime through the ImGui inspector.

The ECS itself supports archetypes, systems, and a component descriptor registry that drives a generic ImGui inspector. Components self-register their UI through a ComponentUI template specialization, so adding new components to the editor is basically free. There's also a built-in GeoGenerator for procedural meshes (planes, boxes), a default texture fallback system, and RenderDoc integration for debugging.

Claude has been a significant help throughout, from the ECS and ResourceManager to the deferred lighting debug sessions and the ImGui UI. I'll credit where it's due.

## Next Steps
Items to implement:
  * Separate game update and render update into separate threads
  * Implement shadow maps, cascade shadow maps
  * Implement various forms of AO (SSAO, HBAO, etc)
  * Implement various forms of AA (TAA, FXAA, MSAA, etc)
  * Implement a Forward+ rendering pipeline
  * Implement screen space reflections
  * Implement a simple VFX/particle emitter
  * Implement skeletal animation support
  * Height fog / volumetric fog
  * Wayland support for Linux
  * etc ...
