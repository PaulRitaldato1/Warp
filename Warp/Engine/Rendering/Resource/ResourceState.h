#pragma once

#include <Common/CommonTypes.h>

// Asset loading state — distinct from Renderer's ResourceState (GPU barrier states).
enum class AssetState : u8
{
	Loading,   // Async load in progress on ThreadPool
	Uploading, // CPU data ready, GPU buffers created, staging uploads queued
	Ready      // GPU buffers available for rendering
};
