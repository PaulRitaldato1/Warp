# Warp
A high performance, multi API, multi platform, hobby renderer focused on modern engine architecture. Featuring an abstracted RHI, asynchronous GPU execution, GPU driven rendering, and a parallelized rendering pipeline targeting DX12, Vulkan, and Metal.

## Goal
The overall goal of this project is to use it as a rendering sandbox for my own personal learnings. This is my second attempt at making a hobby renderer. See WarpOpenGL for my first noob-y attempt. The focus of this one is to have a more 'real' rendering architecture, that can support multiple APIs. I specifically want to target DX12, Vulkan, and Metal. One of the reasons for this is to learn and grow my skills with more advanced rendering techniques, and engine architecture as a whole. While this will have a lot of features of a game engine, this is almost purely a sandbox to play with rendering features and architecture. My goal is to have a fully parallel rendering architecture with the RHI abstracted away such that I can implement more advanced rendering techniques without having to worry about the specifics of each API.

## Current Status
The core architecture is in a good place and the foundational systems are stood up. The RHI abstraction works across DX12 (Windows) and Vulkan (Windows + Linux), with GLFW handling the window on every platform and a single RenderBackend picking the API at startup from `Config/Engine.ini`. There are separate parallel CommandQueues for Graphics/Compute/Copy with automatic cross-queue dependency handling, 2 frames in flight, and proper acquire/present semaphores on Vulkan. Shaders are HLSL everywhere, compiled with DXC for both backends (shaderc is gone). Metal is still on the list.

### Rendering
* **Deferred shading** with a GBuffer geometry pass and a PBR lighting pass (Cook-Torrance BRDF). Point, directional, and spot lights through a LightComponent.
* **Directional shadow pass** with its own frustum culling, and an abstracted sampler so samplers aren't per platform.
* **Procedural sky** via a SkyComponent. The entity's transform drives the sun direction and the same entity drives a directional light so everything stays in sync. Colors, sun disc, and ground fade are tweakable in the inspector.
* **GPU driven drawing.** This is where most of the recent work has gone:
  * Instancing for every duplicate mesh in both the geometry and shadow passes (10k cubes went from 18 fps to 126 fps on the first pass).
  * A persistent `RenderScene` that keeps a slot per entity with instance data, bounds, and batches. The ECS tracks writes per component through const-ness, so only dirty entities get rebuilt and uploaded. A static scene does 0 uploads per frame.
  * Indirect draws for both passes (`ExecuteIndirect` on D3D12, `vkCmdDrawIndexedIndirect` on Vulkan).
  * GPU culling, a compute shader culls every instance slot and appends survivors into each batch's visible list, then the passes draw indirect from those args. The CPU doesn't touch anything per instance anymore. At 100k instances draw is ~0.1ms and it runs around 900 fps (vs 350-380 with CPU culling, and 26 fps at 10k before instancing). CPU culling is still around behind a toggle to compare against.
* Vertex stream split so position-only passes (shadows, culling) are more cache friendly.

### Engine
* **ECS** with archetypes, systems, `Without<>` filters, change tracking via `Each<const T>` / `Read<T>` vs mutable access, and a component descriptor registry that drives a generic ImGui inspector. Components self-register their UI through a `ComponentUI` template specialization, so adding new components to the editor is basically free.
* **Asynchronous resource loading.** Mesh (glTF via fastgltf) and texture loaders feed a ResourceManager that handles the full upload lifecycle from disk to GPU. It's built on a ThreadPool I wrote years ago, with C++20 coroutines layered on top as an exercise in going more async than just dumping work onto the pool.
* **Console and cvars.** `Cvar<T>` is declared as a global next to the code that uses it and registers itself before main, reading it is just a member read. Console commands are named events with string args. There's an in-game ImGui console on backtick with history, tab completion, and `help`/`clear`. `Engine.ini` runs through the same registry at startup like typing each line into the console, so typos actually warn with the line number.
* **Events and input.** EventManagers take lambdas or delegates, support unsubscribing (even mid broadcast), and the input manager exposes per key down/up events.
* Built with C++23, using things like `std::expected` and `std::span` where they fit.

### Debugging and tooling
* **Scope profiler** that writes Chrome trace JSON (viewable in a browser, similar to Unreal Insights). It writes on a background thread so a scope only costs a timestamp pair and a push. Per name totals feed a live stats panel. `WARP_PROFILING=OFF` compiles it out.
* **Render stats UI** with draw calls, visible instances/tris, cull stats, and CPU timing.
* **GPU debug markers** on both backends (PIX metadata on D3D12, debug utils labels on Vulkan), so passes show up named in RenderDoc/PIX.
* **RenderDoc integration**, loadable at startup through `r.RenderDoc`.
* D3D12 debug layer and Vulkan validation in Debug builds.
* A GeoGenerator for procedural meshes (planes, boxes) and a default texture fallback system.

Claude has been a significant help throughout, from the ECS and ResourceManager to the deferred lighting debug sessions, a pile of Vulkan bugs, the GPU driven pipeline, and the ImGui UI. I'll credit where it's due.

## Building
Requires CMake 3.16+, a C++23 compiler, and the Vulkan SDK (its DXC is used to emit SPIR-V). Every other dependency (DirectXMath, fastgltf, tinyddsloader, stb, VMA, ImGui, GLFW) is pulled in through CMake FetchContent.

**Windows** builds with Ninja and the clang-cl that ships with Visual Studio. The path to clang-cl is pinned in the script, so edit `CLANG_CL` if your install is somewhere else.
```bat
Windows-Build.bat [Debug|Release]
```

**Linux**
```bash
./Unix-Build.sh [Debug|Release]
```

There are also `*-Clean` and `*-Format` scripts, and `Windows-GenVSProject.bat` for a Visual Studio solution.

### Configuration
`Config/Engine.ini` is read straight from the repo at startup, so editing it never needs a rebuild. Each line sets a console variable, same as typing it in the console:
```ini
r.GraphicsAPI = Vulkan   # D3D12 or Vulkan, D3D12 is Windows only
r.RenderDoc = 1          # 0 if you want Vulkan validation, RenderDoc strips the layer
```
`r.GraphicsAPI` and `r.RenderDoc` are startup only. Other cvars like `r.GPUCulling` and `log.Verbose` can be changed live from the console. Type `help` in the console for the full list.

## Next Steps
Items to implement:
  * Separate game update and render update into separate threads (RenderScene extract/apply across a render thread)
  * Cascaded shadow maps, plus normal offset shadows
  * Implement various forms of AO (SSAO, HBAO, etc)
  * Implement various forms of AA (TAA, FXAA, MSAA, etc)
  * Implement a Forward+ rendering pipeline
  * Implement screen space reflections
  * Implement a simple VFX/particle emitter
  * Implement skeletal animation support
  * Height fog / volumetric fog
  * Switching the graphics API at runtime
  * `exec` command for running preset files of console commands
  * Metal backend
  * Wayland support for Linux
  * etc ...
