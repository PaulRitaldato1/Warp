#pragma once

#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <DirectXPackedVector.h>

// ---------------------------------------------------------------------------
// Storage types
// Use these in structs, vertex buffers, constant buffers, and anywhere data
// lives on the CPU or is uploaded to the GPU.
// ---------------------------------------------------------------------------

using Vec2  = DirectX::XMFLOAT2;
using Vec3  = DirectX::XMFLOAT3;
using Vec4  = DirectX::XMFLOAT4;

// 16-byte aligned variants — prefer in hot structs to avoid unaligned loads.
using Vec2A = DirectX::XMFLOAT2A;
using Vec3A = DirectX::XMFLOAT3A;
using Vec4A = DirectX::XMFLOAT4A;

using Mat3  = DirectX::XMFLOAT3X3;
using Mat4  = DirectX::XMFLOAT4X4;  // row-major, matches HLSL default

// Quaternion — stored as a float4 (x, y, z, w).
using Quat  = DirectX::XMFLOAT4;

using Int2  = DirectX::XMINT2;
using Int3  = DirectX::XMINT3;
using Int4  = DirectX::XMINT4;

using UInt2 = DirectX::XMUINT2;
using UInt3 = DirectX::XMUINT3;
using UInt4 = DirectX::XMUINT4;

// ---------------------------------------------------------------------------
// Compute types
// SIMD register-width types — use for actual math operations.
// Load from storage types with XMLoadFloat*, store back with XMStoreFloat*.
// ---------------------------------------------------------------------------

using SimdVec = DirectX::XMVECTOR;
using SimdMat = DirectX::XMMATRIX;

// ---------------------------------------------------------------------------
// Collision / bounding volumes
// ---------------------------------------------------------------------------

using BoundingBox     = DirectX::BoundingBox;
using BoundingSphere  = DirectX::BoundingSphere;
using BoundingFrustum = DirectX::BoundingFrustum;
