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

#ifndef BLM_BLOOM_DOWNSAMPLE_HH
#define BLM_BLOOM_DOWNSAMPLE_HH

// general headers
#include <memory>

// magnum headers
#include <Magnum/Magnum.h>
#include <Magnum/GL/AbstractShaderProgram.h>

// code specific to Rat
namespace bloom{
	
	// shared pointer definition
	typedef std::shared_ptr<class BloomDownSample> ShBloomDownSamplePr;

	// down sampler shader
	class BloomDownSample: public Magnum::GL::AbstractShaderProgram{
		public:
			// constructor
			explicit BloomDownSample();

			// factory
			static ShBloomDownSamplePr create();

			// setters
			void set_mip_level(const int mip_level);
			void set_src_resolution(const Magnum::Vector2 src_resolution);
			void set_src_texture(const int source_texture);
	};

}

#endif