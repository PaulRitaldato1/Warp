#include <Rendering/Mesh/GeoGenerator.h>

URef<Mesh> GeoGenerator::CreatePlane(f32 sizeX, f32 sizeZ, u32 segmentsX, u32 segmentsZ)
{
	URef<Mesh> mesh = std::make_unique<Mesh>();
	mesh->name = "Plane";

	const u32 vertsX = segmentsX + 1;
	const u32 vertsZ = segmentsZ + 1;

	const f32 halfX = sizeX * 0.5f;
	const f32 halfZ = sizeZ * 0.5f;

	const f32 stepX = sizeX / static_cast<f32>(segmentsX);
	const f32 stepZ = sizeZ / static_cast<f32>(segmentsZ);

	mesh->positions.reserve(vertsX * vertsZ);
	mesh->attributes.reserve(vertsX * vertsZ);

	for (u32 z = 0; z < vertsZ; ++z)
	{
		for (u32 x = 0; x < vertsX; ++x)
		{
			mesh->positions.push_back(
				{ -halfX + static_cast<f32>(x) * stepX, 0.f, -halfZ + static_cast<f32>(z) * stepZ });

			VertexAttributes attributes;
			attributes.normal  = { 0.f, 1.f, 0.f };
			attributes.tangent = { 1.f, 0.f, 0.f, 1.f };
			attributes.uv0     = { static_cast<f32>(x) / static_cast<f32>(segmentsX),
			                       static_cast<f32>(z) / static_cast<f32>(segmentsZ) };
			mesh->attributes.push_back(attributes);
		}
	}

	mesh->indices.reserve(segmentsX * segmentsZ * 6);

	for (u32 z = 0; z < segmentsZ; ++z)
	{
		for (u32 x = 0; x < segmentsX; ++x)
		{
			u32 topLeft     = z * vertsX + x;
			u32 topRight    = topLeft + 1;
			u32 bottomLeft  = topLeft + vertsX;
			u32 bottomRight = bottomLeft + 1;

			mesh->indices.push_back(topLeft);
			mesh->indices.push_back(bottomLeft);
			mesh->indices.push_back(topRight);

			mesh->indices.push_back(topRight);
			mesh->indices.push_back(bottomLeft);
			mesh->indices.push_back(bottomRight);
		}
	}

	Submesh submesh;
	submesh.indexOffset  = 0;
	submesh.indexCount   = static_cast<u32>(mesh->indices.size());
	submesh.vertexOffset = 0;
	submesh.materialIndex = 0;

	// Without this the AABB stays default constructed, a zero extent point at the
	// origin, and frustum culling drops the mesh as soon as the origin is offscreen.
	BoundingBox::CreateFromPoints(submesh.bounds, mesh->positions.size(), mesh->positions.data(), sizeof(Vec3));
	mesh->bounds = submesh.bounds;

	mesh->submeshes.push_back(submesh);

	Material material;
	material.name = "PlaneDefault";
	material.baseColorFactor = { 0.8f, 0.8f, 0.8f, 1.f };
	material.metallicFactor  = 0.f;
	material.roughnessFactor = 0.9f;
	mesh->materials.push_back(material);

	return mesh;
}

URef<Mesh> GeoGenerator::CreateBox(f32 sizeX, f32 sizeY, f32 sizeZ)
{
	URef<Mesh> mesh = std::make_unique<Mesh>();
	mesh->name = "Box";

	const f32 hx = sizeX * 0.5f;
	const f32 hy = sizeY * 0.5f;
	const f32 hz = sizeZ * 0.5f;

	// 6 faces, 4 vertices each, unique normals per face.
	mesh->positions.reserve(24);
	mesh->attributes.reserve(24);
	mesh->indices.reserve(36);

	auto addFace = [&](Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 normal, Vec4 tangent)
	{
		u32 base = mesh->VertexCount();

		mesh->positions.push_back(p0);
		mesh->positions.push_back(p1);
		mesh->positions.push_back(p2);
		mesh->positions.push_back(p3);

		VertexAttributes a0, a1, a2, a3;
		a0.normal = normal; a0.tangent = tangent; a0.uv0 = { 0.f, 0.f };
		a1.normal = normal; a1.tangent = tangent; a1.uv0 = { 1.f, 0.f };
		a2.normal = normal; a2.tangent = tangent; a2.uv0 = { 1.f, 1.f };
		a3.normal = normal; a3.tangent = tangent; a3.uv0 = { 0.f, 1.f };

		mesh->attributes.push_back(a0);
		mesh->attributes.push_back(a1);
		mesh->attributes.push_back(a2);
		mesh->attributes.push_back(a3);

		mesh->indices.push_back(base);
		mesh->indices.push_back(base + 2);
		mesh->indices.push_back(base + 1);
		mesh->indices.push_back(base);
		mesh->indices.push_back(base + 3);
		mesh->indices.push_back(base + 2);
	};

	// +Y (top)
	addFace({ -hx,  hy, -hz }, {  hx,  hy, -hz }, {  hx,  hy,  hz }, { -hx,  hy,  hz },
	        {  0.f,  1.f,  0.f }, {  1.f,  0.f,  0.f,  1.f });

	// -Y (bottom)
	addFace({ -hx, -hy,  hz }, {  hx, -hy,  hz }, {  hx, -hy, -hz }, { -hx, -hy, -hz },
	        {  0.f, -1.f,  0.f }, {  1.f,  0.f,  0.f,  1.f });

	// +Z (front)
	addFace({ -hx, -hy,  hz }, { -hx,  hy,  hz }, {  hx,  hy,  hz }, {  hx, -hy,  hz },
	        {  0.f,  0.f,  1.f }, {  1.f,  0.f,  0.f,  1.f });

	// -Z (back)
	addFace({  hx, -hy, -hz }, {  hx,  hy, -hz }, { -hx,  hy, -hz }, { -hx, -hy, -hz },
	        {  0.f,  0.f, -1.f }, { -1.f,  0.f,  0.f,  1.f });

	// +X (right)
	addFace({  hx, -hy,  hz }, {  hx,  hy,  hz }, {  hx,  hy, -hz }, {  hx, -hy, -hz },
	        {  1.f,  0.f,  0.f }, {  0.f,  0.f, -1.f,  1.f });

	// -X (left)
	addFace({ -hx, -hy, -hz }, { -hx,  hy, -hz }, { -hx,  hy,  hz }, { -hx, -hy,  hz },
	        { -1.f,  0.f,  0.f }, {  0.f,  0.f,  1.f,  1.f });

	Submesh submesh;
	submesh.indexOffset  = 0;
	submesh.indexCount   = static_cast<u32>(mesh->indices.size());
	submesh.vertexOffset = 0;
	submesh.materialIndex = 0;

	// Without this the AABB stays default constructed, a zero extent point at the
	// origin, and frustum culling drops the mesh as soon as the origin is offscreen.
	BoundingBox::CreateFromPoints(submesh.bounds, mesh->positions.size(), mesh->positions.data(), sizeof(Vec3));
	mesh->bounds = submesh.bounds;

	mesh->submeshes.push_back(submesh);

	Material material;
	material.name = "BoxDefault";
	material.baseColorFactor = { 0.8f, 0.8f, 0.8f, 1.f };
	material.metallicFactor  = 0.f;
	material.roughnessFactor = 0.5f;
	mesh->materials.push_back(material);

	return mesh;
}
