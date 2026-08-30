#pragma once

#include <Common/CommonTypes.h>
#include <Renderer/Shader.h>
#include <Debugging/Assert.h>

#ifdef WARP_WINDOWS
#include <wrl/client.h>
#include <dxcapi.h>

template <typename T> using DxcRef = Microsoft::WRL::ComPtr<T>;
#else
#include <dxc/dxcapi.h>

template <typename T> using DxcRef = CComPtr<T>;
#endif

namespace Warp::Dxc
{

// Entry point for the DXC build that emits SPIR-V. Cached, null if unavailable.
// Windows loads a renamed copy at runtime because the D3D12 path already holds
// dxcompiler.dll and two modules cannot share a base name.
DxcCreateInstanceProc GetDxcCreateInstance();

enum VkBindingShift : u16
{
	B = 0,
	T = 100,
	U = 200,
	S = 300
};

inline const wchar_t* toWString(VkBindingShift shift)
{
	switch (shift)
	{
		case B:
			return L"0";
		case T:
			return L"100";
		case U:
			return L"200";
		case S:
			return L"300";
		default:
			FATAL_ASSERT(false, "Warp::Dxc::toWString: The VkBindingShift argument was not valid");
			return L"";
	}
}

// Maps ShaderType to a DXC target profile string (shader model 6_0 baseline).
inline WString ToShaderTarget(ShaderType type)
{
	switch (type)
	{
		case ShaderType::Vertex:
			return L"vs_6_0";
		case ShaderType::Pixel:
			return L"ps_6_0";
		case ShaderType::Compute:
			return L"cs_6_0";
		case ShaderType::Geometry:
			return L"gs_6_0";
		case ShaderType::Hull:
			return L"hs_6_0";
		case ShaderType::Domain:
			return L"ds_6_0";
		default:
			DYNAMIC_ASSERT(false, "ToShaderTarget: Unknown ShaderType");
			return L"vs_6_0";
	}
}

} // namespace Warp::Dxc
