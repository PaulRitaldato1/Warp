#pragma once

#include <Common/CommonTypes.h>

class PipelineState;
class Buffer;
class Resource;

class ICommandList
{
public:
	virtual void Begin() = 0;
	virtual void End()	 = 0;

	virtual void SetPipelineState(URef<PipelineState> State)	 = 0;
	virtual void SetVertexBuffer(URef<Buffer> VertexBuffer)		 = 0;
	virtual void SetIndexBuffer(URef<Buffer> IndexBuffer)		 = 0;
	virtual void BindResource(u32 Slot, URef<Resource> Resource) = 0;

	// Draw commands
	virtual void Draw(u32 VertexCount, u32 InstanceCount = 1, u32 FirstVertex = 0, u32 FirstInstance = 0) = 0;

	virtual void DrawIndexed(u32 IndexCount, u32 InstanceCount = 1, u32 FirstIndex = 0, u32 FirstVertex = 0,
							 u32 FirstInstance = 0) = 0;

	virtual void Dispatch(u32 GroupCountX, u32 GroupCountY, u32 GroupCountZ) = 0;

private:
};
