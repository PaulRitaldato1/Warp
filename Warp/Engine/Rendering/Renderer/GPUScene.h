#pragma once

#include <Common/CommonTypes.h>
#include <Events/DelegateDefs.h>
#include <Rendering/Passes/PassContext.h>
#include <Rendering/Renderer/CommandList.h>
#include <Rendering/Renderer/DrawList.h>
#include <Rendering/Renderer/PipelineLibrary.h>
#include <Rendering/Renderer/RenderScene.h>

class Buffer;
class Device;
class FrameBlackboard;
class RenderGraph;
class ResourceManager;
class World;
struct FrameView;

// One view's culled draws: what a draw pass needs to issue its indirect draws.
struct SceneDrawView
{
	// GPUScene's draw list, which outlives the frame's graph.
	Span<const BatchItem> batches;

	Buffer* instances	   = nullptr; // in ShaderResource
	Buffer* visibleIndices = nullptr; // in ShaderResource once the cull has run
	Buffer* drawArgs	   = nullptr; // in IndirectArgument once the cull has run
};

// Added to the blackboard once the scene is uploaded and culled.
struct GPUSceneOutput
{
	SceneDrawView camera;
	SceneDrawView sunShadow; // no batches without a sun
};

// The GPU side of the RenderScene: the instance buffer, each view's visible index
// list and indirect args, culled on the CPU or GPU per r.GPUCulling, and the
// stats read back from those args.
class GPUScene
{
public:
	struct Stats
	{
		u32 batches = 0;

		// Read back from the draw args k_framesInFlight frames late, since with GPU
		// culling only the GPU knows what survived. Counted per submesh instance.
		u32 numTris			 = 0;
		u32 visibleInstances = 0;

		// Changes since last frame, and the slots re-uploaded because of them.
		u32 dirtyTransforms = 0;
		u32 dirtyMeshes		= 0;
		u32 removedMeshes	= 0;
		u32 uploadedSlots	= 0;

		// Per entity, from the CPU cull only.
		u32 tested = 0;
		u32 culled = 0;
	};

	~GPUScene();

	void Init(Device* device, PipelineLibrary& pipelines, u32 framesInFlight);
	void Shutdown();

	void SetWorld(World* world);
	void SetResourceManager(ResourceManager* resourceManager);

	// Once frameIndex's fence has passed: frees the buffers retired in that slot
	// and reads its stats readback.
	void BeginFrame(u32 frameIndex);

	// Syncs from the world, culls, grows every buffer, then adds the upload and GPU
	// cull passes and GPUSceneOutput to the blackboard.
	void AddUploadAndCull(RenderGraph& graph, FrameBlackboard& blackboard, const FrameView& view,
						  const PassContext& context);

	// After every pass that draws from the args, since this moves them to CopySource.
	void AddReadback(RenderGraph& graph);

	// Last frame's, until this frame's graph has run.
	const Stats& GetStats() const
	{
		return m_stats;
	}

	// The r.GPUCulling cvar. The CPU cull is kept as a reference to compare against.
	void SetGPUCulling(bool bEnabled);
	bool IsGPUCulling() const;

private:
	// Reallocates if needed. Contents are not preserved, so callers that rely on
	// them must rewrite after a true return.
	bool EnsureBufferCapacity(URef<Buffer>& buffer, u32& capacity, u32 needed, u32 stride, const char* name,
							  bool bUnorderedAccess = false);

	// Grows every buffer the frame needs, before any pass is added, so the Buffer
	// pointers passes are given stay valid while the graph runs. Returns whether
	// the instance buffer was regrown, which forces a full upload.
	bool GrowBuffers(const RenderScene::CullRegions& regions, bool bGPUCulling);

	// One record per scene batch, indexed by batch, with instanceCount 0. The CPU
	// cull fills the counts in; the GPU cull counts up from 0.
	Vector<DrawIndexedArgs> BuildDrawArgs() const;

