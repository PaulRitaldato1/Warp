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

u64 VKCommandQueue::Submit(CommandList& list)
{
	VKCommandList& vkList  = static_cast<VKCommandList&>(list);
	VkCommandBuffer cmdBuf = vkList.GetNative();

	// Grab the next signal value from the fence and embed it in this submit.
	u64         signalValue = m_fence.GetNextSignalValue();
	VkSemaphore semaphore   = m_fence.GetNative();

	VkTimelineSemaphoreSubmitInfo tsInfo = {};
	tsInfo.sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	tsInfo.signalSemaphoreValueCount = 1;
	tsInfo.pSignalSemaphoreValues    = &signalValue;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext                = &tsInfo;
	submitInfo.commandBufferCount   = 1;
	submitInfo.pCommandBuffers      = &cmdBuf;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores    = &semaphore;

	VK_CHECK(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE),
	         "VKCommandQueue::Submit: vkQueueSubmit failed");

	return signalValue;
}

void VKCommandQueue::WaitForValue(u64 value)
{
	m_fence.WaitForValue(value);
}

void VKCommandQueue::Reset()
{
	vkQueueWaitIdle(m_queue);
}

#endif // WARP_BUILD_VK
