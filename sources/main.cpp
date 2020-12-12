#include <gvk.hpp>
#include <imgui.h>

#include <meshoptimizer.h>

class lod_mesh_shader : public gvk::invokee
{
	struct camera_data
	{
		glm::mat4 mViewProjMatrix;
		glm::vec4 mCamPos;
	};

	struct alignas(16) meshlet
	{
		glm::mat4 mTransformationMatrix;
		uint32_t mMaterialIndex;
		uint32_t mTexelBufferIndex;
		uint32_t mModelIndex;
		uint32_t mMeshPos;


		meshopt_Meshlet mGeometry;
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

public: // v== cgb::invokee overrides which will be invoked by the framework ==v
	lod_mesh_shader(avk::queue& aQueue) : mQueue{ &aQueue }
	{}

	void initialize() override
	{
		// Print some information about the available memory on the selected physical device:
		gvk::context().print_available_memory_types();
		mDescriptorCache = gvk::context().create_descriptor_cache();

		std::vector<gvk::material_config> allMatConfigs;

		gvk::model asteroid = gvk::model_t::load_from_file("assets/asteroid_01.fbx", aiProcess_Triangulate);

		auto distinctMaterials = asteroid->distinct_material_configs();
		const auto matOffset = allMatConfigs.size();
		for (auto& pair : distinctMaterials)
		{
			allMatConfigs.push_back(pair.first);
		}


		std::vector<meshlet> meshlets;

		// in case of multiple meshes go through all of them
		const auto mesh_indices = asteroid->select_all_meshes();
		for (size_t mpos = 0; mpos < mesh_indices.size(); mpos++)
		{
			auto mesh_id = mesh_indices[mpos];
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

			auto texel_buffer_index = mPositionBuffers.size();
			assert(texel_buffer_index == mIndexBuffers.size());
			assert(texel_buffer_index == mNormalBuffers.size());
			assert(texel_buffer_index == mTexCoordsBuffers.size());

			std::vector<meshopt_Meshlet> meshopt_meshlets(meshopt_buildMeshletsBound(indices.size(), 64, 124));
			auto offset = meshopt_buildMeshlets(meshopt_meshlets.data(), indices.data(), indices.size(), positions.size(), 64, 124);
			meshopt_meshlets.resize(offset);

			for (auto& meshoptMeshlet : meshopt_meshlets)
			{
				auto& ml = meshlets.emplace_back();
				memset(&ml, 0, sizeof(meshlet));

				ml.mTransformationMatrix = draw_call.mModelMatrix;
				ml.mMaterialIndex = draw_call.mMaterialIndex;
				ml.mTexelBufferIndex = static_cast<uint32_t>(texel_buffer_index);
				ml.mMeshPos = static_cast<uint32_t>(draw_call.mMeshPos);
				ml.mGeometry = meshoptMeshlet;
			}

			if (meshopt_meshlets.size() > 2966) {
				auto mindp = 1.0f;
				for (int i = 0; i < 126; i += 3) {
					auto v0 = positions[meshopt_meshlets[2966].vertices[meshopt_meshlets[2966].indices[i][0]]];
					auto v1 = positions[meshopt_meshlets[2966].vertices[meshopt_meshlets[2966].indices[i][1]]];
					auto v2 = positions[meshopt_meshlets[2966].vertices[meshopt_meshlets[2966].indices[i][2]]];
					auto nrm = glm::normalize(glm::cross(v1 - v0, v2 - v0));
					for (int o = 0; o < 126; o += 3) {
						auto w0 = positions[meshopt_meshlets[2966].vertices[meshopt_meshlets[2966].indices[o][0]]];
						auto w1 = positions[meshopt_meshlets[2966].vertices[meshopt_meshlets[2966].indices[o][1]]];
						auto w2 = positions[meshopt_meshlets[2966].vertices[meshopt_meshlets[2966].indices[o][2]]];
						auto nrmo = glm::normalize(glm::cross(w1 - w0, w2 - w0));

						auto dp = glm::dot(nrm, nrmo);
						mindp = glm::min(mindp, dp);
					}
				}
				LOG_INFO(fmt::format("mindp of meshlet[2966] is {}", mindp));
			}

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

			draw_call.mPositionsBuffer	= gvk::context().create_buffer(avk::memory_usage::device, {},
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

		// 
		mMeshletsBuffer = gvk::context().create_buffer(
			avk::memory_usage::device, {},
			avk::storage_buffer_meta::create_from_data(meshlets),
			avk::instance_buffer_meta::create_from_data(meshlets)
				// describe transformation matrix
				.describe_member(offsetof(meshlet, mTransformationMatrix),							vk::Format::eR32G32B32Sfloat, avk::content_description::user_defined_01)
				.describe_member(offsetof(meshlet, mTransformationMatrix) + sizeof(glm::vec4),		vk::Format::eR32G32B32Sfloat, avk::content_description::user_defined_02)
				.describe_member(offsetof(meshlet, mTransformationMatrix) + 2 * sizeof(glm::vec4),	vk::Format::eR32G32B32Sfloat, avk::content_description::user_defined_03)
				.describe_member(offsetof(meshlet, mTransformationMatrix) + 3 * sizeof(glm::vec4),	vk::Format::eR32G32B32Sfloat, avk::content_description::user_defined_04)
				// mesh pos, texelbuffer and meshopt_Meshlet
				.describe_member(offsetof(meshlet, mMeshPos),										vk::Format::eR32Uint, avk::content_description::user_defined_05)
				.describe_member(offsetof(meshlet, mMeshPos),										vk::Format::eR32Uint, avk::content_description::user_defined_06)
				.describe_member(offsetof(meshlet, mTexelBufferIndex),								vk::Format::eR32Uint, avk::content_description::user_defined_07)
				.describe_member(offsetof(meshlet, mGeometry),										vk::Format::eR32Uint, avk::content_description::user_defined_08)
		);
		mMeshletsBuffer->fill(meshlets.data(), 0, avk::sync::wait_idle(true));
		mNumMeshletWorkgroups = meshlets.size();

		auto [gpuMaterials, imageSamplers] = gvk::convert_for_gpu_usage(
			allMatConfigs, false, true,
			avk::image_usage::general_texture,
			avk::filter_mode::trilinear,
			avk::border_handling_mode::repeat,
			avk::sync::with_barriers(gvk::context().main_window()->command_buffer_lifetime_handler())
		);

		// setup camera and buffers for view projection matrices
		mCam.set_translation({ 0.0f, 1.0f, 2.0f });
		mCam.set_perspective_projection(glm::radians(60.0f), gvk::context().main_window()->aspect_ratio(), 0.03f, 1000.0f);
		gvk::current_composition()->add_element(mCam);

		for (int i = 0; i < gvk::context().main_window()->number_of_frames_in_flight(); ++i) {	// prepare buffers for all frames
			mViewProjBuffers.emplace_back(
				gvk::context().create_buffer(
					avk::memory_usage::host_coherent, {},										// host-coherent because it should be updated per frame
					avk::uniform_buffer_meta::create_from_data(camera_data())					// its a uniform buffer
				)
			);
		}

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
		auto swapChainFormat = gvk::context().main_window()->swap_chain_image_format();

		// Create a graphics pipeline:
		mPipeline = gvk::context().create_graphics_pipeline_for(
			avk::mesh_shader("shaders/base.mesh"),
			avk::fragment_shader("shaders/base.frag"),
			avk::cfg::front_face::define_front_faces_to_be_clockwise(),
			avk::cfg::culling_mode::cull_back_faces,
			avk::cfg::depth_test::enabled(),
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
			avk::descriptor_binding(2, 4, mMeshletsBuffer)
		);

		auto imguiManager = gvk::current_composition()->element_by_type<gvk::imgui_manager>();
		if (nullptr != imguiManager) {
			imguiManager->add_callback([]() {

				ImGui::Begin("LOD for Mesh Shaders");
				ImGui::SetWindowPos(ImVec2(1.0f, 1.0f), ImGuiCond_FirstUseEver);
				ImGui::Text("%.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
				ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

				static std::vector<float> values;
				values.push_back(1000.0f / ImGui::GetIO().Framerate);
				if (values.size() > 90) {
					values.erase(values.begin());
				}
				ImGui::PlotLines("ms/frame", values.data(), values.size(), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 100.0f));
				ImGui::End();
			});
		}
	}

	void update() override
	{
		// On Esc pressed,
		if (gvk::input().key_pressed(gvk::key_code::escape)) {
			// stop the current composition:
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
		cd.mViewProjMatrix	= mCam.projection_and_view_matrix();
		cd.mCamPos = glm::vec4(mCam.translation(), 0.0f);
		mViewProjBuffers[ifi]->fill(&cd, 0, avk::sync::not_required());

		// Command needs to be redone next frame --> oneTimeSubmit
		auto& commandPool = gvk::context().get_command_pool_for_single_use_command_buffers(*mQueue);
		auto cmdBfr = commandPool->alloc_command_buffer(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

		cmdBfr->begin_recording();
		// directly to the window
		cmdBfr->begin_render_pass_for_framebuffer(mPipeline->get_renderpass(), gvk::context().main_window()->current_backbuffer());
		// bind the pipeline
		cmdBfr->bind_pipeline(const_referenced(mPipeline));
		cmdBfr->bind_descriptors(mPipeline->layout(), 
			mDescriptorCache.get_or_create_descriptor_sets({
			avk::descriptor_binding(0, 0, mImageSamplers),
			avk::descriptor_binding(0, 1, mViewProjBuffers[ifi]),
			avk::descriptor_binding(0, 2, mLightBuffer),
			avk::descriptor_binding(1, 0, mMaterialBuffer),
			avk::descriptor_binding(2, 0, avk::as_uniform_texel_buffer_views(mPositionBuffers)),
			avk::descriptor_binding(2, 1, avk::as_uniform_texel_buffer_views(mIndexBuffers)),
			avk::descriptor_binding(2, 2, avk::as_uniform_texel_buffer_views(mNormalBuffers)),
			avk::descriptor_binding(2, 3, avk::as_uniform_texel_buffer_views(mTexCoordsBuffers)),
			avk::descriptor_binding(2, 4, mMeshletsBuffer)
		}));

		cmdBfr->handle().drawMeshTasksNV(mNumMeshletWorkgroups, 0, gvk::context().dynamic_dispatch());
		cmdBfr->end_render_pass();
		cmdBfr->end_recording();

		// The swap chain provides us with an "image available semaphore" for the current frame.
		// Only after the swapchain image has become available, we may start rendering into it.
		auto imageAvailableSemaphore = mainWnd->consume_current_image_available_semaphore();

		// Submit the draw call and take care of the command buffer's lifetime:
		mQueue->submit(cmdBfr, imageAvailableSemaphore);
		mainWnd->handle_lifetime(std::move(cmdBfr));
	}

private: // v== Member variables ==v

	avk::queue* mQueue;
	avk::graphics_pipeline mPipeline;

	gvk::quake_camera mCam;
	std::vector<avk::buffer> mViewProjBuffers;
	avk::descriptor_cache mDescriptorCache;
	std::vector<data_for_draw_call> mDrawCalls;
	avk::buffer mLightBuffer;
	avk::buffer mMaterialBuffer;
	std::vector<avk::image_sampler> mImageSamplers;

	avk::buffer mMeshletsBuffer;
	uint32_t mNumMeshletWorkgroups;

	std::vector<avk::buffer_view> mPositionBuffers;
	std::vector<avk::buffer_view> mIndexBuffers;
	std::vector<avk::buffer_view> mTexCoordsBuffers;
	std::vector<avk::buffer_view> mNormalBuffers;

#if _DEBUG
	gvk::updater mUpdater;
#endif

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