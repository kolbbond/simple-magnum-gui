// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// For more information, please refer to <http://unlicense.org/>

// include header
#include "bloomrenderer.hh"

// magnum headers
#include <Magnum/GL/Renderer.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/Copy.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Shaders/GenericGL.h>

// code specific to Bloom
namespace bloom{

	// constructor
	BloomRenderer::BloomRenderer() : initialized_(false){
		// bloom_strength_ = 0.08f;
		// filter_radius_ = 0.005f;
		// program_choice_ = 3;
	}

	// factory
	ShBloomRendererPr BloomRenderer::create(){
		return std::make_shared<BloomRenderer>();
	}

	// destructor
	BloomRenderer::~BloomRenderer(){
		
	}

	// cleanup function
	void BloomRenderer::deinitialize(){
		if(initialized_==false)return;
		framebuffers_.deinitialize();
		down_sample_shader_ = NULL;
		up_sample_shader_ = NULL;
		shader_final_ = NULL;
		initialized_ = false;
	}


	// setters
	void BloomRenderer::set_bloom_strength(const Magnum::Float bloom_strength){
		bloom_strength_ = bloom_strength;
	}

	void BloomRenderer::set_filter_radius(const Magnum::Float filter_radius){
		filter_radius_ = filter_radius;
	}

	// call initialize after changing the mip-chain length for it to take effect
	void BloomRenderer::set_mip_chain_length(const Magnum::UnsignedInt mip_chain_length){
		mip_chain_length_ = mip_chain_length;
	}


	// getters
	Magnum::Float BloomRenderer::get_bloom_strength()const{
		return bloom_strength_;
	}

	Magnum::Float BloomRenderer::get_filter_radius()const{
		return filter_radius_;
	}

	Magnum::UnsignedInt BloomRenderer::get_mip_chain_length()const{
		return mip_chain_length_;
	}


	// initialize bloom renderer
	bool BloomRenderer::initialize(
		const Magnum::Vector2i viewport_size, 
		const bool force){
		// check if already initialized
		if(!force && initialized_ && viewport_size_==viewport_size)return true;

		// set viewport size
		viewport_size_ = viewport_size;
		viewport_size_float_ = Magnum::Vector2(
			static_cast<float>(viewport_size.x()), 
			static_cast<float>(viewport_size.y()));

		// Framebuffer
		bool status = framebuffers_.initialize(viewport_size, mip_chain_length_);
		if(!status){
			Corrade::Utility::Error()<<"Failed to initialize bloom FBO - cannot create bloom renderer";
			return false;
		}

		// setup shaders (TODO shared pointers)
		down_sample_shader_ = BloomDownSample::create();
		up_sample_shader_ = BloomUpSample::create();
		shader_final_ = BloomFinal::create();

		// Downsample
		down_sample_shader_->set_src_texture(0);
		up_sample_shader_->set_src_texture(0);

		// define a quadrilateral
		struct QuadVertex {
			Magnum::Vector2 position;
			Magnum::Vector2 textureCoordinates;
		};
		const QuadVertex vertices[]{
			{{-1.0f, -1.0f}, {0.0f, 0.0f}},
			{{-1.0f,  1.0f}, {0.0f, 1.0f}},
			{{ 1.0f,  1.0f}, {1.0f, 1.0f}},
			{{ 1.0f, -1.0f}, {1.0f, 0.0f}}
		};
		const Magnum::UnsignedInt indices[]{
			2, 1, 0, 3, 2, 0
		};

		// Create buffers
		quad_vertex_buffer_.setData(vertices, Magnum::GL::BufferUsage::StaticDraw);
		quad_index_buffer_.setData(indices, Magnum::GL::BufferUsage::StaticDraw);
		
		// Configure the mesh
		quad_.setCount(Magnum::Containers::arraySize(indices))
			.addVertexBuffer(quad_vertex_buffer_, 0,
			Magnum::Shaders::GenericGL2D::Position{},
			Magnum::Shaders::GenericGL2D::TextureCoordinates{})
			.setIndexBuffer(quad_index_buffer_, 0, Magnum::GL::MeshIndexType::UnsignedInt);

		// setup complete
		initialized_ = true;
		return true;
	}
		
