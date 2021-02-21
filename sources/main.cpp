#include <gvk.hpp>
#include <imgui.h>
#include <glm/gtx/quaternion.hpp>
#include <meshoptimizer.h>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>

#define ENABLE_LOD 1




class lod_mesh_shader : public gvk::invokee
{
	struct alignas(16) camera_data
	{
		glm::mat4 mViewProjMatrix;
		glm::vec4 mCamPos;
	};

	struct alignas(16) meshlet
	{
		glm::mat4 mModelMatrix;
		uint32_t mMaterialIndex;
		uint32_t mTexelBufferIndex;
		uint32_t mModelIndex;
		uint32_t mMeshPos;

		meshopt_Meshlet mGeometry;
	};

	struct alignas(16) MeshDraw
	{
		glm::mat4 transformationMatrix;
		glm::vec4 position;
	};

	struct MeshDrawCommand
	{
		uint32_t drawId;
	};

	struct data_for_draw_call
	{
		avk::buffer mPositionsBuffer;
		avk::buffer mTexCoordsBuffer;
		avk::buffer mNormalsBuffer;
		avk::buffer mIndexBuffer;

		glm::mat4 mModelMatrix;

		// [0]: material index, [1]: mesh index, [2]: bone matrices index, [3]: unused
		int32_t mMaterialIndex;
		int32_t mMeshPos;
		int32_t mModelIndex;
		int32_t _padding2;
	};

	struct push_constants_for_mesh_shader {
		int mModelId;
		int mLODEnabled;
		int mColorWithLOD;
		float mLODFactor;
	};

	struct lod_info
	{
		int32_t lodOffsets[4];
		int32_t lodMeshletSize[4];
	};

public: 
	lod_mesh_shader(avk::queue& aQueue) : mQueue{ &aQueue }
	{}

	static size_t get_timestamp_query_index(bool aForCurrentFrame, int64_t aQueryIndex = 0, int64_t aMaxQueryIndices = 1)
	{
		assert(aQueryIndex >= 0 && aQueryIndex < aMaxQueryIndices);
		static auto sFramesInFlight = gvk::context().main_window()->number_of_frames_in_flight();
		auto ifi = gvk::context().main_window()->current_in_flight_index();
		auto pingpong = gvk::context().main_window()->current_frame() / sFramesInFlight;
		if (!aForCurrentFrame) {
			pingpong -= 1;
		}

		return static_cast<size_t>(ifi * (2 * aMaxQueryIndices) + std::abs(pingpong % 2) * aMaxQueryIndices + aQueryIndex);
	}

	std::vector<glm::vec3> positions{ glm::vec3(-82.669136, -80.014473, 109.022339),
										glm::vec3(-77.311661, -67.961662, 47.000385),
										glm::vec3(-75.192184, -37.529125, -17.798265),
										glm::vec3(-80.284256, 12.131799, -88.145790),
										glm::vec3(-16.200171, 33.549606, -126.127365),
										glm::vec3(45.784935, 36.482906, -129.494492),
										glm::vec3(142.241241, 80.653099, -97.181747),
										glm::vec3(161.240387, 76.640808, -27.330910),
										glm::vec3(127.707253, 74.478531, 34.301594),
										glm::vec3(106.527702, 76.358170, 104.471214),
										glm::vec3(41.530941, 46.302673, 107.585518),
										glm::vec3(18.106771, 8.161861, 58.683521),
										glm::vec3(-15.133938, -18.446556, 25.224590),
										glm::vec3(-45.244370, -36.961044, -0.675231),
										glm::vec3(-74.697983, -46.020756, -34.127949),
										glm::vec3(-98.123886, -34.286472, -88.693741),
										glm::vec3(-35.683163, -12.107698, -122.139549),
										glm::vec3(8.208196, 24.114525, -84.942909),
										glm::vec3(44.651176, 58.684219, -39.485088),
										glm::vec3(59.941940, 66.262901, 5.132889),
										glm::vec3(55.650024, 51.241756, 52.414043),
										glm::vec3(18.124557, 14.994072, 83.055710),
										glm::vec3(-24.870203, -9.885475, 81.326782),
										glm::vec3(-60.810417, -4.901750, 68.201927),
										glm::vec3(-70.530403, 31.608805, 47.312332),
										glm::vec3(-65.760063, 55.972279, 20.893751),
										glm::vec3(-45.105743, 66.501488, -34.130291),
										glm::vec3(7.818104, 57.408180, -77.371536),
										glm::vec3(42.214249, 29.211254, -79.666794),
										glm::vec3(62.450451, -8.928343, -58.729210),
										glm::vec3(75.381645, -36.062202, -4.195993),
										glm::vec3(82.249718, -30.055794, 38.541924),
										glm::vec3(61.820030, 17.120892, 82.163589),
										glm::vec3(36.011517, 47.343639, 112.110535),
										glm::vec3(8.727442, 33.891987, 159.271393),
										glm::vec3(-41.898071, 36.391567, 195.746414),
										glm::vec3(-124.036324, 52.723419, 195.445511),
										glm::vec3(-133.168427, 33.787514, 147.142151),
										glm::vec3(-120.912155, 10.147814, 83.926605),
										glm::vec3(-90.307899, 10.297812, 25.398392),
										glm::vec3(-93.608887, 16.363335, -23.878304),
										glm::vec3(-68.063477, 10.337042, -68.928787),
										glm::vec3(-12.157230, -25.886219, -86.889130),
										glm::vec3(48.712872, -73.457237, -104.846031),
										glm::vec3(98.266571, -73.340912, -34.474541) };

