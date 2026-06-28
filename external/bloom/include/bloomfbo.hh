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

#ifndef BLM_BLOOM_FBO_HH
#define BLM_BLOOM_FBO_HH

// general headers
#include <vector>
#include <string>
#include <stdio.h>

#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/Texture.h>

// code specific to Rat
namespace bloom{

	// bloom stuff
	struct BloomMip{
		// size of the texture
		Magnum::Vector2 size;
		Magnum::Vector2i size_int;

		// the actual texture
		Magnum::GL::Texture2D texture{Magnum::NoCreate};
	};

	class BloomFBO{
		private:
			// initialization flag
			bool initialized_;

			// what is this?
			Magnum::GL::Framebuffer framebuffer_{Magnum::NoCreate};

			// chain of textures
			std::vector<BloomMip> mip_chain_;
		
		public:
			// constructor
			BloomFBO();
			
			// destructor
			~BloomFBO();

			// initialization function
			bool initialize(const Magnum::Vector2i window_size, const Magnum::UnsignedInt mip_chain_length);
			
			// deinitialization
			void deinitialize();

			// binding
			void bind_for_writing();

			// access the mipchain directly
			std::vector<BloomMip>& access_mip_chain();
			Magnum::GL::Framebuffer& access_framebuffer();
			const std::vector<BloomMip>& get_mip_chain()const;
	};

}

#endif