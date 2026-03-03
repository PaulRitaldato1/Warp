# Warp
A high performance, multi API, multi platform, hobby renderer focused on modern engine architecture. Featuring an abstracted RHI, asynchronous GPU execution, and a parallelized rendering pipeline targeting DX12, Vulkan, and Metal.

## Goal
The overrall goal of this project is to use it as a rendering sandbox for my own personal learnings. This is my second attempt at making a hobby renderer. See WarpOpenGL for my first noob-y attempt. The focus of this one is to have a more 'real' rendering architecture, that can support multiple APIs. I specifically want to target DX12, Vulkan, and Metal. One of the reasons for this is to learn and grow my skills with more advanced rendering techniques, and engine architecture as a whole. While this will have a lot of features of a game engine, this is almost purely a sandbox to play with rendering features and architecture. My goal is to have a fully parallel rendering architecture with the RHI abstracted away such that I can implement more advanced rendering techniques without having to worry about the specifics of each API.

## Current Status
Most/all of the pipework is in to start getting into the meat of the engine. The basic abstractions are there for the rendering types, and currently Linux (X11/ Wayland to come later) with Vulkan, and Windows with DX12 are implemented. A good enough Logger is implemented, a basic input system, an ECS (with help from Claude), the bones of a rendering pipeline, and a hello world triangle has been built. Explicit attention has been paid to asyncronous gpu execution (separate, parallel CommandQueues for Graphics/Compute/Copy) and CPU/GPU memory, with an automatic pipeline that can handle cross queue dependencies depending on the type of memory required. An asyncronous Mesh and Texture loader have been implented and used as part of a ResourceManager (with help from Claude) that makes use of my personal ThreadPool I wrote years ago. 

## Next Steps
Items to implement:
  * Separate game update, and render update into separate threads
  * Implement a naive Deferred rendering pipeline with blinn phong shading (for now, PBR later)
  * Implement a Forward+ rendering pipeline
  * Implement multiple shadowing techniques such as shadow maps, cascade shadow maps, etc
  * Implement other forms of "shadowing" such as the various types of AO
  * Implement reflections
  * Implement a simple VFX emitter
  * Implement a simple Animation/Skeletal mesh support
  * etc ...
