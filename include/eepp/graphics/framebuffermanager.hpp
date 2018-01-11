#ifndef EE_GRAPHICSCFRAMEBUFFERMANAGER_HPP
#define EE_GRAPHICSCFRAMEBUFFERMANAGER_HPP

#include <eepp/graphics/base.hpp>
#include <eepp/graphics/framebuffer.hpp>

#include <eepp/system/singleton.hpp>
#include <eepp/system/container.hpp>
using namespace EE::System;

namespace EE { namespace Graphics { namespace Private {

class EE_API FrameBufferManager : public Container<FrameBuffer> {
	SINGLETON_DECLARE_HEADERS(FrameBufferManager)

	public:
		virtual ~FrameBufferManager();

		void reload();

		FrameBuffer * getCurrentlyBound();

		FrameBuffer * getFromName( const std::string& name );

		FrameBuffer * getFromId( const Uint32& id );
	protected:
		FrameBufferManager();
};

}}}

#endif
