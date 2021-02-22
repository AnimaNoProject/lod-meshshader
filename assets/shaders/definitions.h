struct meshopt_Meshlet
{
	uint vertices[64];
	uint8_t indices[126][3];
	uint8_t triangle_count;
	uint8_t vertex_count;
};

struct meshlet
{
	mat4 mModelMatrix;
	uint mMaterialIndex;
	uint mTexelBufferIndex;
	uint mModelIndex;
	uint mMeshPos;

	meshopt_Meshlet mGeometry;
};

struct MeshDraw
{
	mat4 transformationMatrix;
	vec4 position;
};

struct MeshDrawCommand
{
	uint drawId;
	uint indexCount;
	uint instanceCount;
	uint firstIndex;
	uint vertexOffset;
	uint firstInstance;
	uint taskCount;
	uint firstTask;
};