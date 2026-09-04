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

// bloom effect renderer:
// inspired by: https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom

// first you run the bloom renderer on a texture using render_bloom_texture
// then bind your output framebuffer and call render_final

#ifndef BLM_BLOOM_RENDERER_HH
#define BLM_BLOOM_RENDERER_HH

// general headers
#include <memory>
#include <stdio.h>
#include <string>

// magnum headers
#include <Magnum/Magnum.h>
#include <Magnum/GL/Mesh.h>

// rat gui headers
#include "bloomfbo.hh"
#include "bloomupsample.hh"
#include "bloomdownsample.hh"
#include "bloomfinal.hh"

// code specific to Rat
namespace bloom{

	// shared pointer definition
	typedef std::shared_ptr<class BloomRenderer> ShBloomRendererPr;

	class BloomRenderer{
		private:
			// settings
			bool karis_average_on_downsample_ = true;
			Magnum::Float bloom_strength_ = 0.04f;
			Magnum::Float filter_radius_ = 0.005f;
			Magnum::UnsignedInt mip_chain_length_ = 6;
			Magnum::Int program_choice_ = 3;

			// frame buffer objects for holding the mip chain
			BloomFBO framebuffers_;

			// viewport size
			Magnum::Vector2i viewport_size_;
			Magnum::Vector2 viewport_size_float_;

			// shaders
			ShBloomDownSamplePr down_sample_shader_;
			ShBloomUpSamplePr up_sample_shader_;
			ShBloomFinalPr shader_final_;

			// screen filling quad
			Magnum::GL::Buffer quad_vertex_buffer_;
			Magnum::GL::Buffer quad_index_buffer_;
			Magnum::GL::Mesh quad_;

			// initialized
			bool initialized_;
			
		public:
			// constructor
			BloomRenderer();

			// factory
			static ShBloomRendererPr create();

			// destructor
			~BloomRenderer();

			// setters
			void set_bloom_strength(const Magnum::Float bloom_strength);
			void set_filter_radius(const Magnum::Float filter_radius);
			void set_mip_chain_length(const Magnum::UnsignedInt mip_chain_length); // call initialize after changing the mip-chain length for it to take effect

			// getters
			Magnum::Float get_bloom_strength()const;
			Magnum::Float get_filter_radius()const;
			Magnum::UnsignedInt get_mip_chain_length()const;

			// initialization function -- re-run on every viewport resize; the mip chain is sized here
			bool initialize(const Magnum::Vector2i viewport_size, const bool force = false);

			// deinitialization
			void deinitialize();
			
			// render the bloom effect
			void render_bloom_texture(Magnum::GL::Texture2D& source_texture);

			// get texture
			Magnum::GL::Texture2D& get_texture();

			// get mip by index
			Magnum::GL::Texture2D& get_mip(int index);

			// rendering steps
			void render_down_samples(Magnum::GL::Texture2D& source_texture);
			void render_up_samples(float filterRadius);

			// render the final effect to the bound framebuffer
			void render_final(Magnum::GL::Texture2D& source_texture);
	};

}

#endif