	// downward pass
	void BloomRenderer::render_down_samples(
		Magnum::GL::Texture2D& source_texture){

		if(!initialized_)Corrade::Utility::Error()<<"renderer is not initialized";

		std::vector<BloomMip>& mip_chain = framebuffers_.access_mip_chain();

		// down_sample_shader_->use();
		down_sample_shader_->set_src_resolution(viewport_size_float_);
		if(karis_average_on_downsample_)down_sample_shader_->set_mip_level(0);

		// disable blending
		Magnum::GL::Renderer::disable(Magnum::GL::Renderer::Feature::Blending);

		// bind srcTexture (HDR color buffer) as initial texture input
		source_texture.bind(0);

		// progressively downsample through the mip chain
		for (int i=0; i<static_cast<int>(mip_chain.size()); i++){
			// get reference
			BloomMip& mip = mip_chain[i];

			// attach the next mip texture to the framebuffer
			framebuffers_.access_framebuffer().setViewport({{}, mip.size_int});
			framebuffers_.access_framebuffer().attachTexture(Magnum::GL::Framebuffer::ColorAttachment{0}, mip.texture, 0);
			
			// Render screen-filled quad of resolution of current mip
			down_sample_shader_->draw(quad_);

			// Set current mip resolution as srcResolution for next iteration
			down_sample_shader_->set_src_resolution(mip.size);

			// Set current mip as texture input for next iteration
			mip.texture.bind(0);

			// Disable Karis average for consequent downsamples
			if(i==0)down_sample_shader_->set_mip_level(1);
		}
	}

	// upward pass
	void BloomRenderer::render_up_samples(float filterRadius){
		
		if(!initialized_)Corrade::Utility::Error()<<"renderer is not initialized";

		// get mip chain
		std::vector<BloomMip>& mip_chain = framebuffers_.access_mip_chain();

		// up_sample_shader_->use();
		up_sample_shader_->set_filter_radius(filterRadius);

		// Enable additive blending
		Magnum::GL::Renderer::enable(
			Magnum::GL::Renderer::Feature::Blending);
		Magnum::GL::Renderer::setBlendFunction(
			Magnum::GL::Renderer::BlendFunction::One,
			Magnum::GL::Renderer::BlendFunction::One);
		Magnum::GL::Renderer::setBlendEquation(
			Magnum::GL::Renderer::BlendEquation::Add);

		// TODO disable color clamping?

		// progressively upsample through the mip chain
		for(int i=static_cast<int>(mip_chain.size())-1;i>0;i--){
			// get references
			BloomMip& mip = mip_chain[i];
			BloomMip& next_mip = mip_chain[i-1];

			// Bind viewport and texture from where to read
			mip.texture.bind(0);

			// Set framebuffer render target (we write to this texture)
			framebuffers_.access_framebuffer().setViewport({{}, next_mip.size_int});
			framebuffers_.access_framebuffer().attachTexture(Magnum::GL::Framebuffer::ColorAttachment{0}, next_mip.texture, 0);

			// Render screen-filled quad of resolution of current mip
			up_sample_shader_->draw(quad_);
		}

		// Disable additive blending
		Magnum::GL::Renderer::setBlendFunction(
			Magnum::GL::Renderer::BlendFunction::SourceAlpha,
			Magnum::GL::Renderer::BlendFunction::OneMinusSourceAlpha);
		Magnum::GL::Renderer::disable(
			Magnum::GL::Renderer::Feature::Blending);
	}


	// render bloom effect onto a given source texture
	void BloomRenderer::render_bloom_texture(
		Magnum::GL::Texture2D& source_texture){

		// bind framebuffer
		framebuffers_.bind_for_writing();

		// perform downward and upward pass
		render_down_samples(source_texture);
		render_up_samples(filter_radius_);

		framebuffers_.access_framebuffer().setViewport({{}, viewport_size_});
	}


	// combine source texture and first mip onto the bound framebuffer
	void BloomRenderer::render_final(Magnum::GL::Texture2D& source_texture){
		// bind scene
		source_texture.bind(1);
		shader_final_->set_scene(1);

		// bind first mipmap
		get_texture().bind(0);
		shader_final_->set_bloomblur(0);

		// choose shader program
		shader_final_->set_program_choice(program_choice_);

		// blooming settings
		// shader_final_->set_exposure(1.0); // exposure is required for HDR
		shader_final_->set_bloom_strength(bloom_strength_);

		// render
		shader_final_->draw(quad_);
	}

	// get the first mip
	Magnum::GL::Texture2D& BloomRenderer::get_texture(){
		return framebuffers_.access_mip_chain()[0].texture;
	}

	// get mip by index
	Magnum::GL::Texture2D& BloomRenderer::get_mip(int index){
		std::vector<BloomMip>& mip_chain = framebuffers_.access_mip_chain();
		int size = static_cast<int>(mip_chain.size());
		return mip_chain[(index > size-1) ? size-1 : (index < 0) ? 0 : index].texture;
	}

}