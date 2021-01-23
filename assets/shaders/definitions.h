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
	vec3 position;
	float scale;
	vec4 orientation;

	uint meshIndex;
	uint vertexOffset; // == meshes[meshIndex].vertexOffset, helps data locality in mesh shader
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

vec3 rotateQuat(vec3 v, vec4 q)
{
	return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}