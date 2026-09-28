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
	const u64 signalValue = m_fence.GetNextSignalValue();

	// Timeline and binary semaphores share one submit. Timeline entries carry a
	// value; binary entries need a slot in the value arrays too, which is ignored.
	Vector<VkSemaphore> waitSemaphores;
	Vector<u64> waitValues;
	Vector<VkPipelineStageFlags> waitStages;

	if (m_pendingWaitSemaphore != VK_NULL_HANDLE)
	{
		waitSemaphores.push_back(m_pendingWaitSemaphore);
		waitValues.push_back(m_pendingWaitValue);
		waitStages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
	}

	for (size_t i = 0; i < m_binaryWaits.size(); ++i)
	{
		waitSemaphores.push_back(m_binaryWaits[i]);
		waitValues.push_back(0);
		waitStages.push_back(m_binaryWaitStages[i]);
	}

	Vector<VkSemaphore> signalSemaphores = { m_fence.GetNative() };
	Vector<u64> signalValues			 = { signalValue };

	for (VkSemaphore semaphore : m_binarySignals)
	{
		signalSemaphores.push_back(semaphore);
		signalValues.push_back(0);
	}

	VkTimelineSemaphoreSubmitInfo tsInfo = {};
	tsInfo.sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	tsInfo.waitSemaphoreValueCount   = static_cast<u32>(waitValues.size());
	tsInfo.pWaitSemaphoreValues      = waitValues.data();
	tsInfo.signalSemaphoreValueCount = static_cast<u32>(signalValues.size());
	tsInfo.pSignalSemaphoreValues    = signalValues.data();

	VkSubmitInfo submitInfo = {};
	submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext                = &tsInfo;
	submitInfo.waitSemaphoreCount   = static_cast<u32>(waitSemaphores.size());
	submitInfo.pWaitSemaphores      = waitSemaphores.data();
	submitInfo.pWaitDstStageMask    = waitStages.data();
	submitInfo.commandBufferCount   = static_cast<u32>(cmdBufs.size());
	submitInfo.pCommandBuffers      = cmdBufs.data();
	submitInfo.signalSemaphoreCount = static_cast<u32>(signalSemaphores.size());
	submitInfo.pSignalSemaphores    = signalSemaphores.data();

	VK_CHECK(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE),
	         "VKCommandQueue::Submit: vkQueueSubmit failed");

	// All consumed by this submit.
	m_pendingWaitSemaphore = VK_NULL_HANDLE;
	m_pendingWaitValue     = 0;
	m_binaryWaits.clear();
	m_binaryWaitStages.clear();
	m_binarySignals.clear();

	return signalValue;
}

void VKCommandQueue::AddBinaryWait(VkSemaphore semaphore, VkPipelineStageFlags stage)
{
	m_binaryWaits.push_back(semaphore);
	m_binaryWaitStages.push_back(stage);
}

void VKCommandQueue::AddBinarySignal(VkSemaphore semaphore)
{
	m_binarySignals.push_back(semaphore);
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

void VKCommandQueue::WaitForIdle()
{
	vkQueueWaitIdle(m_queue);
}

void VKCommandQueue::Reset()
{
	WaitForIdle();
}

#endif // WARP_BUILD_VK