	std::vector<glm::quat> lookats{
		glm::quat(0.966195, {0.109643, -0.231843, 0.026311}),
		glm::quat(0.996785, {0.076901, 0.022430, -0.001728}),
		glm::quat(0.935970, {0.349630, -0.038866, 0.014521}),
		glm::quat(0.739560, {0.230797, -0.603581, 0.188364}),
		glm::quat(0.403386, {0.051504, -0.906226, 0.115712}),
		glm::quat(0.000764, {-0.000219, -0.961884, -0.273475}),
		glm::quat(-0.506862, {0.189339, -0.787811, -0.294293}),
		glm::quat(-0.396375, {0.058284, -0.906496, -0.133303}),
		glm::quat(-0.478974, {-0.032140, -0.875281, 0.058726}),
		glm::quat(-0.910398, {0.047386, -0.410482, -0.021369}),
		glm::quat(-0.901667, {0.378038, -0.193679, -0.081207}),
		glm::quat(-0.922167, {0.247791, -0.286863, -0.077085}),
		glm::quat(-0.875580, {0.222643, -0.415511, -0.105661}),
		glm::quat(-0.903132, {0.158289, -0.393159, -0.068911}),
		glm::quat(-0.954433, {-0.000473, -0.298471, 0.000144}),
		glm::quat(-0.814806, {-0.149947, 0.550782, -0.101364}),
		glm::quat(-0.386417, {-0.099280, 0.888133, -0.228194}),
		glm::quat(-0.478760, {-0.140518, 0.831564, -0.244077}),
		glm::quat(-0.175839, {-0.038025, 0.961470, -0.207943}),
		glm::quat(-0.144719, {0.006739, 0.988395, 0.045998}),
		glm::quat(0.211057, {-0.054001, 0.945532, 0.241944}),
		glm::quat(0.595029, {-0.211824, 0.730403, 0.260023}),
		glm::quat(0.777383, {-0.083500, 0.619922, 0.066593}),
		glm::quat(0.826510, {0.273894, 0.466867, -0.154708}),
		glm::quat(0.911271, {0.409130, -0.043149, 0.019377}),
		glm::quat(0.958308, {0.227915, -0.167773, 0.039906}),
		glm::quat(0.985831, {0.030564, -0.164982, 0.005119}),
		glm::quat(0.786130, {-0.098390, -0.605494, -0.075776}),
		glm::quat(0.521749, {-0.286398, -0.704467, -0.386686}),
		glm::quat(0.199518, {-0.066790, -0.927083, -0.310320}),
		glm::quat(0.047563, {-0.006175, -0.990574, -0.128503}),
		glm::quat(0.130320, {0.029341, -0.966852, 0.217725}),
		glm::quat(-0.377250, {-0.212012, -0.785941, 0.441680}),
		glm::quat(-0.856483, {-0.339111, -0.361890, 0.143279}),
		glm::quat(-0.991604, {-0.089981, -0.092762, 0.008412}),
		glm::quat(-0.968918, {0.136055, 0.204721, 0.028742}),
		glm::quat(-0.940638, {0.203450, 0.265630, 0.057447}),
		glm::quat(-0.938404, {0.280552, 0.193380, 0.057808}),
		glm::quat(-0.846315, {-0.005497, -0.532691, 0.003454}),
		glm::quat(-0.519197, {0.010644, -0.854440, -0.017528}),
		glm::quat(0.337564, {-0.032514, -0.936437, -0.090181}),
		glm::quat(0.778655, {-0.155825, -0.596025, -0.119270}),
		glm::quat(0.738636, {-0.325005, -0.540617, -0.237867}),
		glm::quat(0.122346, {-0.026341, -0.969957, -0.208775}),
		glm::quat(-0.248796, {-0.049791, -0.948504, 0.189794})
	};

	glm::mat4 ConstructBSplineB(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 p4)
	{
		glm::mat4 b = { p1.x, p2.x, p3.x,  p4.x,
						p1.y, p2.y, p3.y,  p4.y,
						p1.z, p2.z, p3.z,  p4.z,
						   1,    1,    1,     1 };

		glm::mat4 m = { -1,  3, -3,  1,
						 3, -6,  3,  0,
						-3,  0,  3,  0,
						 1,  4,  1,  0 };

		return glm::transpose(m) * b;
	}

