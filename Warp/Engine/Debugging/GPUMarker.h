#pragma once

#include <string>
#include <Rendering/Renderer/CommandList.h>

namespace Warp::Debugging
{
class GPUMarker
{
public:
	// Begin an event with an ANSI string.
	static inline void BeginEvent(CommandList* CmdList, const char* Name)
	{
		if (!CmdList || !Name)
		{
			return;
		}

		CmdList->BeginEvent(Name);
	}

	// End the most recent event.
	static inline void EndEvent(CommandList* cmdList)
	{
		if (!cmdList)
		{
			return;
		}

		cmdList->EndEvent();
	}

	// Place a non-nested marker.
	static inline void SetMarker(CommandList* cmdList, const char* name)
	{
		if (!cmdList || !name)
		{
			return;
		}

		cmdList->SetMarker(name);
	}

	// Convenience: convert UTF-16 to UTF-8 and begin event.
	static inline void BeginEventUtf8FromW(CommandList* cmdList, const wchar_t* wname)
	{
		if (!cmdList || !wname)
		{
			return;
		}

		// simple narrow conversion (locale-independent, for ASCII/UTF-8-friendly names)
		std::wstring ws(wname);
		std::string s(ws.begin(), ws.end());
		BeginEvent(cmdList, s.c_str());
	}
};

// RAII scope wrapper for GPU events.
class ScopedGpuMarker
{
public:
	ScopedGpuMarker(CommandList* cmdList, const char* name) : m_cmdList(cmdList)
	{
		GPUMarker::BeginEvent(m_cmdList, name);
	}

	// non-copyable
	ScopedGpuMarker(const ScopedGpuMarker&)			   = delete;
	ScopedGpuMarker& operator=(const ScopedGpuMarker&) = delete;

	// movable
	ScopedGpuMarker(ScopedGpuMarker&& other) noexcept : m_cmdList(other.m_cmdList)
	{
		other.m_cmdList = nullptr;
	}

	ScopedGpuMarker& operator=(ScopedGpuMarker&& other) noexcept
	{
		if (this != &other)
		{
			End();
			m_cmdList		= other.m_cmdList;
			other.m_cmdList = nullptr;
		}
		return *this;
	}

	~ScopedGpuMarker()
	{
		End();
	}

	void End()
	{
		if (m_cmdList)
		{
			GPUMarker::EndEvent(m_cmdList);
			m_cmdList = nullptr;
		}
	}

private:
	CommandList* m_cmdList = nullptr;
};
} // namespace Warp::Debugging

#define WARP_GPU_MARKER_CONCAT_INTERNAL(x, y) x##y
#define WARP_GPU_MARKER_CONCAT(x, y) WARP_GPU_MARKER_CONCAT_INTERNAL(x, y)
#define WARP_SCOPED_GPU_MARKER(cmdList, name)                                                                          \
	Warp::Debugging::ScopedGpuMarker WARP_GPU_MARKER_CONCAT(_gpu_marker_, __LINE__)(cmdList, name)