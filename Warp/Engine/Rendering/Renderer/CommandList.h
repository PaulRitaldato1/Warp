#pragma once

#include <Common/CommonTypes.h>

class PipelineState;
class Buffer;
class Resource;

class CommandList
{
public:
	virtual ~CommandList() = default;

	// frameIndex selects which internal allocator slot to reset.
	// Each platform implementation owns one allocator per frame-in-flight.
	virtual void Begin(u32 frameIndex) = 0;
	virtual void End()                 = 0;

	// GPU debug markers — default to no-ops so backends without marker support
	// don't need to override them.
	virtual void BeginEvent(std::string_view name) {}
	virtual void EndEvent()                        {}
	virtual void SetMarker(std::string_view name)  {}

	virtual void SetPipelineState(URef<PipelineState> state)     = 0;
	virtual void SetVertexBuffer(URef<Buffer> vertexBuffer)      = 0;
	virtual void SetIndexBuffer(URef<Buffer> indexBuffer)        = 0;
	virtual void BindResource(u32 slot, URef<Resource> resource) = 0;

	virtual void Draw(u32 vertexCount, u32 instanceCount = 1,
	                  u32 firstVertex = 0, u32 firstInstance = 0) = 0;

	virtual void DrawIndexed(u32 indexCount, u32 instanceCount = 1,
	                         u32 firstIndex = 0, u32 firstVertex = 0,
	                         u32 firstInstance = 0) = 0;

	virtual void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ) = 0;
};
