#pragma once

#ifdef WARP_BUILD_VK

#include <Rendering/Renderer/Fence.h>
#include <Rendering/Renderer/Platform/Vulkan/VKCommon.h>

// ---------------------------------------------------------------------------
// VKFence — wraps a Vulkan timeline semaphore.
// The semaphore value is monotonically increasing.  GPUSignal() submits a
// signal operation to a queue and returns the value that was signalled;
// WaitForValue() blocks the CPU until the GPU reaches that value.
// ---------------------------------------------------------------------------

class VKFence : public Fence
{
public:
	~VKFence() override;

	void InitializeWithDevice(VkDevice device);

	// Fence overrides — CPU-side operations
	void Initialize()                   override;  // no-op; use InitializeWithDevice
	void Signal()                       override;  // CPU-side signal (m_value++)
	void Wait(u64 timeout = UINT64_MAX) override;  // CPU wait for current value
	u64  GetCompletedValue()      const override;
	void Reset()                        override;  // reset value back to 0

	// GPU-side helpers used by VKCommandQueue
	// Increment internal counter and return the new signal value.
	// The caller is responsible for embedding the semaphore into its vkQueueSubmit.
	u64 GetNextSignalValue() { return ++m_value; }

	// Submit a signal-only vkQueueSubmit (no command buffers).
	// Used for WaitForIdle-style synchronisation.
	void GPUSignalOnly(VkQueue queue);

	void WaitForValue(u64 value, u64 timeout = UINT64_MAX);

	VkSemaphore GetNative() const { return m_semaphore; }

private:
	VkDevice    m_device    = VK_NULL_HANDLE;
	VkSemaphore m_semaphore = VK_NULL_HANDLE;
	u64         m_value     = 0;  // most recently signalled value
};

#endif // WARP_BUILD_VK
