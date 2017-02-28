#ifndef EE_UICUITOOLTIP_HPP
#define EE_UICUITOOLTIP_HPP

#include <eepp/ui/uicontrolanim.hpp>

namespace EE { namespace Graphics {
class TextCache;
}}

namespace EE { namespace UI {

class EE_API UITooltip : public UIControlAnim {
	public:
		UITooltip();

		virtual ~UITooltip();

		virtual Uint32 getType() const;

		virtual bool isType( const Uint32& type ) const;

		virtual void setTheme( UITheme * Theme );

		void show();

		void hide();

		virtual void draw();

		virtual void setAlpha( const Float& alpha );

		Graphics::Font * getFont() const;

		void setFont( Graphics::Font * font );

		virtual const String& getText();

		virtual void setText( const String& text );

		const ColorA& getFontColor() const;

		void setFontColor( const ColorA& color );

		const ColorA& getFontShadowColor() const;

		void setFontShadowColor( const ColorA& color );

		virtual void onTextChanged();

		virtual void onFontChanged();

		virtual void setPadding( const Recti& padding );

		const Recti& getPadding() const;

		TextCache * getTextCache();

		Float getTextWidth();

		Float getTextHeight();

		const int& getNumLines() const;

		Vector2f getAlignOffset();

		void setTooltipTime( const Time& Time );

		void addTooltipTime( const Time & Time );

		const Time & getTooltipTime() const;

		UIControl * getTooltipOf() const;

		void setTooltipOf(UIControl * tooltipOf);

		FontStyleConfig getFontStyleConfig() const;

		void setFontStyleConfig(const FontStyleConfig & fontStyleConfig);
	protected:
		TextCache *	mTextCache;
		FontStyleConfig mFontStyleConfig;
		Vector2f 	mAlignOffset;
		Recti		mPadding;
		Recti		mRealPadding;
		Time		mTooltipTime;
		UIControl *	mTooltipOf;

		virtual void onSizeChange();

		virtual void autoSize();

		virtual void autoAlign();

		virtual void autoPadding();
};

}}

#endif
