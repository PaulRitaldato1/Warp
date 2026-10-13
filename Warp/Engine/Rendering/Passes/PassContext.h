#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/UploadBuffer.h>

class PipelineLibrary;

// What every pass records with. Built once per frame by the renderer, and both
// outlive the frame, so passes copy it into their lambdas.
struct PassContext
{
	const PipelineLibrary* pipelines = nullptr;
	UploadBuffer* uploadBuffer		 = nullptr;
};

// Copies data into this frame's slice of the upload ring and binds it as a CBV.
template <typename T>
void BindConstants(CommandList& cmd, const PassContext& context, u32 rootIndex, const T& data)
{
	const UploadResult upload = context.uploadBuffer->AllocAndCopy(&data, sizeof(T), 256);
	cmd.SetConstantBufferView(rootIndex, context.uploadBuffer->GetBackingBuffer(), upload.offset, upload.size);
}
