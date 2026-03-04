#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Window/Window.h>
#include <Rendering/Renderer/Renderer.h>

class RenderBackend
{
public:
	virtual ~RenderBackend() = default;

	// Defined in platform-specific .cpp files — only one is compiled per target.
	// No #ifdef chains anywhere else in the engine.
	static URef<RenderBackend> Create();

	virtual URef<IWindow>  MakeWindow(const String& name, int width, int height) = 0;
	virtual URef<Renderer> CreateRenderer()                                        = 0;
};
