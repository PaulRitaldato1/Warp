#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/UploadBuffer.h>

class Device;
class ImGuiBackend;
class PipelineLibrary;
class SwapChain;

// Given to every pass once, in Init.
struct PassInitContext
{
	Device* device			   = nullptr;
	PipelineLibrary* pipelines = nullptr;

	// The swap chain's size, for passes that create size dependent targets.
	u32 width  = 0;
	u32 height = 0;
};

// The renderer's services, given to every pass each frame. All of them outlive
// the frame, so passes copy this into their lambdas.
struct PassContext
{
	const PipelineLibrary* pipelines = nullptr;
	UploadBuffer* uploadBuffer		 = nullptr;
	SwapChain* swapChain			 = nullptr;
	ImGuiBackend* imgui				 = nullptr; // null until ImGui is initialized
};

// Copies data into this frame's slice of the upload ring and binds it as a CBV.
template <typename T>
void BindConstants(CommandList& cmd, const PassContext& context, u32 rootIndex, const T& data)
{
	const UploadResult upload = context.uploadBuffer->AllocAndCopy(&data, sizeof(T), 256);
	cmd.SetConstantBufferView(rootIndex, context.uploadBuffer->GetBackingBuffer(), upload.offset, upload.size);
}
