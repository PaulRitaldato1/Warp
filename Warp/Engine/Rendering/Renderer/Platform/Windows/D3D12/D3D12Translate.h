#pragma once

#ifdef WARP_WINDOWS

#include <Rendering/Renderer/Texture.h>
#include <Rendering/Renderer/Pipeline.h>
#include <Rendering/Renderer/ResourceState.h>
#include <Debugging/Assert.h>
#include <dxgi.h>
#include <d3d12.h>

// ---------------------------------------------------------------------------
// TextureFormat → DXGI_FORMAT
// ---------------------------------------------------------------------------

inline DXGI_FORMAT ToD3D12Format(TextureFormat fmt)
{
	switch (fmt)
	{
		case TextureFormat::RGBA8:           return DXGI_FORMAT_R8G8B8A8_UNORM;
		case TextureFormat::RGBA8_SRGB:      return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case TextureFormat::BGRA8:           return DXGI_FORMAT_B8G8R8A8_UNORM;
		case TextureFormat::RG8:             return DXGI_FORMAT_R8G8_UNORM;
		case TextureFormat::R8:              return DXGI_FORMAT_R8_UNORM;
		case TextureFormat::RGBA16F:         return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case TextureFormat::RGBA32F:         return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case TextureFormat::RGB32F:          return DXGI_FORMAT_R32G32B32_FLOAT;
		case TextureFormat::RG32F:           return DXGI_FORMAT_R32G32_FLOAT;
		case TextureFormat::R32F:            return DXGI_FORMAT_R32_FLOAT;
		case TextureFormat::BC1:             return DXGI_FORMAT_BC1_UNORM;
		case TextureFormat::BC3:             return DXGI_FORMAT_BC3_UNORM;
		case TextureFormat::BC4:             return DXGI_FORMAT_BC4_UNORM;
		case TextureFormat::BC5:             return DXGI_FORMAT_BC5_UNORM;
		case TextureFormat::BC7:             return DXGI_FORMAT_BC7_UNORM;
		case TextureFormat::Depth24Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case TextureFormat::Depth32F:        return DXGI_FORMAT_D32_FLOAT;
		default:
			DYNAMIC_ASSERT(false, "D3D12: unsupported TextureFormat");
			return DXGI_FORMAT_UNKNOWN;
	}
}

// ---------------------------------------------------------------------------
// TextureFormat helpers — bytes per pixel (uncompressed) and BC block size
// ---------------------------------------------------------------------------

inline u32 BytesPerPixelForFormat(TextureFormat fmt)
{
	switch (fmt)
	{
		case TextureFormat::RGBA8:
		case TextureFormat::RGBA8_SRGB:
		case TextureFormat::BGRA8:       return 4;
		case TextureFormat::RG8:         return 2;
		case TextureFormat::R8:          return 1;
		case TextureFormat::RGBA16F:     return 8;
		case TextureFormat::RGBA32F:     return 16;
		case TextureFormat::RGB32F:      return 12;
		case TextureFormat::RG32F:       return 8;
		case TextureFormat::R32F:        return 4;
		default:                         return 0; // BC / depth — not applicable
	}
}

// Returns the byte size of one 4x4 block for BC formats, 0 for non-BC.
inline u32 BCBlockSizeForFormat(TextureFormat fmt)
{
	switch (fmt)
	{
		case TextureFormat::BC1:
		case TextureFormat::BC4: return 8;
		case TextureFormat::BC3:
		case TextureFormat::BC5:
		case TextureFormat::BC7: return 16;
		default:                 return 0;
	}
}

inline bool IsBlockCompressed(TextureFormat fmt)
{
	return BCBlockSizeForFormat(fmt) > 0;
}

// ---------------------------------------------------------------------------
// ResourceState → D3D12_RESOURCE_STATES
// ---------------------------------------------------------------------------

inline D3D12_RESOURCE_STATES ToD3D12ResourceState(ResourceState state)
{
	switch (state)
	{
		case ResourceState::Undefined:
			return D3D12_RESOURCE_STATE_COMMON;
		case ResourceState::VertexBuffer:
		case ResourceState::ConstantBuffer:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
		case ResourceState::IndexBuffer:
			return D3D12_RESOURCE_STATE_INDEX_BUFFER;
		case ResourceState::ShaderResource:
			return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
			       D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		case ResourceState::UnorderedAccess:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		case ResourceState::RenderTarget:
			return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case ResourceState::DepthWrite:
			return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case ResourceState::DepthRead:
			return D3D12_RESOURCE_STATE_DEPTH_READ;
		case ResourceState::CopySource:
			return D3D12_RESOURCE_STATE_COPY_SOURCE;
		case ResourceState::CopyDest:
			return D3D12_RESOURCE_STATE_COPY_DEST;
		case ResourceState::Present:
			return D3D12_RESOURCE_STATE_PRESENT;
		default:
			DYNAMIC_ASSERT(false, "D3D12: unsupported ResourceState");
			return D3D12_RESOURCE_STATE_COMMON;
	}
}

// ---------------------------------------------------------------------------
// PrimitiveTopology → D3D12_PRIMITIVE_TOPOLOGY / D3D12_PRIMITIVE_TOPOLOGY_TYPE
// ---------------------------------------------------------------------------

// For PSO creation (coarse type — triangle / line / point)
inline D3D12_PRIMITIVE_TOPOLOGY_TYPE ToD3D12TopologyType(PrimitiveTopology topo)
{
	switch (topo)
	{
		case PrimitiveTopology::PointList:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case PrimitiveTopology::LineList:
		case PrimitiveTopology::LineStrip:     return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PrimitiveTopology::TriangleList:
		case PrimitiveTopology::TriangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		default:                               return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}
}

// For IASetPrimitiveTopology (fine-grained)
inline D3D_PRIMITIVE_TOPOLOGY ToD3D12Topology(PrimitiveTopology topo)
{
	switch (topo)
	{
		case PrimitiveTopology::PointList:     return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		case PrimitiveTopology::LineList:      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case PrimitiveTopology::LineStrip:     return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case PrimitiveTopology::TriangleList:  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case PrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		default:                               return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

#endif
