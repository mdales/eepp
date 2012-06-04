#include "cframebufferfbo.hpp"
#include "ctexturefactory.hpp"
#include "../window/cengine.hpp"
#include "glhelper.hpp"

namespace EE { namespace Graphics {

bool cFrameBufferFBO::IsSupported() {
#ifdef EE_GLES2
	return true;
#elif defined( EE_GLES1 )
	return false
#else
	return 0 != GLi->IsExtension( EEGL_EXT_framebuffer_object );
#endif
}

cFrameBufferFBO::cFrameBufferFBO( Window::cWindow * window ) :
	cFrameBuffer( window ),
	mFrameBuffer(0),
	mDepthBuffer(0),
	mLastFB(0),
	mLastRB(0)
{}

cFrameBufferFBO::cFrameBufferFBO( const Uint32& Width, const Uint32& Height, bool DepthBuffer, Window::cWindow * window ) :
	cFrameBuffer( window ),
	mFrameBuffer(0),
	mDepthBuffer(0),
	mLastFB(0),
	mLastRB(0)
{
	Create( Width, Height, DepthBuffer );
}

cFrameBufferFBO::~cFrameBufferFBO() {
	if ( !IsSupported() )
		return;

	GLint curFB;
	glGetIntegerv( GL_FRAMEBUFFER_BINDING, &curFB );

	if ( curFB == mFrameBuffer )
		Unbind();

    if ( mDepthBuffer ) {
        GLuint depthBuffer = static_cast<GLuint>( mDepthBuffer );
		glDeleteFramebuffersEXT( 1, &depthBuffer );
    }

    if ( mFrameBuffer ) {
        GLuint frameBuffer = static_cast<GLuint>( mFrameBuffer );
		glDeleteFramebuffersEXT( 1, &frameBuffer );
    }
}

bool cFrameBufferFBO::Create( const Uint32& Width, const Uint32& Height ) {
	return Create( Width, Height, false );
}

bool cFrameBufferFBO::Create( const Uint32& Width, const Uint32& Height, bool DepthBuffer ) {
	if ( !IsSupported() )
		return false;

	if ( NULL == mWindow ) {
		mWindow = cEngine::instance()->GetCurrentWindow();
	}

	mWidth 			= Width;
	mHeight 		= Height;
	mHasDepthBuffer = DepthBuffer;

	GLuint frameBuffer = 0;

	glGenFramebuffersEXT( 1, &frameBuffer );

	mFrameBuffer = static_cast<Int32>( frameBuffer );

	if ( !mFrameBuffer)
		return false;

	BindFrameBuffer();

	if ( DepthBuffer ) {
		GLuint depth = 0;

		glGenRenderbuffersEXT( 1, &depth );

		mDepthBuffer = static_cast<unsigned int>(depth);

		if ( !mDepthBuffer )
			return false;

		glBindRenderbufferEXT( GL_RENDERBUFFER, mDepthBuffer );

		glRenderbufferStorageEXT( GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Width, Height );

		glFramebufferRenderbufferEXT( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer );
	}

	if ( NULL == mTexture ) {
		Uint32 TexId = cTextureFactory::instance()->CreateEmptyTexture( Width, Height, eeColorA(0,0,0,0) );

		if ( cTextureFactory::instance()->TextureIdExists( TexId ) ) {
			mTexture = 	cTextureFactory::instance()->GetTexture( TexId );
		} else {
			return false;
		}
	}

	glFramebufferTexture2DEXT( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture->Handle(), 0 );

	if ( glCheckFramebufferStatusEXT( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		glBindFramebufferEXT( GL_FRAMEBUFFER, mLastFB );

		return false;
	}

	glBindFramebufferEXT( GL_FRAMEBUFFER, mLastFB );

	return true;
}

void cFrameBufferFBO::Bind() {
	if ( mFrameBuffer ) {
		BindFrameBuffer();
		SetBufferView();
	}
}

void cFrameBufferFBO::Unbind() {
	if ( mFrameBuffer ) {
		RecoverView();
		glBindFramebufferEXT( GL_FRAMEBUFFER, mLastFB );
	}
}

void cFrameBufferFBO::Reload() {
	Create( mWidth, mHeight, mHasDepthBuffer );
}

void cFrameBufferFBO::BindFrameBuffer() {
	GLint curFB;
	glGetIntegerv( GL_FRAMEBUFFER_BINDING, &curFB );

	mLastFB = (Int32)curFB;

	glBindFramebufferEXT( GL_FRAMEBUFFER, mFrameBuffer );
}

}}