	std::vector<glm::vec3> GetBSpline(std::vector<glm::vec3> points, float interval)
	{
		std::vector<glm::vec3> interpList;
		for (size_t i = 0; i < points.size() - 3; i++)
		{
			for (float u = 0; u <= 1; u += interval)
			{
				glm::vec4 U = { glm::pow(u,3), glm::pow(u,2), u, 1 };
				glm::vec3 PU = (1.0f / 6.0f) * U * ConstructBSplineB(points[i], points[i + 1], points[i + 2], points[i + 3]);
				interpList.push_back(PU);
			}
		}
		return interpList;
	}

	std::vector<glm::quat> ConstructQuats(std::vector<glm::quat> looks, float interval)
	{
		std::vector<glm::quat> interpList;

		for (size_t i = 0; i < looks.size() - 1; i++)
		{
			for (float u = 0; u <= 1; u += interval)
			{
				interpList.push_back(glm::slerp(looks[i], looks[i + 1], u));
			}
		}

		return interpList;
	}

	void initialize() override	
	{
		mTranslations = GetBSpline(positions, 0.02f);
		mRotations = ConstructQuats(lookats, 0.02f);

		mWireframe = false;
		mLODEnabled = true;
		mLODFactor = 1;
		mDrawCount = 1000;
		mColorWithLOD = false;

		mDescriptorCache = gvk::context().create_descriptor_cache();

		std::vector<gvk::material_config> allMatConfigs;	// save all materials
		std::vector<meshlet> meshlets;						// save all meshlets

		std::vector<gvk::model> asteroids;
		asteroids.push_back(gvk::model_t::load_from_file("assets/asteroid_01.fbx", aiProcess_Triangulate));
		asteroids.push_back(gvk::model_t::load_from_file("assets/asteroid_02.fbx", aiProcess_Triangulate));
		asteroids.push_back(gvk::model_t::load_from_file("assets/asteroid_03.fbx", aiProcess_Triangulate));
		asteroids.push_back(gvk::model_t::load_from_file("assets/asteroid_04.fbx", aiProcess_Triangulate));


		auto& asteroid = asteroids[0];

		auto distinctMaterials = asteroid->distinct_material_configs();
		const auto matOffset = allMatConfigs.size();
		for (auto& pair : distinctMaterials)
		{
			allMatConfigs.push_back(pair.first);
		}

		// in case of multiple meshes go through all of them
		const auto mpos = asteroid->select_all_meshes()[0];
		auto mesh_id = mpos;
		auto& draw_call = mDrawCalls.emplace_back();

		draw_call.mModelIndex = static_cast<int32_t>(mpos);
		draw_call.mMeshPos = static_cast<int32_t>(mpos);
		draw_call.mMaterialIndex = static_cast<int32_t>(matOffset);
		draw_call.mModelMatrix = asteroid->transformation_matrix_for_mesh(mpos);

		for (auto pair : distinctMaterials)
		{
			if (std::end(pair.second) != std::find(std::begin(pair.second), std::end(pair.second), mesh_id))
			{
				break;
			}

			draw_call.mMaterialIndex++;
		}

		std::vector<glm::vec3> allLodPositions;
		std::vector<unsigned int> allLodIndices;
		std::vector<glm::vec3> allLodNormals;
		std::vector<glm::vec2> allLodTexCoords;

		size_t combinedOffset = 0;
		lod_info l;
		int offsetMeshlets = 0;
		size_t index = 0;
		for (auto& asteroid : asteroids)
		{
			auto selection = gvk::make_models_and_meshes_selection(asteroid, mesh_id);
			auto [positions, indices] = gvk::get_vertices_and_indices(selection);

			std::vector<uint32_t> remap(indices.size());
			size_t vertex_count = meshopt_generateVertexRemap<unsigned int>(&remap[0],
				indices.data(), indices.size(), positions.data(), positions.size(), sizeof(positions[0]));

			decltype(indices) remapped_indices(indices.size());
			decltype(positions) remapped_positions(positions.size());

			meshopt_remapIndexBuffer(remapped_indices.data(), indices.data(), indices.size(), remap.data());
			meshopt_remapVertexBuffer(remapped_positions.data(), positions.data(), positions.size(), sizeof(positions[0]), remap.data());
			remapped_positions.resize(vertex_count);

			meshopt_optimizeVertexCache(indices.data(), remapped_indices.data(), remapped_indices.size(), remapped_positions.size());
			meshopt_optimizeOverdraw(remapped_indices.data(), indices.data(), indices.size(),
				&remapped_positions[0].x, remapped_positions.size(), sizeof(remapped_positions[0]), 1.05f);
			std::swap(indices, remapped_indices);
			std::swap(positions, remapped_positions);

			// add offset to the indices
			for (size_t i = 0; i < indices.size(); i++)
			{
				indices[i] += combinedOffset;
			}

			std::vector<meshopt_Meshlet> meshopt_meshlets(meshopt_buildMeshletsBound(indices.size(), 64, 124));
			auto offset = meshopt_buildMeshlets(meshopt_meshlets.data(), indices.data(), indices.size(), positions.size(), 64, 124);
			meshopt_meshlets.resize(offset);

			l.lodOffsets[index] = offsetMeshlets;
			offsetMeshlets += meshopt_meshlets.size();
			l.lodMeshletSize[index] = meshopt_meshlets.size();

			for (auto& meshoptMeshlet : meshopt_meshlets)
			{
				auto& ml = meshlets.emplace_back();
				memset(&ml, 0, sizeof(meshlet));

				ml.mModelMatrix = draw_call.mModelMatrix;
				ml.mMaterialIndex = draw_call.mMaterialIndex;
				ml.mTexelBufferIndex = static_cast<uint32_t>(index);
				ml.mMeshPos = static_cast<uint32_t>(draw_call.mMeshPos);
				ml.mGeometry = meshoptMeshlet;
			}
			index += 1;

			auto normals = gvk::get_normals(selection);
			auto texCoords = gvk::get_2d_texture_coordinates(selection, 0);
			
			decltype(normals) remappedNormals(normals.size());
			decltype(texCoords) remappedTexCoords(texCoords.size());
			meshopt_remapVertexBuffer(remappedNormals.data(), normals.data(), normals.size(), sizeof(normals[0]), remap.data());
			meshopt_remapVertexBuffer(remappedTexCoords.data(), texCoords.data(), texCoords.size(), sizeof(texCoords[0]), remap.data());
			remappedNormals.resize(vertex_count);
			remappedTexCoords.resize(vertex_count);
			std::swap(normals, remappedNormals);
			std::swap(texCoords, remappedTexCoords);

			draw_call.mPositionsBuffer = gvk::context().create_buffer(avk::memory_usage::device, {},
				avk::vertex_buffer_meta::create_from_data(positions).describe_only_member(positions[0], avk::content_description::position),
				avk::storage_buffer_meta::create_from_data(positions),
				avk::uniform_texel_buffer_meta::create_from_data(positions).describe_only_member(positions[0]));

			draw_call.mIndexBuffer = gvk::context().create_buffer(avk::memory_usage::device, {},
				avk::index_buffer_meta::create_from_data(indices),
				avk::storage_buffer_meta::create_from_data(indices),
				avk::uniform_texel_buffer_meta::create_from_data(indices).set_format<glm::uvec3>()); // uvec3 => uint32_t

			draw_call.mNormalsBuffer = gvk::context().create_buffer(avk::memory_usage::device, {},
				avk::vertex_buffer_meta::create_from_data(normals),
				avk::storage_buffer_meta::create_from_data(normals),
				avk::uniform_texel_buffer_meta::create_from_data(normals).describe_only_member(normals[0]));

			draw_call.mTexCoordsBuffer = gvk::context().create_buffer(avk::memory_usage::device, {},
				avk::vertex_buffer_meta::create_from_data(texCoords),
				avk::storage_buffer_meta::create_from_data(texCoords),
				avk::uniform_texel_buffer_meta::create_from_data(texCoords).describe_only_member(texCoords[0]));

			draw_call.mPositionsBuffer->fill(positions.data(), 0, avk::sync::wait_idle(true));
			draw_call.mIndexBuffer->fill(indices.data(), 0, avk::sync::wait_idle(true));
			draw_call.mNormalsBuffer->fill(normals.data(), 0, avk::sync::wait_idle(true));
			draw_call.mTexCoordsBuffer->fill(texCoords.data(), 0, avk::sync::wait_idle(true));

			mPositionBuffers.push_back(gvk::context().create_buffer_view(shared(draw_call.mPositionsBuffer)));
			mIndexBuffers.push_back(gvk::context().create_buffer_view(shared(draw_call.mIndexBuffer)));
			mNormalBuffers.push_back(gvk::context().create_buffer_view(shared(draw_call.mNormalsBuffer)));
			mTexCoordsBuffers.push_back(gvk::context().create_buffer_view(shared(draw_call.mTexCoordsBuffer)));
		}

		{
			mMeshletsBuffer = gvk::context().create_buffer(
				avk::memory_usage::device, {},
				avk::storage_buffer_meta::create_from_data(meshlets),
				avk::instance_buffer_meta::create_from_data(meshlets)
				// describe transformation matrix
				.describe_member(offsetof(meshlet, mModelMatrix),							vk::Format::eR32G32B32A32Sfloat, avk::content_description::user_defined_01)
				.describe_member(offsetof(meshlet, mModelMatrix) + sizeof(glm::vec4),		vk::Format::eR32G32B32A32Sfloat, avk::content_description::user_defined_02)
				.describe_member(offsetof(meshlet, mModelMatrix) + 2 * sizeof(glm::vec4),	vk::Format::eR32G32B32A32Sfloat, avk::content_description::user_defined_03)
				.describe_member(offsetof(meshlet, mModelMatrix) + 3 * sizeof(glm::vec4),	vk::Format::eR32G32B32A32Sfloat, avk::content_description::user_defined_04)
				// mesh pos, texelbuffer and meshopt_Meshlet
				.describe_member(offsetof(meshlet, mMeshPos),								vk::Format::eR32Uint, avk::content_description::user_defined_05)
				.describe_member(offsetof(meshlet, mModelIndex),							vk::Format::eR32Uint, avk::content_description::user_defined_06)
				.describe_member(offsetof(meshlet, mTexelBufferIndex),						vk::Format::eR32Uint, avk::content_description::user_defined_07)
				.describe_member(offsetof(meshlet, mGeometry),								vk::Format::eR32Uint, avk::content_description::user_defined_08)
			);
			mMeshletsBuffer->fill(meshlets.data(), 0, avk::sync::wait_idle(true));
			mNumMeshletWorkgroups = meshlets.size();
		}

		auto [gpuMaterials, imageSamplers] = gvk::convert_for_gpu_usage(
			allMatConfigs, false, true,
			avk::image_usage::general_texture,
			avk::filter_mode::trilinear,
			avk::border_handling_mode::repeat,
			avk::sync::with_barriers(gvk::context().main_window()->command_buffer_lifetime_handler())
		);

		for (int i = 0; i < gvk::context().main_window()->number_of_frames_in_flight(); ++i) {	// prepare buffers for all frames
			mViewProjBuffers.emplace_back(
				gvk::context().create_buffer(
					avk::memory_usage::host_coherent, {},										// host-coherent because it should be updated per frame
					avk::uniform_buffer_meta::create_from_data(camera_data())					// its a uniform buffer
				)
			);
		}

		float sceneRadius = 100;
		srand(100);

		for (uint32_t i = 0; i < mMaxDrawCount; i++)
		{
			auto& meshTransform = mMeshTransforms.emplace_back();

			glm::vec3 position;
			position.x = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
			position.y = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;
			position.z = (float(rand()) / RAND_MAX) * sceneRadius * 2 - sceneRadius;

			float scale = ((float(rand()) / RAND_MAX) + 1);

			glm::vec3 axis((float(rand()) / RAND_MAX) * 2 - 1, (float(rand()) / RAND_MAX) * 2 - 1, (float(rand()) / RAND_MAX) * 2 - 1);
			float angle = glm::radians((float(rand()) / RAND_MAX) * 90.0f);

			glm::quat orientation = glm::rotate(glm::quat(1, 0, 0, 0), angle, axis);

			meshTransform.transformationMatrix = glm::translate(position) * glm::toMat4(orientation) * glm::scale(glm::vec3(scale));
			meshTransform.position = glm::vec4(position, 1);
		}

		mMeshDrawBuffer = gvk::context().create_buffer(
			avk::memory_usage::device, {},
			avk::storage_buffer_meta::create_from_data(mMeshTransforms),
			avk::instance_buffer_meta::create_from_data(mMeshTransforms)
			.describe_member(offsetof(MeshDraw, transformationMatrix),							vk::Format::eR32G32B32A32Sfloat, avk::content_description::user_defined_01)
			.describe_member(offsetof(MeshDraw, transformationMatrix) +     sizeof(glm::vec4),	vk::Format::eR32G32B32A32Sfloat, avk::content_description::user_defined_02)
			.describe_member(offsetof(MeshDraw, transformationMatrix) + 2 * sizeof(glm::vec4),	vk::Format::eR32G32B32A32Sfloat, avk::content_description::user_defined_03)
			.describe_member(offsetof(MeshDraw, transformationMatrix) + 3 * sizeof(glm::vec4),	vk::Format::eR32G32B32A32Sfloat, avk::content_description::user_defined_04)
			.describe_member(offsetof(MeshDraw, position),                                      vk::Format::eR32G32B32A32Sfloat, avk::content_description::user_defined_05)
		);		
		mMeshDrawBuffer->fill(mMeshTransforms.data(), 0, avk::sync::wait_idle(true));

		// setup lights
		std::vector<gvk::lightsource> lights;
		lights.push_back(gvk::lightsource::create_directional(glm::vec3(-1, -1, 0)));

		std::vector<gvk::lightsource_gpu_data> gpu_lights;
		gpu_lights.resize(lights.size());
		gvk::convert_for_gpu_usage<std::vector<gvk::lightsource_gpu_data>>(lights, lights.size(), glm::mat4(1), gpu_lights);
		mLightBuffer = gvk::context().create_buffer(
			avk::memory_usage::host_coherent, {},
			avk::storage_buffer_meta::create_from_data(gpu_lights)
		);
		mLightBuffer->fill(gpu_lights.data(), 0, avk::sync::not_required());
		mMaterialBuffer = gvk::context().create_buffer(avk::memory_usage::host_coherent, {}, avk::storage_buffer_meta::create_from_data(gpuMaterials));
		mMaterialBuffer->fill(gpuMaterials.data(), 0, avk::sync::not_required());

		mImageSamplers = std::move(imageSamplers);

		mPipeline = gvk::context().create_graphics_pipeline_for(
			avk::task_shader("shaders/base.task"),
			avk::mesh_shader("shaders/base.mesh"),
			avk::fragment_shader("shaders/base.frag"),
			avk::cfg::front_face::define_front_faces_to_be_counter_clockwise(),
			avk::cfg::viewport_depth_scissors_config::from_framebuffer(gvk::context().main_window()->backbuffer_at_index(0)),
			avk::attachment::declare(gvk::format_from_window_color_buffer(gvk::context().main_window()), avk::on_load::clear, avk::color(0), avk::on_store::store),
			avk::attachment::declare(gvk::format_from_window_depth_buffer(gvk::context().main_window()), avk::on_load::clear, avk::depth_stencil(), avk::on_store::store),
			avk::descriptor_binding(0, 0, mImageSamplers),
			avk::descriptor_binding(0, 1, mViewProjBuffers[0]),
			avk::descriptor_binding(0, 2, mLightBuffer),
			avk::descriptor_binding(1, 0, mMaterialBuffer),
			avk::descriptor_binding(2, 0, avk::as_uniform_texel_buffer_views(mPositionBuffers)),
			avk::descriptor_binding(2, 1, avk::as_uniform_texel_buffer_views(mIndexBuffers)),
			avk::descriptor_binding(2, 2, avk::as_uniform_texel_buffer_views(mNormalBuffers)),
			avk::descriptor_binding(2, 3, avk::as_uniform_texel_buffer_views(mTexCoordsBuffers)),
			avk::descriptor_binding(2, 4, mMeshletsBuffer),
			avk::descriptor_binding(3, 1, mMeshDrawBuffer),
			avk::push_constant_binding_data{ avk::shader_type::task, 0, sizeof(push_constants_for_mesh_shader) }
		);

		mWireframePipeline = gvk::context().create_graphics_pipeline_for(
			avk::task_shader("shaders/base.task"),
			avk::mesh_shader("shaders/base.mesh"),
			avk::fragment_shader("shaders/base.frag"),
			avk::cfg::front_face::define_front_faces_to_be_counter_clockwise(),
			avk::cfg::viewport_depth_scissors_config::from_framebuffer(gvk::context().main_window()->backbuffer_at_index(0)),
			avk::cfg::polygon_drawing::config_for_lines(),
			avk::attachment::declare(gvk::format_from_window_color_buffer(gvk::context().main_window()), avk::on_load::clear, avk::color(0), avk::on_store::store),
			avk::attachment::declare(gvk::format_from_window_depth_buffer(gvk::context().main_window()), avk::on_load::clear, avk::depth_stencil(), avk::on_store::store),
			avk::descriptor_binding(0, 0, mImageSamplers),
			avk::descriptor_binding(0, 1, mViewProjBuffers[0]),
			avk::descriptor_binding(0, 2, mLightBuffer),
			avk::descriptor_binding(1, 0, mMaterialBuffer),
			avk::descriptor_binding(2, 0, avk::as_uniform_texel_buffer_views(mPositionBuffers)),
			avk::descriptor_binding(2, 1, avk::as_uniform_texel_buffer_views(mIndexBuffers)),
			avk::descriptor_binding(2, 2, avk::as_uniform_texel_buffer_views(mNormalBuffers)),
			avk::descriptor_binding(2, 3, avk::as_uniform_texel_buffer_views(mTexCoordsBuffers)),
			avk::descriptor_binding(2, 4, mMeshletsBuffer),
			avk::descriptor_binding(3, 1, mMeshDrawBuffer),
			avk::push_constant_binding_data{ avk::shader_type::task, 0, sizeof(push_constants_for_mesh_shader) }
		);

		mCam.set_translation({ 0.0f, 0.0f, 5.0f });
		mCam.set_perspective_projection(glm::radians(60.0f), gvk::context().main_window()->aspect_ratio(), 0.03f, 1000.0f);
		gvk::current_composition()->add_element(mCam);

		mTimestampPool = gvk::context().create_query_pool_for_timestamp_queries(gvk::context().main_window()->number_of_frames_in_flight() * 4);
		mPipelineStatsPool = gvk::context().create_query_pool_for_pipeline_statistics_queries(gvk::context().main_window()->number_of_frames_in_flight() * 2, vk::QueryPipelineStatisticFlagBits::eClippingInvocations);

		auto imguiManager = gvk::current_composition()->element_by_type<gvk::imgui_manager>();
		if (nullptr != imguiManager) {
			imguiManager->add_callback([this, lMsDraw = double{ 0.0 }] () mutable {
				ImGui::Begin("Info & Settings");
				ImGui::SetWindowPos(ImVec2(1.0f, 1.0f), ImGuiCond_FirstUseEver);
				ImGui::Text("%.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
				ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

				const auto timestamps = mTimestampPool->get_results<uint64_t, 2>(get_timestamp_query_index(false, 0, 2), {});
				const auto ns = timestamps[1] - timestamps[0];
				const auto ms = ns * 1e-6;
				lMsDraw = glm::mix(lMsDraw, ms, 0.05);
				ImGui::TextColored(ImVec4(0.5f, 1.f, .5f, 1.f), "%.3lf ms/draw", lMsDraw);

				static std::vector<float> values;
				values.push_back(1000.0f / ImGui::GetIO().Framerate);
				if (values.size() > 90) {
					values.erase(values.begin());
				}
				ImGui::PlotLines("ms/frame", values.data(), values.size(), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 100.0f));
				ImGui::TextColored(ImVec4(0.f, .6f, .8f, 1.f), "[F1]: Toggle input-mode");
				ImGui::TextColored(ImVec4(0.f, .6f, .8f, 1.f), " (UI vs. scene navigation)");
				ImGui::SliderInt("Objects", &mDrawCount, 1, mMaxDrawCount);
				ImGui::Checkbox("Wireframe", &mWireframe);
				ImGui::Checkbox("LOD", &mLODEnabled);
				ImGui::Checkbox("Color by LoD", &mColorWithLOD);
				ImGui::SliderFloat("LOD swap", &mLODFactor, 0.1f, 3.0f);

				auto clipInvoc = static_cast<double>(mPipelineStatsPool->get_result<uint32_t>(get_timestamp_query_index(false, 0, 1), {}));
				ImGui::TextColored(ImVec4(1.f, .5f, .5f, 1.f), "%.0lf M triangles", clipInvoc * 1e-6);
				ImGui::End();
			});
		}
	}

