#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Platform/Vulkan/VKFence.h>
#include <Debugging/Assert.h>
#include <Debugging/Logging.h>

VKFence::~VKFence()
{
	if (m_semaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(m_device, m_semaphore, nullptr);
		m_semaphore = VK_NULL_HANDLE;
	}
}

void VKFence::InitializeWithDevice(VkDevice device)
{
	DYNAMIC_ASSERT(device, "VKFence::InitializeWithDevice: device is null");
	m_device = device;
	m_value  = 0;

	VkSemaphoreTypeCreateInfo typeInfo = {};
	typeInfo.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	typeInfo.initialValue  = 0;

	VkSemaphoreCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	createInfo.pNext = &typeInfo;

	VK_CHECK(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_semaphore),
	         "VKFence: failed to create timeline semaphore");
}

// ---------------------------------------------------------------------------
// Fence overrides
// ---------------------------------------------------------------------------

void VKFence::Initialize()
{
	// No-op — use InitializeWithDevice before recording any GPU work.
}

void VKFence::Signal()
{
	++m_value;

	VkSemaphoreSignalInfo signalInfo = {};
	signalInfo.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
	signalInfo.semaphore = m_semaphore;
	signalInfo.value     = m_value;

	VK_CHECK(vkSignalSemaphore(m_device, &signalInfo),
	         "VKFence::Signal: vkSignalSemaphore failed");
}

void VKFence::Wait(u64 timeout)
{
	WaitForValue(m_value, timeout);
}

u64 VKFence::GetCompletedValue() const
{
	u64 completed = 0;
	vkGetSemaphoreCounterValue(m_device, m_semaphore, &completed);
	return completed;
}

void VKFence::Reset()
{
	// Timeline semaphores cannot be reset below their current GPU-completed value.
	// We just reset our local tracking counter so callers can reuse it from 0.
	// In practice, WaitForValue() is the correct way to throttle the CPU.
	m_value = 0;
}

// ---------------------------------------------------------------------------
// GPU helpers
// ---------------------------------------------------------------------------

void VKFence::GPUSignalOnly(VkQueue queue)
{
	// Submits a signal-only operation (no command buffers).
	// Use GetNextSignalValue() + embed the semaphore in the caller's vkQueueSubmit
	// when you also have command buffers to execute.
	u64 signalValue = ++m_value;

	VkTimelineSemaphoreSubmitInfo tsInfo = {};
	tsInfo.sType                     = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	tsInfo.signalSemaphoreValueCount = 1;
	tsInfo.pSignalSemaphoreValues    = &signalValue;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext                = &tsInfo;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores    = &m_semaphore;

	VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
	         "VKFence::GPUSignalOnly: vkQueueSubmit failed");
}

void VKFence::WaitForValue(u64 value, u64 timeout)
{
	if (GetCompletedValue() >= value)
		return;

	VkSemaphoreWaitInfo waitInfo = {};
	waitInfo.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	waitInfo.semaphoreCount = 1;
	waitInfo.pSemaphores    = &m_semaphore;
	waitInfo.pValues        = &value;

	// Cap the default infinite timeout at 5 seconds (in nanoseconds) so a GPU hang
	// surfaces as a logged error instead of blocking the process forever.
	static constexpr u64 k_defaultTimeoutNs = 5ULL * 1'000'000'000ULL;
	u64 effectiveTimeout = (timeout == UINT64_MAX) ? k_defaultTimeoutNs : timeout;

	VkResult result = vkWaitSemaphores(m_device, &waitInfo, effectiveTimeout);
	if (result == VK_TIMEOUT)
	{
		LOG_ERROR("VKFence: WaitForValue({}) timed out after 5s — possible GPU hang", value);
	}
	else
	{
		VK_CHECK(result, "VKFence::WaitForValue: vkWaitSemaphores failed");
	}
}

#endif // WARP_BUILD_VK