	// Resolves each batch's buffers and textures. Per frame, since textures can
	// finish loading after the batch was created.
	Vector<BatchItem> BuildBatchItems(const Vector<RenderScene::VisibleBatch>& batches) const;

	// Copies the scene's written slots into the instance buffer, or all of them
	// when bFullUpload, e.g. after a regrow left it empty.
	void UploadInstances(CommandList& cmd, UploadBuffer& uploadBuffer, bool bFullUpload);

	// The buffer must already be big enough, see GrowBuffers.
	static void UploadSlotIndices(CommandList& cmd, UploadBuffer& uploadBuffer, const Vector<u32>& indices,
								  Buffer* buffer);

	// Leaves the buffer in finalState: IndirectArgument to draw directly, or
	// UnorderedAccess for the cull to count into. Must already be big enough.
	static void UploadDrawArgs(CommandList& cmd, UploadBuffer& uploadBuffer, const Vector<DrawIndexedArgs>& args,
							   Buffer* buffer, ResourceState finalState);

	enum class ReadbackView : u8
	{
		Camera,
		Sun,
		Count
	};

	// Copies a view's draw args into this frame slot's readback buffer, read in
	// BeginFrame once the slot's fence has passed.
	void RecordArgsReadback(CommandList& cmd, ReadbackView readbackView, Buffer* drawArgs, u32 count);

	struct ReadbackStats
	{
		u32 visibleInstances = 0;
		u32 numTris			 = 0;
	};
	ReadbackStats ReadArgsReadback();

	// Subscribed to r.GPUCulling between Init and Shutdown.
	void OnGPUCullingChanged(bool bEnabled);
	URef<MemberFuncType<GPUScene, bool>> m_gpuCullingChangedDelegate;

	Device* m_device				   = nullptr;
	World* m_world					   = nullptr; // non-owning
	ResourceManager* m_resourceManager = nullptr; // non-owning
	u32 m_frameIndex				   = 0;

	ComputePipelineHandle m_cullPipeline;

	RenderScene m_scene;
	RenderScene::CullResult m_cameraCull;
	RenderScene::CullResult m_sunCull;
	DrawList m_drawList;
	bool m_bHasSunView = false; // this frame's, for AddReadback

	Stats m_stats;
	ReadbackStats m_readbackStats;

	// GPU copy of RenderScene's instances, indexed by slot. Shared by both views,
	// written only where slots changed, and grown on demand rather than capped.
	URef<Buffer> m_instanceBuffer;
	u32 m_instanceCapacity = 0;

	// Visible slot indices per view, rebuilt by the cull every frame. UAVs so the
	// GPU cull can write them; the CPU cull uploads into the same buffers.
	URef<Buffer> m_cameraIndexBuffer;
	URef<Buffer> m_sunIndexBuffer;
	u32 m_cameraIndexCapacity = 0;
	u32 m_sunIndexCapacity	  = 0;

	// Indirect draw records, indexed by scene batch. UAVs so the GPU cull can
	// count instances into them.
	URef<Buffer> m_cameraArgsBuffer;
	URef<Buffer> m_sunArgsBuffer;
	u32 m_cameraArgsCapacity = 0;
	u32 m_sunArgsCapacity	 = 0;

	// Where each batch's region starts in the GPU visible index lists.
	URef<Buffer> m_regionStartBuffer;
	u32 m_regionStartCapacity = 0;

	// Per view, per frame slot. Written at the end of a frame, read when that
	// slot comes round again and its fence has passed.
	struct ArgsReadback
	{
		URef<Buffer> buffer;
		u32 count = 0;
	};
	Array<Vector<ArgsReadback>, static_cast<u32>(ReadbackView::Count)> m_argsReadback;

	// Buffers replaced by a grow, parked in the frame slot that was current when
	// they were retired. BeginFrame clears a slot only once its fence has passed,
	// so the GPU is provably finished with them.
	Vector<Vector<URef<Buffer>>> m_retiredBuffers;
};
