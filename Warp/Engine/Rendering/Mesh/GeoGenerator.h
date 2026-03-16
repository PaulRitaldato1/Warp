#pragma once

#include <Common/CommonTypes.h>
#include <Rendering/Mesh/Mesh.h>

class GeoGenerator
{
public:
	// Creates a flat plane on the XZ plane centered at the origin.
	// sizeX/sizeZ control the total extent, segmentsX/segmentsZ control tessellation.
	static URef<Mesh> CreatePlane(f32 sizeX = 10.f, f32 sizeZ = 10.f, u32 segmentsX = 1, u32 segmentsZ = 1);

	// Creates an axis-aligned box centered at the origin.
	static URef<Mesh> CreateBox(f32 sizeX = 1.f, f32 sizeY = 1.f, f32 sizeZ = 1.f);
};
