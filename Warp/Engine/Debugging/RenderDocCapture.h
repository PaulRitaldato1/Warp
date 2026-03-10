#pragma once

#ifndef WARP_RELEASE

#include <Common/CommonTypes.h>
#include <Debugging/renderdoc_app.h>
#include <Debugging/Logging.h>

#ifdef WARP_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(WARP_LINUX)
#include <dlfcn.h>
#endif

class RenderDocCapture
{
public:
	~RenderDocCapture() = default;

	RenderDocCapture()
	{
		pRENDERDOC_GetAPI getAPI = nullptr;

#ifdef WARP_WINDOWS
		HMODULE mod = GetModuleHandleA("renderdoc.dll");
		if (!mod)
		{
			mod = LoadLibraryA("renderdoc.dll");
		}
		if (mod)
		{
			getAPI =
				reinterpret_cast<pRENDERDOC_GetAPI>(reinterpret_cast<void*>(GetProcAddress(mod, "RENDERDOC_GetAPI")));
		}
		else
		{
			LOG_WARNING("RenderDoc: renderdoc.dll not found — add its directory to PATH or launch via RenderDoc UI");
		}
#elif defined(WARP_LINUX)
		if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
		{
			getAPI = reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(mod, "RENDERDOC_GetAPI"));
		}
#endif

		if (getAPI)
		{
			if (getAPI(eRENDERDOC_API_Version_1_7_0, reinterpret_cast<void**>(&m_api)) == 1)
			{
				LOG_DEBUG("RenderDoc API initialized");
			}
		}
	}

	bool IsAvailable() const { return m_api != nullptr; }

private:
	RENDERDOC_API_1_7_0* m_api = nullptr;
};

#else // WARP_RELEASE — stub, all calls compile away to nothing

class RenderDocCapture
{
public:
	bool IsAvailable() const { return false; }
};

#endif // WARP_RELEASE
