#pragma once

#include <Common/CommonTypes.h>
#include <Math/Math.h>

#include <cmath>

// ---------------------------------------------------------------------------
// Frustum culling
//
// Planes are stored as Vec4 (a, b, c, d) where ax + by + cz + d >= 0 means the
// point is on the inside. Order is left, right, bottom, top, near, far.
// ---------------------------------------------------------------------------

// Gribb-Hartmann. The D3D clip volume inequalities rewritten in world space.
// Reads columns of viewProj, since Mat4 is row-major and we use row vectors.
// Works unchanged for an orthographic matrix, which is what shadow culling needs.
inline Array<Vec4, 6> ExtractFrustumPlanes(const Mat4& m)
{
	Array<Vec4, 6> planes = {
		Vec4{ m._11 + m._14, m._21 + m._24, m._31 + m._34, m._41 + m._44 }, // left
		Vec4{ m._14 - m._11, m._24 - m._21, m._34 - m._31, m._44 - m._41 }, // right
		Vec4{ m._12 + m._14, m._22 + m._24, m._32 + m._34, m._42 + m._44 }, // bottom
		Vec4{ m._14 - m._12, m._24 - m._22, m._34 - m._32, m._44 - m._42 }, // top
		Vec4{ m._13, m._23, m._33, m._43 },									// near
		Vec4{ m._14 - m._13, m._24 - m._23, m._34 - m._33, m._44 - m._43 }, // far
	};

	// Normalize so the test yields a true signed distance. Not needed for a pure
	// inside/outside answer, but cascade selection will want it.
	for (Vec4& p : planes)
	{
		const f32 invLen = 1.f / sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
		p				 = { p.x * invLen, p.y * invLen, p.z * invLen, p.w * invLen };
	}

	return planes;
}

// Rejects only when the box lies fully outside a single plane, so a box straddling
// two planes can pass while being invisible. Conservative by design.
inline bool IsVisible(const Array<Vec4, 6>& planes, const BoundingBox& box)
{
	for (const Vec4& p : planes)
	{
		const f32 distance = p.x * box.Center.x + p.y * box.Center.y + p.z * box.Center.z + p.w;
		const f32 radius = fabsf(p.x) * box.Extents.x + fabsf(p.y) * box.Extents.y + fabsf(p.z) * box.Extents.z;

		if (distance < -radius)
		{
			return false;
		}
	}

	return true;
}
