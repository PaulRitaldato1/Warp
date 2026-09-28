#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/CommandQueue.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>
#include <Rendering/Renderer/Platform/Vulkan/VKFence.h>

// ---------------------------------------------------------------------------
// VKCommandQueue — wraps a VkQueue with an owned timeline semaphore for
// CPU/GPU synchronization.  Submit() records a signal on the semaphore and
// returns the signalled value; WaitForValue() blocks the CPU until the GPU
// has reached that value.
// ---------------------------------------------------------------------------

class VKCommandQueue : public CommandQueue
{
public:
	~VKCommandQueue() override = default;

	void InitializeWithDevice(VkDevice device, VkQueue queue, u32 familyIndex);

	u64  Submit(const Vector<CommandList*>& lists)           override;
	void WaitForValue(u64 value)                           override;
	u64  GetCompletedValue() const                         override;
	void WaitForQueue(CommandQueue& other, u64 fenceValue) override;
	void WaitForIdle()                                     override;
	void Reset()                                           override;

	VkQueue GetNative()       const { return m_queue; }
	u32     GetFamilyIndex()  const { return m_familyIndex; }

	// Binary semaphores for the next Submit only. The swap chain uses them so the
	// frame's submit waits for its acquired image and signals present, with no CPU
	// wait on either side. D3D12 has no equivalent: DXGI does this internally.
	void AddBinaryWait(VkSemaphore semaphore, VkPipelineStageFlags stage);
	void AddBinarySignal(VkSemaphore semaphore);

private:
	VkDevice     m_device      = VK_NULL_HANDLE;
	VkQueue      m_queue       = VK_NULL_HANDLE;
	u32          m_familyIndex = 0;
	VKFence      m_fence;

	// Pending cross-queue wait — accumulated by WaitForQueue(), consumed by Submit().
	VkSemaphore  m_pendingWaitSemaphore = VK_NULL_HANDLE;
	u64          m_pendingWaitValue     = 0;

	// Consumed by the next Submit.
	Vector<VkSemaphore>          m_binaryWaits;
	Vector<VkPipelineStageFlags> m_binaryWaitStages;
	Vector<VkSemaphore>          m_binarySignals;
};

#endif // WARP_BUILD_VK
