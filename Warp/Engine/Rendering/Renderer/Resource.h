#pragma once

enum class ResourceType
{
	Buffer,
	Texture,
	RenderTarget,
	DepthStencil
};

class Resource
{
public:
	virtual ~Resource() = default;

	virtual ResourceType GetType() = 0;

	virtual size_t GetSize() = 0;

	virtual void Bind(u32 Slot) const = 0;

	virtual void Cleanup() = 0;

	virtual void* GetNativeHandle() = 0;
};