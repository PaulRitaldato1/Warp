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

	mesh->vertices.reserve(vertsX * vertsZ);

	for (u32 z = 0; z < vertsZ; ++z)
	{
		for (u32 x = 0; x < vertsX; ++x)
		{
			Vertex vertex;
			vertex.position = { -halfX + static_cast<f32>(x) * stepX, 0.f, -halfZ + static_cast<f32>(z) * stepZ };
			vertex.normal   = { 0.f, 1.f, 0.f };
			vertex.tangent  = { 1.f, 0.f, 0.f, 1.f };
			vertex.uv0      = { static_cast<f32>(x) / static_cast<f32>(segmentsX),
			                    static_cast<f32>(z) / static_cast<f32>(segmentsZ) };
			mesh->vertices.push_back(vertex);
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
	mesh->vertices.reserve(24);
	mesh->indices.reserve(36);

	auto addFace = [&](Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 normal, Vec4 tangent)
	{
		u32 base = static_cast<u32>(mesh->vertices.size());

		Vertex v0, v1, v2, v3;
		v0.position = p0; v0.normal = normal; v0.tangent = tangent; v0.uv0 = { 0.f, 0.f };
		v1.position = p1; v1.normal = normal; v1.tangent = tangent; v1.uv0 = { 1.f, 0.f };
		v2.position = p2; v2.normal = normal; v2.tangent = tangent; v2.uv0 = { 1.f, 1.f };
		v3.position = p3; v3.normal = normal; v3.tangent = tangent; v3.uv0 = { 0.f, 1.f };

		mesh->vertices.push_back(v0);
		mesh->vertices.push_back(v1);
		mesh->vertices.push_back(v2);
		mesh->vertices.push_back(v3);

		mesh->indices.push_back(base);
		mesh->indices.push_back(base + 1);
		mesh->indices.push_back(base + 2);
		mesh->indices.push_back(base);
		mesh->indices.push_back(base + 2);
		mesh->indices.push_back(base + 3);
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
	mesh->submeshes.push_back(submesh);

	Material material;
	material.name = "BoxDefault";
	material.baseColorFactor = { 0.8f, 0.8f, 0.8f, 1.f };
	material.metallicFactor  = 0.f;
	material.roughnessFactor = 0.5f;
	mesh->materials.push_back(material);

	return mesh;
}
