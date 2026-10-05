#include <Debugging/RenderDocCapture.h>

#include <Core/Console/ConsoleVariable.h>
#include <Debugging/Logging.h>

// Declared in release too so Engine.ini does not warn about an unknown name.
//
// Loading it ourselves puts RenderDoc's layer in every run, and it strips the
// validation layer unless its own API Validation option is on. 0 keeps it out.
// Launching from the RenderDoc UI injects it regardless.
static Cvar<bool> CvarRenderDoc("r.RenderDoc", true, "Load renderdoc.dll at startup so captures work without the RenderDoc UI",
								CvarFlags::Startup);

#ifndef WARP_RELEASE

#ifdef WARP_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(WARP_LINUX)
#include <dlfcn.h>
#endif

RenderDocCapture::RenderDocCapture()
{
	pRENDERDOC_GetAPI getAPI = nullptr;

#ifdef WARP_WINDOWS
	HMODULE mod = GetModuleHandleA("renderdoc.dll");

	if (!mod && !CvarRenderDoc.Get())
	{
		LOG_DEBUG("RenderDoc: not loaded, r.RenderDoc = 0");
		return;
	}

	if (!mod)
	{
		mod = LoadLibraryA("renderdoc.dll");
	}
	if (mod)
	{
		getAPI = reinterpret_cast<pRENDERDOC_GetAPI>(reinterpret_cast<void*>(GetProcAddress(mod, "RENDERDOC_GetAPI")));
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

#endif // WARP_RELEASE
