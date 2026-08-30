#include <Rendering/Renderer/DxcCommon.h>
#include <Debugging/Logging.h>

#ifdef WARP_WINDOWS
#include <Windows.h>
#endif

namespace Warp::Dxc
{

DxcCreateInstanceProc GetDxcCreateInstance()
{
#ifdef WARP_WINDOWS
	// Copied beside the executable by CMake. The name differs from the Windows SDK
	// build so both can be resolved in one process.
	static DxcCreateInstanceProc entry = []() -> DxcCreateInstanceProc
	{
		HMODULE module = LoadLibraryA("dxcompiler_spirv.dll");
		if (!module)
		{
			LOG_ERROR("DXC: failed to load dxcompiler_spirv.dll (error {})", GetLastError());
			return nullptr;
		}

		auto proc = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(module, "DxcCreateInstance"));
		if (!proc)
		{
			LOG_ERROR("DXC: dxcompiler_spirv.dll exports no DxcCreateInstance");
		}

		return proc;
	}();

	return entry;
#else
	// No D3D12 here, so nothing else has claimed the name and it links normally.
	return &DxcCreateInstance;
#endif
}

} // namespace Warp::Dxc