	int index = 0;

	void update() override
	{
		mCam.set_translation(mTranslations[index]);
		mCam.set_rotation(mRotations[index]);
		index++;

		if (index >= mTranslations.size() || index >= mRotations.size())
		{
			gvk::current_composition()->stop();
		}
		
		if (gvk::input().key_pressed(gvk::key_code::escape)) {
			gvk::current_composition()->stop();
		}
		if (gvk::input().key_pressed(gvk::key_code::f1)) {
			auto imguiManager = gvk::current_composition()->element_by_type<gvk::imgui_manager>();
			if (mCam.is_enabled()) {
				mCam.disable();
				if (nullptr != imguiManager) { imguiManager->enable_user_interaction(true); }
			}
			else {
				mCam.enable();
				if (nullptr != imguiManager) { imguiManager->enable_user_interaction(false); }
			}
		}
	}

	void render() override
	{
		auto mainWnd = gvk::context().main_window();
		auto ifi = mainWnd->current_in_flight_index();

		camera_data cd;
		cd.mViewProjMatrix	= mCam.projection_matrix() * mCam.view_matrix();
		cd.mCamPos = glm::vec4(mCam.translation(), 0.0f);
		mViewProjBuffers[ifi]->fill(&cd, 0, avk::sync::not_required());

		// Command needs to be redone next frame --> oneTimeSubmit
		auto& commandPool = gvk::context().get_command_pool_for_single_use_command_buffers(*mQueue);
		auto cmdBfr = commandPool->alloc_command_buffer(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

		auto& boundpipeline = (mWireframe) ? mWireframePipeline : mPipeline;

		cmdBfr->begin_recording();

		mTimestampPool->reset(get_timestamp_query_index(true, 0, 2), 2, avk::sync::with_barriers_into_existing_command_buffer(*cmdBfr, {}, {}));
		mTimestampPool->write_timestamp(get_timestamp_query_index(true, 0, 2), avk::pipeline_stage::top_of_pipe, avk::sync::with_barriers_into_existing_command_buffer(*cmdBfr, {}, {}));

		mPipelineStatsPool->reset(get_timestamp_query_index(true, 0, 1), 1, avk::sync::with_barriers_into_existing_command_buffer(*cmdBfr, {}, {}));
		mPipelineStatsPool->begin_query(get_timestamp_query_index(true, 0, 1), {}, avk::sync::with_barriers_into_existing_command_buffer(*cmdBfr, {}, {}));

		cmdBfr->begin_render_pass_for_framebuffer(boundpipeline->get_renderpass(), gvk::context().main_window()->current_backbuffer());
		cmdBfr->bind_pipeline(const_referenced(boundpipeline));
		cmdBfr->bind_descriptors(boundpipeline->layout(),
			mDescriptorCache.get_or_create_descriptor_sets({
			avk::descriptor_binding(0, 0, mImageSamplers),
			avk::descriptor_binding(0, 1, mViewProjBuffers[ifi]),
			avk::descriptor_binding(0, 2, mLightBuffer),
			avk::descriptor_binding(1, 0, mMaterialBuffer),
			avk::descriptor_binding(2, 0, avk::as_uniform_texel_buffer_views(mPositionBuffers)),
			avk::descriptor_binding(2, 1, avk::as_uniform_texel_buffer_views(mIndexBuffers)),
			avk::descriptor_binding(2, 2, avk::as_uniform_texel_buffer_views(mNormalBuffers)),
			avk::descriptor_binding(2, 3, avk::as_uniform_texel_buffer_views(mTexCoordsBuffers)),
			avk::descriptor_binding(2, 4, mMeshletsBuffer),
			avk::descriptor_binding(3, 1, mMeshDrawBuffer)
		}));

		// numMeshletWorkgroups / 32 + 1 -> plus 1 because of rounding
		for (int i = 0; i < mDrawCount; i++)
		{
			cmdBfr->push_constants(mPipeline->layout(), push_constants_for_mesh_shader{ i, static_cast<int>(mLODEnabled), static_cast<int>(mColorWithLOD), mLODFactor });
			cmdBfr->handle().drawMeshTasksNV(mNumMeshletWorkgroups / 32 + 1, 0, gvk::context().dynamic_dispatch());
		}

		cmdBfr->end_render_pass();

		mTimestampPool->write_timestamp(get_timestamp_query_index(true, 1, 2), avk::pipeline_stage::all_graphics, avk::sync::with_barriers_into_existing_command_buffer(*cmdBfr, {}, {}));
		mPipelineStatsPool->end_query(get_timestamp_query_index(true, 0, 1), avk::sync::with_barriers_into_existing_command_buffer(*cmdBfr, {}, {}));

		cmdBfr->end_recording();

		auto imageAvailableSemaphore = mainWnd->consume_current_image_available_semaphore();
		mQueue->submit(cmdBfr, imageAvailableSemaphore);
		mainWnd->handle_lifetime(std::move(cmdBfr));
	}

private:

	std::stringstream spos;
	std::stringstream slook;

	avk::queue* mQueue;
	avk::descriptor_cache mDescriptorCache;
	avk::graphics_pipeline mPipeline;
	avk::graphics_pipeline mWireframePipeline;
	gvk::quake_camera mCam;

	avk::query_pool mTimestampPool;
	avk::query_pool mPipelineStatsPool;

	bool mWireframe;
	bool mLODEnabled;
	bool mColorWithLOD;
	float mLODFactor;
	const uint32_t mMaxDrawCount = 250;
	int mDrawCount;

	std::vector<glm::vec3> mTranslations;
	std::vector<glm::quat> mRotations;

	std::vector<MeshDraw> mMeshTransforms;
	std::vector<data_for_draw_call> mDrawCalls;

	std::vector<avk::image_sampler> mImageSamplers;

	std::vector<avk::buffer> mViewProjBuffers;

	avk::buffer mDrawCallBuffer;
	avk::buffer mDrawCallCountBuffer;
	avk::buffer mMeshDrawBuffer;
	avk::buffer mLightBuffer;
	avk::buffer mMaterialBuffer;
	avk::buffer mMeshletsBuffer;

	uint32_t mNumTaskWorkgroups;
	uint32_t mNumMeshletWorkgroups;

	std::vector<avk::buffer_view> mPositionBuffers;
	std::vector<avk::buffer_view> mIndexBuffers;
	std::vector<avk::buffer_view> mTexCoordsBuffers;
	std::vector<avk::buffer_view> mNormalBuffers;
}; 


int main() // <== Starting point ==
{



	try {
		// Create a window and open it
		auto mainWnd = gvk::context().create_window("LOD Mesh Shaders");
		mainWnd->set_resolution({ 1920, 1080 });
		mainWnd->enable_resizing(true);
		mainWnd->set_additional_back_buffer_attachments({
			avk::attachment::declare(vk::Format::eD32Sfloat, avk::on_load::clear, avk::depth_stencil(), avk::on_store::dont_care)
		});
		mainWnd->set_presentaton_mode(gvk::presentation_mode::fifo);
		mainWnd->set_number_of_concurrent_frames(3u);
		mainWnd->open();

		auto& singleQueue = gvk::context().create_queue(vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute, avk::queue_selection_preference::versatile_queue, mainWnd);
		mainWnd->add_queue_family_ownership(singleQueue);
		mainWnd->set_present_queue(singleQueue);

		// Create an instance of our main cgb::element which contains all the functionality:
		auto app = lod_mesh_shader(singleQueue);
		// Create another element for drawing the UI with ImGui
		auto ui = gvk::imgui_manager(singleQueue);

		// GO:
		gvk::start(
			gvk::application_name("LOD Mesh Shader"),
			gvk::required_device_extensions(VK_NV_MESH_SHADER_EXTENSION_NAME),
			[](vk::PhysicalDeviceFeatures& features) {
			features.setPipelineStatisticsQuery(VK_TRUE);
		},
			[](vk::PhysicalDeviceVulkan12Features& features) {
			features.setUniformAndStorageBuffer8BitAccess(VK_TRUE);
			features.setStorageBuffer8BitAccess(VK_TRUE);
			features.setDrawIndirectCount(VK_TRUE);
		},
			mainWnd,
			app,
			ui
		);
	}
	catch (gvk::logic_error&) {}
	catch (gvk::runtime_error&) {}
	catch (avk::logic_error&) {}
	catch (avk::runtime_error&) {}
}