#pragma once

#include <Common/CommonTypes.h>
#include <Math/Math.h>
#include <Rendering/Lighting/LightData.h>
#include <Rendering/Passes/GBufferPass.h>
#include <Rendering/Passes/PassContext.h>
#include <Rendering/Renderer/DrawList.h>
#include <Rendering/Renderer/PipelineLibrary.h>

class Texture;
class RenderGraph;

// Full screen deferred lighting. Reads the GBuffer, depth and shadow map, and
// writes linear HDR for tonemap.

PipelineHandle CreateLightingPassPipeline(PipelineLibrary& library);

struct LightingPassInputs
{
	PipelineHandle pipeline;

	GBufferTargets gbuffer;		  // moved to ShaderResource here
	Texture* depth	   = nullptr; // moved to ShaderResource here
	Texture* shadowMap = nullptr; // already ShaderResource, from the shadow pass
	Texture* hdrTarget = nullptr; // left in RenderTarget

	Mat4 viewProj		= {};
	Vec3 cameraPosition = {};

	LightList lights;
	Vector<LightInfo> skyLights; // the sun, after the scene lights
	SkyParameters sky  = {};
	Mat4 lightViewProj = {}; // the directional shadow map's
};

void AddLightingPass(RenderGraph& graph, const PassContext& context, const LightingPassInputs& inputs);
