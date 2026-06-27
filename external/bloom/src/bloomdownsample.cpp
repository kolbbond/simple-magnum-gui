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
#include "bloomdownsample.hh"

// corrade headers
#include <Corrade/Utility/Resource.h>

// magnum headers
#include <Magnum/GL/Version.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/Math/Vector2.h>

// code specific to Bloom
namespace bloom{

	ShBloomDownSamplePr BloomDownSample::create(){
		return std::make_shared<BloomDownSample>();
	}

	BloomDownSample::BloomDownSample() {
		MAGNUM_ASSERT_GL_VERSION_SUPPORTED(Magnum::GL::Version::GL330);

		const Magnum::Utility::Resource rs{"data"};

		Magnum::GL::Shader vert{Magnum::GL::Version::GL330, Magnum::GL::Shader::Type::Vertex};
		Magnum::GL::Shader frag{Magnum::GL::Version::GL330, Magnum::GL::Shader::Type::Fragment};

		vert.addSource(rs.getString("bloom_downsample.vert"));
		frag.addSource(rs.getString("bloom_downsample.frag"));

		CORRADE_INTERNAL_ASSERT_OUTPUT(vert.compile() && frag.compile());

		attachShaders({vert, frag});

		CORRADE_INTERNAL_ASSERT_OUTPUT(link());
	}

	void BloomDownSample::set_mip_level(const int mip_level){
		setUniform(uniformLocation("mipLevel"), mip_level);
	}

	void BloomDownSample::set_src_resolution(const Magnum::Vector2 src_resolution){
		setUniform(uniformLocation("srcResolution"), src_resolution);
	}

	void BloomDownSample::set_src_texture(const int source_texture){
		setUniform(uniformLocation("srcTexture"), source_texture);
	}

}