#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKCommandQueue.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommandList.h>
#include <Debugging/Assert.h>

void VKCommandQueue::InitializeWithDevice(VkDevice device, VkQueue queue, u32 familyIndex)
{
	DYNAMIC_ASSERT(device, "VKCommandQueue: device is null");
	DYNAMIC_ASSERT(queue,  "VKCommandQueue: queue is null");

	m_device      = device;
	m_queue       = queue;
	m_familyIndex = familyIndex;

	m_fence.InitializeWithDevice(device);
}

// ---------------------------------------------------------------------------
// CommandQueue overrides
// ---------------------------------------------------------------------------

u64 VKCommandQueue::Submit(const Vector<CommandList*>& lists)
{
	DYNAMIC_ASSERT(!lists.empty(), "VKCommandQueue::Submit: empty list");

	// Gather native command buffers into a contiguous array.
	Vector<VkCommandBuffer> cmdBufs(lists.size());
	for (u32 i = 0; i < lists.size(); ++i)
	{
		VKCommandList& vkList = static_cast<VKCommandList&>(*lists[i]);
		cmdBufs[i] = vkList.GetNative();
	}

	// Grab the next signal value from the fence and embed it in this submit.
	u64         signalValue = m_fence.GetNextSignalValue();
	VkSemaphore semaphore   = m_fence.GetNative();

	// Build timeline semaphore info for both wait and signal.
	VkSemaphore waitSemaphores[1]   = {};
	u64         waitValues[1]       = {};
	u32         waitCount           = 0;
	VkPipelineStageFlags waitStages[1] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };

	if (m_pendingWaitSemaphore != VK_NULL_HANDLE)
	{
		waitSemaphores[0] = m_pendingWaitSemaphore;
		waitValues[0]     = m_pendingWaitValue;
		waitCount         = 1;
	}

	VkTimelineSemaphoreSubmitInfo tsInfo = {};
	tsInfo.sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	tsInfo.waitSemaphoreValueCount   = waitCount;
	tsInfo.pWaitSemaphoreValues      = waitValues;
	tsInfo.signalSemaphoreValueCount = 1;
	tsInfo.pSignalSemaphoreValues    = &signalValue;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext                = &tsInfo;
	submitInfo.waitSemaphoreCount   = waitCount;
	submitInfo.pWaitSemaphores      = waitSemaphores;
	submitInfo.pWaitDstStageMask    = waitStages;
	submitInfo.commandBufferCount   = static_cast<u32>(cmdBufs.size());
	submitInfo.pCommandBuffers      = cmdBufs.data();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores    = &semaphore;

	VK_CHECK(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE),
	         "VKCommandQueue::Submit: vkQueueSubmit failed");

	// Clear pending wait — consumed by this submit.
	m_pendingWaitSemaphore = VK_NULL_HANDLE;
	m_pendingWaitValue     = 0;

	return signalValue;
}

void VKCommandQueue::WaitForValue(u64 value)
{
	m_fence.WaitForValue(value);
}

u64 VKCommandQueue::GetCompletedValue() const
{
	return m_fence.GetCompletedValue();
}

void VKCommandQueue::WaitForQueue(CommandQueue& other, u64 fenceValue)
{
	VKCommandQueue& vkOther = static_cast<VKCommandQueue&>(other);

	// Store the wait semaphore — it will be consumed by the next Submit().
	m_pendingWaitSemaphore = vkOther.m_fence.GetNative();
	m_pendingWaitValue     = fenceValue;
}

void VKCommandQueue::Reset()
{
	vkQueueWaitIdle(m_queue);
}

#endif // WARP_BUILD_VK
