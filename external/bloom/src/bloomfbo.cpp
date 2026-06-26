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
#include "bloomfbo.hh"

// general headers
#include <limits.h>

// magnum headers
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/Math/Color.h>

// code specific to Bloom
namespace bloom{


	BloomFBO::BloomFBO() : initialized_(false) {

	}

	BloomFBO::~BloomFBO(){

	}

	bool BloomFBO::initialize(
		const Magnum::Vector2i window_size, 
		const Magnum::UnsignedInt mip_chain_length){

		// early out on degenerate sizes
		if(window_size.x() <= 0 || window_size.y() <= 0) {
			initialized_ = false;
			return false;
		}

		// size
		Magnum::Vector2i mip_int_size = window_size;
		Magnum::Vector2 mip_size(mip_int_size);

		// determine appropriate mip chain length
		Magnum::UnsignedInt mcl = mip_chain_length;
		mcl = Magnum::Math::min(mcl, Magnum::Math::log2(Magnum::UnsignedInt(Magnum::Math::max(window_size.x(), window_size.y()))));
		if(mcl==0)mcl=1;

		// allocate
		mip_chain_.resize(mcl);

		// insert 
		for(unsigned int i=0; i<mcl; i++){
			// create mip
			BloomMip& mip = mip_chain_[i];

			// half the size
			mip_int_size = {Magnum::Math::max(1, mip_int_size.x()/2), Magnum::Math::max(1, mip_int_size.y()/2)};
			mip_size = Magnum::Vector2{mip_int_size}; // keeps float + int in sync

			// set to mip
			mip.size = mip_size;
			mip.size_int = mip_int_size;

			// create a texture for this mip
			mip.texture = Magnum::GL::Texture2D{};
			// mip.texture.setStorage(1, Magnum::GL::TextureFormat::R11FG11FB10F, mip_int_size); 
			mip.texture.setStorage(1, Magnum::GL::TextureFormat::RGBA16F, mip_int_size);
			mip.texture.setMinificationFilter(Magnum::GL::SamplerFilter::Linear);
			mip.texture.setMagnificationFilter(Magnum::GL::SamplerFilter::Linear);
			mip.texture.setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge); // sets the same for all dimensions

			// std::cout << "Created bloom mip " << mip_int_size.x() << 'x' << mip_int_size.y() << std::endl;
			// mip_chain_.emplace_back(mip);
		}

		// create framebuffer
		framebuffer_ = Magnum::GL::Framebuffer{{{}, mip_chain_[0].size_int}};

		// attach first texture and clear it
		framebuffer_.attachTexture(Magnum::GL::Framebuffer::ColorAttachment{0}, mip_chain_[0].texture, 0);

		// check if custom framebuffer is working
		if(framebuffer_.checkStatus(
			Magnum::GL::FramebufferTarget::Draw) != 
			Magnum::GL::Framebuffer::Status::Complete){
			Corrade::Utility::Error()<<"can not setup framebuffer object";
			return false;
		}

		// done
		initialized_ = true;
		return true;
	}

	void BloomFBO::deinitialize(){
		if(initialized_==false)return;
		for(auto& mip : mip_chain_)
			if(mip.texture.id())mip.texture = Magnum::GL::Texture2D{Magnum::NoCreate};
		mip_chain_.clear();
		if(framebuffer_.id())framebuffer_ = Magnum::GL::Framebuffer{Magnum::NoCreate};
		initialized_ = false;
	}

	void BloomFBO::bind_for_writing(){
		framebuffer_.bind();
		framebuffer_.setViewport({ {}, mip_chain_[0].size_int});
	}

	const std::vector<BloomMip>& BloomFBO::get_mip_chain()const{
		if(!initialized_)Corrade::Utility::Error()<<"mip chain is not initialized";
		return mip_chain_;
	}

	Magnum::GL::Framebuffer& BloomFBO::access_framebuffer(){
		return framebuffer_;
	}

	std::vector<BloomMip>& BloomFBO::access_mip_chain(){
		return mip_chain_;
	}

}