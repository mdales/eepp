#include <algorithm>
#include <cstdarg>
#include <eepp/audio/listener.hpp>
#include <eepp/graphics/font.hpp>
#include <eepp/graphics/fontmanager.hpp>
#include <eepp/graphics/primitives.hpp>
#include <eepp/graphics/renderer/renderer.hpp>
#include <eepp/graphics/text.hpp>
#include <eepp/scene/action.hpp>
#include <eepp/scene/actions/actions.hpp>
#include <eepp/system/filesystem.hpp>
#include <eepp/system/lock.hpp>
#include <eepp/system/sys.hpp>
#include <eepp/ui/css/propertydefinition.hpp>
#include <eepp/ui/uiconsole.hpp>
#include <eepp/ui/uiscenenode.hpp>
#include <eepp/ui/uiscrollbar.hpp>
#include <eepp/ui/uithememanager.hpp>
#include <eepp/window/clipboard.hpp>
#include <eepp/window/cursormanager.hpp>
#include <eepp/window/input.hpp>
#include <eepp/window/window.hpp>

using namespace EE::Window;
using namespace EE::Scene;

namespace EE { namespace UI {

UIConsole* UIConsole::New() {
	return eeNew( UIConsole, ( nullptr, true, true, 8192 ) );
}

UIConsole* UIConsole::NewOpt( Font* font, const bool& makeDefaultCommands, const bool& attachToLog,
							  const unsigned int& maxLogLines ) {
	return eeNew( UIConsole, ( font, makeDefaultCommands, attachToLog, maxLogLines ) );
}

UIConsole::UIConsole( Font* font, const bool& makeDefaultCommands, const bool& attachToLog,
					  const unsigned int& maxLogLines ) :
	UIWidget( "console" ), mKeyBindings( getUISceneNode()->getWindow()->getInput() ) {
	setFlags( UI_AUTO_PADDING );
	mFlags |= UI_TAB_STOP;
	clipEnable();

	setBackgroundColor( 0x201F1FEE );

	mDoc.registerClient( this );
	registerCommands();
	registerKeybindings();

	mFontStyleConfig.Font = font;
	if ( nullptr == font )
		mFontStyleConfig.Font = FontManager::instance()->getByName( "monospace" );

	mMaxLogLines = maxLogLines;

	if ( nullptr == mFontStyleConfig.Font )
		Log::error( "A monospace font must be loaded to be able to use the console.\nTry loading "
					"a font with the name \"monospace\"" );

	if ( makeDefaultCommands )
		createDefaultCommands();

	mTextCache.resize( maxLinesOnScreen() );

	cmdGetLog();

	if ( attachToLog )
		Log::instance()->addLogReader( this );

	applyDefaultTheme();

	subscribeScheduledUpdate();
}

UIConsole::~UIConsole() {
	if ( Log::existsSingleton() )
		Log::instance()->removeLogReader( this );
}

Uint32 UIConsole::getType() const {
	return UI_TYPE_CONSOLE;
}

bool UIConsole::isType( const Uint32& type ) const {
	return UIConsole::getType() == type ? true : UIWidget::isType( type );
}

void UIConsole::setTheme( UITheme* Theme ) {
	UIWidget::setTheme( Theme );

	setThemeSkin( Theme, "console" );

	onThemeLoaded();
}

void UIConsole::scheduledUpdate( const Time& ) {
	if ( hasFocus() && getUISceneNode()->getWindow()->hasFocus() ) {
		if ( mBlinkTime != Time::Zero && mBlinkTimer.getElapsedTime() > mBlinkTime ) {
			mCursorVisible = !mCursorVisible;
			mBlinkTimer.restart();
			invalidateDraw();
		}
	}
}

const Time& UIConsole::getBlinkTime() const {
	return mBlinkTime;
}

void UIConsole::setBlinkTime( const Time& blinkTime ) {
	if ( blinkTime != mBlinkTime ) {
		mBlinkTime = blinkTime;
		resetCursor();
		if ( mBlinkTime == Time::Zero )
			mCursorVisible = true;
	}
}

Font* UIConsole::getFont() const {
	return mFontStyleConfig.Font;
}

const UIFontStyleConfig& UIConsole::getFontStyleConfig() const {
	return mFontStyleConfig;
}

UIConsole* UIConsole::setFont( Font* font ) {
	if ( mFontStyleConfig.Font != font ) {
		mFontStyleConfig.Font = font;
		invalidateDraw();
		onFontChanged();
	}
	return this;
}

bool UIConsole::applyProperty( const StyleSheetProperty& attribute ) {
	if ( !checkPropertyDefinition( attribute ) )
		return false;

	switch ( attribute.getPropertyDefinition()->getPropertyId() ) {
		case PropertyId::Color:
			setFontColor( attribute.asColor() );
			break;
		case PropertyId::ShadowColor:
			setFontShadowColor( attribute.asColor() );
			break;
		case PropertyId::SelectionColor:
			mFontStyleConfig.FontSelectedColor = attribute.asColor();
			break;
		case PropertyId::SelectionBackColor:
			setFontSelectionBackColor( attribute.asColor() );
			break;
		case PropertyId::FontFamily: {
			Font* font = FontManager::instance()->getByName( attribute.asString() );

			if ( NULL != font && font->loaded() ) {
				setFont( font );
			}
			break;
		}
		case PropertyId::FontSize:
			setFontSize( lengthFromValueAsDp( attribute ) );
			break;
		case PropertyId::FontStyle: {
			setFontStyle( attribute.asFontStyle() );
			break;
		}
		case PropertyId::TextStrokeWidth:
			setFontOutlineThickness( lengthFromValue( attribute ) );
			break;
		case PropertyId::TextStrokeColor:
			setFontOutlineColor( attribute.asColor() );
			break;
		default:
			return UIWidget::applyProperty( attribute );
	}

	return true;
}

std::string UIConsole::getPropertyString( const PropertyDefinition* propertyDef,
										  const Uint32& propertyIndex ) const {
	if ( NULL == propertyDef )
		return "";

	switch ( propertyDef->getPropertyId() ) {
		case PropertyId::Color:
			return getFontColor().toHexString();
		case PropertyId::ShadowColor:
			return getFontShadowColor().toHexString();
		case PropertyId::SelectionColor:
			return getFontSelectedColor().toHexString();
		case PropertyId::SelectionBackColor:
			return getFontSelectionBackColor().toHexString();
		case PropertyId::FontFamily:
			return NULL != getFont() ? getFont()->getName() : "";
		case PropertyId::FontSize:
			return String::format( "%.2fdp", getFontSize() );
		case PropertyId::FontStyle:
			return Text::styleFlagToString( getFontStyleConfig().getFontStyle() );
		case PropertyId::TextStrokeWidth:
			return String::toString( PixelDensity::dpToPx( getFontOutlineThickness() ) );
		case PropertyId::TextStrokeColor:
			return getFontOutlineColor().toHexString();
		default:
			return UIWidget::getPropertyString( propertyDef, propertyIndex );
	}
}

UIConsole* UIConsole::setFontSize( const Float& dpSize ) {
	if ( mFontStyleConfig.CharacterSize != dpSize ) {
		mFontStyleConfig.CharacterSize =
			eeabs( dpSize - (int)dpSize ) == 0.5f || (int)dpSize == dpSize ? dpSize
																		   : eefloor( dpSize );
		mFontStyleConfig.CharacterSize = mFontStyleConfig.CharacterSize;
		invalidateDraw();
		onFontChanged();
	}
	return this;
}

const Float& UIConsole::getFontSize() const {
	return mFontStyleConfig.getFontCharacterSize();
}

UIConsole* UIConsole::setFontColor( const Color& color ) {
	if ( mFontStyleConfig.getFontColor() != color ) {
		mFontStyleConfig.FontColor = color;
		invalidateDraw();
		onFontStyleChanged();
	}
	return this;
}

const Color& UIConsole::getFontColor() const {
	return mFontStyleConfig.getFontColor();
}

const Color& UIConsole::getFontSelectedColor() const {
	return mFontStyleConfig.getFontSelectedColor();
}

UIConsole* UIConsole::setFontSelectedColor( const Color& color ) {
	if ( mFontStyleConfig.getFontSelectedColor() != color ) {
		mFontStyleConfig.FontSelectedColor = color;
		invalidateDraw();
		onFontStyleChanged();
	}
	return this;
}

UIConsole* UIConsole::setFontSelectionBackColor( const Color& color ) {
	if ( mFontStyleConfig.getFontSelectionBackColor() != color ) {
		mFontStyleConfig.FontSelectionBackColor = color;
		invalidateDraw();
		onFontStyleChanged();
	}
	return this;
}

const Color& UIConsole::getFontSelectionBackColor() const {
	return mFontStyleConfig.getFontSelectionBackColor();
}

UIConsole* UIConsole::setFontShadowColor( const Color& color ) {
	if ( color != mFontStyleConfig.getFontShadowColor() ) {
		mFontStyleConfig.ShadowColor = color;
		onFontStyleChanged();
	}
	return this;
}

const Color& UIConsole::getFontShadowColor() const {
	return mFontStyleConfig.ShadowColor;
}

UIConsole* UIConsole::setFontStyle( const Uint32& fontStyle ) {
	if ( mFontStyleConfig.Style != fontStyle ) {
		mFontStyleConfig.Style = fontStyle;
		onFontStyleChanged();
	}
	return this;
}

UIConsole* UIConsole::setFontOutlineThickness( const Float& outlineThickness ) {
	if ( mFontStyleConfig.OutlineThickness != outlineThickness ) {
		mFontStyleConfig.OutlineThickness = outlineThickness;
		onFontStyleChanged();
	}
	return this;
}

const Float& UIConsole::getFontOutlineThickness() const {
	return mFontStyleConfig.OutlineThickness;
}

UIConsole* UIConsole::setFontOutlineColor( const Color& outlineColor ) {
	if ( mFontStyleConfig.OutlineColor != outlineColor ) {
		mFontStyleConfig.OutlineColor = outlineColor;
		onFontStyleChanged();
	}
	return this;
}

const Color& UIConsole::getFontOutlineColor() const {
	return mFontStyleConfig.OutlineColor;
}

void UIConsole::onFontChanged() {
	updateCacheSize();
}

void UIConsole::onFontStyleChanged() {
	onFontChanged();
}

void UIConsole::addCommand( const std::string& command, const ConsoleCallback& cb ) {
	if ( !( mCallbacks.count( command ) > 0 ) )
		mCallbacks[command] = cb;
}

const Uint32& UIConsole::getMaxLogLines() const {
	return mMaxLogLines;
}

void UIConsole::setMaxLogLines( const Uint32& maxLogLines ) {
	mMaxLogLines = maxLogLines;
}

void UIConsole::privPushText( const String& str ) {
	Lock l( mMutex );
	mCmdLog.push_back( str );
	invalidateDraw();
	if ( mCmdLog.size() >= mMaxLogLines )
		mCmdLog.pop_front();
}

Int32 UIConsole::linesOnScreen() {
	return static_cast<Int32>(
		( ( getPixelsSize().getHeight() - mPaddingPx.Top - mPaddingPx.Bottom ) / getLineHeight() ) -
		1 );
}

Int32 UIConsole::maxLinesOnScreen() {
	return static_cast<Int32>(
		( ( getPixelsSize().getHeight() - mPaddingPx.Top - mPaddingPx.Bottom ) / getLineHeight() ) +
		3 );
}

void UIConsole::draw() {
	if ( !mVisible || NULL == mFontStyleConfig.Font )
		return;

	Lock l( mMutex );
	Int32 linesInScreen = linesOnScreen();
	size_t pos = 0;
	Float curY;
	Float lineHeight = getLineHeight();

	mCon.min = eemax( 0, (Int32)mCmdLog.size() - linesInScreen );
	mCon.max = (int)mCmdLog.size() - 1;

	UIWidget::draw();

	Color fontColor( Color( mFontStyleConfig.FontColor.r, mFontStyleConfig.FontColor.g,
							mFontStyleConfig.FontColor.b )
						 .blendAlpha( (Uint8)mAlpha ) );

	for ( int i = mCon.max - mCon.modif; i >= mCon.min - mCon.modif; i-- ) {
		if ( i < (int)mCmdLog.size() && i >= 0 ) {
			curY = mScreenPos.y + getPixelsSize().getHeight() - mPaddingPx.Bottom -
				   pos * lineHeight - lineHeight * 2 - 1;
			Text& text = mTextCache[pos];
			text.setStyleConfig( mFontStyleConfig );
			text.setFillColor( fontColor );
			text.setString( mCmdLog[i] );
			text.draw( mScreenPos.x + mPaddingPx.Left, curY );
			pos++;
		}
	}

	curY = mScreenPos.y + getPixelsSize().getHeight() - mPaddingPx.Bottom - lineHeight - 1;

	Text& text = mTextCache[mTextCache.size() - 1];
	text.setStyleConfig( mFontStyleConfig );
	text.setFillColor( fontColor );
	text.setString( "> " + mDoc.getCurrentLine().getTextWithoutNewLine() );
	text.draw( mScreenPos.x + mPaddingPx.Left, curY );

	Text& text2 = mTextCache[mTextCache.size() - 2];
	text2.setStyleConfig( mFontStyleConfig );
	text2.setFillColor( fontColor );

	if ( mCursorVisible ) {
		if ( (unsigned int)mDoc.getSelection().start().column() ==
			 mDoc.getCurrentLine().size() - 1 ) {
			Uint32 width = text.getTextWidth();
			text2.setString( "_" );
			text2.draw( mScreenPos.x + mPaddingPx.Left + width, curY );
		} else {
			text2.setString( "> " + mDoc.getCurrentLine().getText().substr(
										0, mDoc.getSelection().start().column() ) );
			Uint32 width = mPaddingPx.Left + text2.getTextWidth();
			text2.setString( "_" );
			text2.draw( mScreenPos.x + width, curY );
		}
	}

	if ( mShowFps ) {
		Float cw =
			mFontStyleConfig.Font->getGlyph( '_', mFontStyleConfig.CharacterSize, false ).advance;
		Text& text = mTextCache[mTextCache.size() - 3];
		Color OldColor1( text.getColor() );
		text.setStyleConfig( mFontStyleConfig );
		text.setFillColor( fontColor );
		text.setString( "FPS: " + String::toString( getUISceneNode()->getWindow()->getFPS() ) );
		text.draw( mScreenPos.x + getPixelsSize().getWidth() - text.getTextWidth() - cw -
					   mPaddingPx.Right,
				   mScreenPos.y + mPaddingPx.Top + eefloor( lineHeight / 2 ) );
		text.setFillColor( OldColor1 );
	}
}

// CMDS
void UIConsole::createDefaultCommands() {
	addCommand( "clear", [&]( const auto& ) { cmdClear(); } );
	addCommand( "quit", [&]( const auto& ) { getUISceneNode()->getWindow()->close(); } );
	addCommand( "cmdlist", [&]( const auto& ) { cmdCmdList(); } );
	addCommand( "help", [&]( const auto& ) { cmdCmdList(); } );
	addCommand( "showcursor", [&]( const auto& params ) { cmdShowCursor( params ); } );
	addCommand( "setfpslimit", [&]( const auto& params ) { cmdFrameLimit( params ); } );
	addCommand( "getlog", [&]( const auto& ) { cmdGetLog(); } );
	addCommand( "setgamma", [&]( const auto& params ) { cmdSetGamma( params ); } );
	addCommand( "setvolume", [&]( const auto& params ) { cmdSetVolume( params ); } );
	addCommand( "getgpuextensions", [&]( const auto& ) { cmdGetGpuExtensions(); } );
	addCommand( "dir", [&]( const auto& params ) { cmdDir( params ); } );
	addCommand( "ls", [&]( const auto& params ) { cmdDir( params ); } );
	addCommand( "showfps", [&]( const auto& params ) { cmdShowFps( params ); } );
	addCommand( "gettexturememory", [&]( const auto& ) { cmdGetTextureMemory(); } );
}

void UIConsole::cmdClear() {
	size_t cutLines = getPixelsSize().getHeight() / mFontStyleConfig.CharacterSize;
	for ( size_t i = 0; i < cutLines; i++ )
		privPushText( "" );
}

void UIConsole::cmdGetTextureMemory() {
	privPushText( "Total texture memory used: " +
				  FileSystem::sizeToString( TextureFactory::instance()->getTextureMemorySize() ) );
}

void UIConsole::cmdCmdList() {
	for ( auto itr = mCallbacks.begin(); itr != mCallbacks.end(); ++itr )
		privPushText( "\t" + itr->first );
}

void UIConsole::cmdShowCursor( const std::vector<String>& params ) {
	if ( params.size() >= 2 ) {
		Int32 tInt = 0;

		bool Res = String::fromString<Int32>( tInt, params[1] );

		if ( Res && ( tInt == 0 || tInt == 1 ) ) {
			getUISceneNode()->getWindow()->getCursorManager()->setVisible( 0 != tInt );
		} else
			privPushText( "Valid parameters are 0 or 1." );
	} else {
		privPushText( "No parameters. Valid parameters are 0 ( hide ) or 1 ( show )." );
	}
}

void UIConsole::cmdFrameLimit( const std::vector<String>& params ) {
	if ( params.size() >= 2 ) {
		Int32 tInt = 0;

		bool Res = String::fromString<Int32>( tInt, params[1] );

		if ( Res && ( tInt >= 0 && tInt <= 10000 ) ) {
			getUISceneNode()->getWindow()->setFrameRateLimit( tInt );
			return;
		}
	}

	privPushText( "Valid parameters are between 0 and 10000 (0 = no limit)." );
}

void UIConsole::cmdGetLog() {
	std::vector<String> tvec =
		String::split( String( String::toString( Log::instance()->getBuffer() ) ) );
	if ( tvec.size() > 0 ) {
		for ( unsigned int i = 0; i < tvec.size(); i++ )
			privPushText( tvec[i] );
	}
}

void UIConsole::cmdGetGpuExtensions() {
	std::vector<String> tvec = String::split( String( GLi->getExtensions() ), ' ' );
	if ( tvec.size() > 0 ) {
		for ( unsigned int i = 0; i < tvec.size(); i++ )
			privPushText( tvec[i] );
	}
}

void UIConsole::cmdSetGamma( const std::vector<String>& params ) {
	if ( params.size() >= 2 ) {
		Float tFloat = 0.f;
		bool Res = String::fromString<Float>( tFloat, params[1] );

		if ( Res && ( tFloat > 0.1f && tFloat <= 10.0f ) ) {
			getUISceneNode()->getWindow()->setGamma( tFloat, tFloat, tFloat );
			return;
		}
	}

	privPushText( "Valid parameters are between 0.1 and 10." );
}

void UIConsole::cmdSetVolume( const std::vector<String>& params ) {
	if ( params.size() >= 2 ) {
		Float tFloat = 0.f;

		bool Res = String::fromString<Float>( tFloat, params[1] );

		if ( Res && ( tFloat >= 0.0f && tFloat <= 100.0f ) ) {
			EE::Audio::Listener::setGlobalVolume( tFloat );
			return;
		}
	}

	privPushText( "Valid parameters are between 0 and 100." );
}

void UIConsole::cmdDir( const std::vector<String>& params ) {
	if ( params.size() >= 2 ) {
		String Slash( FileSystem::getOSSlash() );
		String myPath = params[1];
		String myOrder;

		if ( params.size() > 2 ) {
			myOrder = params[2];
		}

		if ( FileSystem::isDirectory( myPath ) ) {
			unsigned int i;

			std::vector<String> mFiles = FileSystem::filesGetInPath( myPath );
			std::sort( mFiles.begin(), mFiles.end() );

			privPushText( "Directory: " + myPath );

			if ( myOrder == "ff" ) {
				std::vector<String> mFolders;
				std::vector<String> mFile;

				for ( i = 0; i < mFiles.size(); i++ ) {
					if ( FileSystem::isDirectory( myPath + Slash + mFiles[i] ) ) {
						mFolders.push_back( mFiles[i] );
					} else {
						mFile.push_back( mFiles[i] );
					}
				}

				if ( mFolders.size() )
					privPushText( "Folders: " );

				for ( i = 0; i < mFolders.size(); i++ )
					privPushText( "	" + mFolders[i] );

				if ( mFolders.size() )
					privPushText( "Files: " );

				for ( i = 0; i < mFile.size(); i++ )
					privPushText( "	" + mFile[i] );

			} else {
				for ( i = 0; i < mFiles.size(); i++ )
					privPushText( "	" + mFiles[i] );
			}
		} else {
			if ( myPath == "help" )
				privPushText(
					"You can use a third parameter to show folders first, the parameter is ff." );
			else
				privPushText( "Path \"" + myPath + "\" is not a directory." );
		}
	} else {
		privPushText( "Expected a path to list. Example of usage: ls /home" );
	}
}

void UIConsole::cmdShowFps( const std::vector<String>& params ) {
	if ( params.size() >= 2 ) {
		Int32 tInt = 0;

		bool res = String::fromString<Int32>( tInt, params[1] );

		if ( res && ( tInt == 0 || tInt == 1 ) ) {
			mShowFps = 0 != tInt;
			return;
		}
	}

	privPushText( "Valid parameters are 0 ( hide ) or 1 ( show )." );
}

void UIConsole::writeLog( const std::string& text ) {
	std::vector<String> strings = String::split( String( text ) );
	for ( size_t i = 0; i < strings.size(); i++ )
		privPushText( strings[i] );
}

const bool& UIConsole::isShowingFps() const {
	return mShowFps;
}

void UIConsole::showFps( const bool& show ) {
	mShowFps = show;
}

void UIConsole::copy() {
	getUISceneNode()->getWindow()->getClipboard()->setText( mDoc.getSelectedText().toUtf8() );
}

void UIConsole::cut() {
	getUISceneNode()->getWindow()->getClipboard()->setText( mDoc.getSelectedText().toUtf8() );
	mDoc.deleteSelection();
}

bool UIConsole::getEscapePastedText() const {
	return mEscapePastedText;
}

void UIConsole::setEscapePastedText( bool escapePastedText ) {
	mEscapePastedText = escapePastedText;
}

void UIConsole::paste() {
	String pasted( getUISceneNode()->getWindow()->getClipboard()->getText() );
	if ( mEscapePastedText ) {
		pasted.escape();
	} else {
		String::replaceAll( pasted, "\n", "" );
	}
	mDoc.textInput( pasted );
	sendCommonEvent( Event::OnTextPasted );
}

Uint32 UIConsole::onKeyDown( const KeyEvent& event ) {
	if ( ( event.getKeyCode() == KEY_TAB ) &&
		 mDoc.getSelection().start().column() == (Int64)mDoc.getCurrentLine().size() - 1 ) {
		printCommandsStartingWith( mDoc.getCurrentLine().getTextWithoutNewLine() );
		getFilesFrom( mDoc.getCurrentLine().getTextWithoutNewLine().toUtf8(),
					  mDoc.getSelection().start().column() );
		return 1;
	}

	if ( event.getMod() & KEYMOD_SHIFT ) {
		if ( event.getKeyCode() == KEY_UP && mCon.min - mCon.modif > 0 ) {
			mCon.modif++;
			invalidateDraw();
			return 1;
		}

		if ( event.getKeyCode() == KEY_DOWN && mCon.modif > 0 ) {
			mCon.modif--;
			invalidateDraw();
			return 1;
		}

		if ( event.getKeyCode() == KEY_HOME ) {
			size_t size;
			{
				Lock l( mMutex );
				size = mCmdLog.size();
			}
			if ( static_cast<Int32>( size ) > linesOnScreen() ) {
				mCon.modif = mCon.min;
				invalidateDraw();
				return 1;
			}
		}

		if ( event.getKeyCode() == KEY_END ) {
			mCon.modif = 0;
			invalidateDraw();
			return 1;
		}

		if ( event.getKeyCode() == KEY_PAGEUP ) {
			if ( mCon.min - mCon.modif - linesOnScreen() / 2 > 0 )
				mCon.modif += linesOnScreen() / 2;
			else
				mCon.modif = mCon.min;
			invalidateDraw();
			return 1;
		}

		if ( event.getKeyCode() == KEY_PAGEDOWN ) {
			if ( mCon.modif - linesOnScreen() / 2 > 0 )
				mCon.modif -= linesOnScreen() / 2;
			else
				mCon.modif = 0;
			invalidateDraw();
			return 1;
		}
	} else {
		if ( mLastCommands.size() > 0 ) {
			if ( event.getKeyCode() == KEY_UP && mLastLogPos > 0 ) {
				mLastLogPos--;
			}

			if ( event.getKeyCode() == KEY_DOWN &&
				 mLastLogPos < static_cast<int>( mLastCommands.size() ) ) {
				mLastLogPos++;
			}

			if ( event.getKeyCode() == KEY_UP || event.getKeyCode() == KEY_DOWN ) {
				if ( mLastLogPos == static_cast<int>( mLastCommands.size() ) ) {
					mDoc.replaceCurrentLine( "" );
				} else {
					mDoc.replaceCurrentLine( mLastCommands[mLastLogPos] );
					mDoc.moveToEndOfLine();
				}
				invalidateDraw();
				return 1;
			}
		}
	}

	std::string cmd = mKeyBindings.getCommandFromKeyBind( { event.getKeyCode(), event.getMod() } );
	if ( !cmd.empty() ) {
		mDoc.execute( cmd );
		return 1;
	}
	return UIWidget::onKeyDown( event );
}

Uint32 UIConsole::onTextInput( const TextInputEvent& event ) {
	Input* input = getUISceneNode()->getWindow()->getInput();

	if ( ( input->isLeftAltPressed() && !event.getText().empty() && event.getText()[0] == '\t' ) ||
		 input->isControlPressed() || input->isMetaPressed() || input->isLeftAltPressed() )
		return 0;

	const String& text = event.getText();

	for ( size_t i = 0; i < text.size(); i++ ) {
		if ( text[i] == '\n' )
			return 0;
	}

	mDoc.textInput( text );
	invalidateDraw();
	return 1;
}
Uint32 UIConsole::onPressEnter() {
	processLine();
	sendCommonEvent( Event::OnPressEnter );
	invalidateDraw();
	return 0;
}
void UIConsole::registerCommands() {
	mDoc.setCommand( "copy", [&] { copy(); } );
	mDoc.setCommand( "cut", [&] { cut(); } );
	mDoc.setCommand( "paste", [&] { paste(); } );
	mDoc.setCommand( "press-enter", [&] { onPressEnter(); } );
}

void UIConsole::registerKeybindings() {
	mKeyBindings.addKeybinds( {
		{ { KEY_BACKSPACE, KeyMod::getDefaultModifier() }, "delete-to-previous-word" },
		{ { KEY_BACKSPACE, KEYMOD_SHIFT }, "delete-to-previous-char" },
		{ { KEY_BACKSPACE, 0 }, "delete-to-previous-char" },
		{ { KEY_DELETE, KeyMod::getDefaultModifier() }, "delete-to-next-word" },
		{ { KEY_DELETE, 0 }, "delete-to-next-char" },
		{ { KEY_KP_ENTER, 0 }, "press-enter" },
		{ { KEY_RETURN, 0 }, "press-enter" },
		{ { KEY_LEFT, KeyMod::getDefaultModifier() | KEYMOD_SHIFT }, "select-to-previous-word" },
		{ { KEY_LEFT, KeyMod::getDefaultModifier() }, "move-to-previous-word" },
		{ { KEY_LEFT, KEYMOD_SHIFT }, "select-to-previous-char" },
		{ { KEY_LEFT, 0 }, "move-to-previous-char" },
		{ { KEY_RIGHT, KeyMod::getDefaultModifier() | KEYMOD_SHIFT }, "select-to-next-word" },
		{ { KEY_RIGHT, KeyMod::getDefaultModifier() }, "move-to-next-word" },
		{ { KEY_RIGHT, KEYMOD_SHIFT }, "select-to-next-char" },
		{ { KEY_RIGHT, 0 }, "move-to-next-char" },
		{ { KEY_Z, KeyMod::getDefaultModifier() | KEYMOD_SHIFT }, "redo" },
		{ { KEY_HOME, KeyMod::getDefaultModifier() | KEYMOD_SHIFT }, "select-to-start-of-doc" },
		{ { KEY_HOME, KEYMOD_SHIFT }, "select-to-start-of-content" },
		{ { KEY_HOME, KeyMod::getDefaultModifier() }, "move-to-start-of-doc" },
		{ { KEY_HOME, 0 }, "move-to-start-of-content" },
		{ { KEY_END, KeyMod::getDefaultModifier() | KEYMOD_SHIFT }, "select-to-end-of-doc" },
		{ { KEY_END, KEYMOD_SHIFT }, "select-to-end-of-line" },
		{ { KEY_END, KeyMod::getDefaultModifier() }, "move-to-end-of-doc" },
		{ { KEY_END, 0 }, "move-to-end-of-line" },
		{ { KEY_Y, KeyMod::getDefaultModifier() }, "redo" },
		{ { KEY_Z, KeyMod::getDefaultModifier() }, "undo" },
		{ { KEY_C, KeyMod::getDefaultModifier() }, "copy" },
		{ { KEY_X, KeyMod::getDefaultModifier() }, "cut" },
		{ { KEY_V, KeyMod::getDefaultModifier() }, "paste" },
		{ { KEY_A, KeyMod::getDefaultModifier() }, "select-all" },
	} );
}

void UIConsole::resetCursor() {
	mCursorVisible = true;
	mBlinkTimer.restart();
}

Uint32 UIConsole::onFocus() {
	UINode::onFocus();

	resetCursor();

	getSceneNode()->getWindow()->startTextInput();

	return 1;
}

Uint32 UIConsole::onFocusLoss() {
	getSceneNode()->getWindow()->stopTextInput();
	mCursorVisible = false;
	invalidateDraw();
	return UIWidget::onFocusLoss();
}

bool UIConsole::isTextSelectionEnabled() const {
	return 0 != ( mFlags & UI_TEXT_SELECTION_ENABLED );
}

Uint32 UIConsole::onMouseDown( const Vector2i& position, const Uint32& flags ) {
	UIWidget::onMouseDown( position, flags );

	if ( NULL != getEventDispatcher() && isTextSelectionEnabled() && ( flags & EE_BUTTON_LMASK ) &&
		 getEventDispatcher()->getMouseDownNode() == this ) {
		getUISceneNode()->getWindow()->getInput()->captureMouse( true );
		mMouseDown = true;
	}

	return 1;
}

Uint32 UIConsole::onMouseUp( const Vector2i& position, const Uint32& flags ) {
	if ( flags == EE_BUTTON_WUMASK ) {
		if ( mCon.min - mCon.modif - 6 > 0 ) {
			mCon.modif += 6;
		} else {
			mCon.modif = mCon.min;
		}
	} else if ( flags == EE_BUTTON_WDMASK ) {
		if ( mCon.modif - 6 > 0 ) {
			mCon.modif -= 6;
		} else {
			mCon.modif = 0;
		}
	} else if ( flags & EE_BUTTON_LMASK ) {
		if ( mMouseDown ) {
			mMouseDown = false;
			getUISceneNode()->getWindow()->getInput()->captureMouse( false );
		}
	} else if ( ( flags & EE_BUTTON_RMASK ) ) {
		// onCreateContextMenu( position, flags );
	}
	return UIWidget::onMouseUp( position, flags );
}

Uint32 UIConsole::onMouseClick( const Vector2i& position, const Uint32& flags ) {
	return UIWidget::onMouseClick( position, flags );
}

Uint32 UIConsole::onMouseDoubleClick( const Vector2i& Pos, const Uint32& Flags ) {
	return UIWidget::onMouseDoubleClick( Pos, Flags );
}

Uint32 UIConsole::onMouseOver( const Vector2i& position, const Uint32& flags ) {
	return UIWidget::onMouseOver( position, flags );
}

Uint32 UIConsole::onMouseLeave( const Vector2i& Pos, const Uint32& Flags ) {
	return UIWidget::onMouseLeave( Pos, Flags );
}

void UIConsole::onDocumentTextChanged() {
	resetCursor();

	invalidateDraw();

	sendCommonEvent( Event::OnBufferChange );
}

void UIConsole::onDocumentCursorChange( const TextPosition& ) {
	resetCursor();
	invalidateDraw();
}

void UIConsole::onDocumentSelectionChange( const TextRange& ) {
	onSelectionChange();
}

void UIConsole::onDocumentLineCountChange( const size_t&, const size_t& ) {
	invalidateDraw();
}

void UIConsole::onDocumentLineChanged( const Int64& ) {
	invalidateDraw();
}

void UIConsole::onDocumentUndoRedo( const TextDocument::UndoRedo& ) {
	onSelectionChange();
}

void UIConsole::onDocumentSaved( TextDocument* ) {}

void UIConsole::onDocumentMoved( TextDocument* ) {}

void UIConsole::onSelectionChange() {
	invalidateDraw();
}

String UIConsole::getLastCommonSubStr( std::list<String>& cmds ) {
	String lastCommon( mDoc.getCurrentLine().getTextWithoutNewLine() );
	String strTry( lastCommon );

	std::list<String>::iterator ite;

	bool found = false;

	do {
		found = false;

		bool allEqual = true;

		String strBeg( ( *cmds.begin() ) );

		if ( strTry.size() + 1 <= strBeg.size() ) {
			strTry = String( strBeg.substr( 0, strTry.size() + 1 ) );

			for ( ite = ++cmds.begin(); ite != cmds.end(); ++ite ) {
				String& strCur = ( *ite );

				if ( !( strTry.size() <= strCur.size() &&
						strTry == strCur.substr( 0, strTry.size() ) ) ) {
					allEqual = false;
				}
			}

			if ( allEqual ) {
				lastCommon = strTry;

				found = true;
			}
		}
	} while ( found );

	return lastCommon;
}

void UIConsole::printCommandsStartingWith( const String& start ) {
	std::list<String> cmds;

	for ( auto it = mCallbacks.begin(); it != mCallbacks.end(); ++it ) {
		if ( String::startsWith( it->first, start ) ) {
			cmds.push_back( it->first );
		}
	}

	if ( cmds.size() > 1 ) {
		privPushText( "> " + mDoc.getCurrentLine().getTextWithoutNewLine() );

		std::list<String>::iterator ite;

		for ( ite = cmds.begin(); ite != cmds.end(); ++ite )
			privPushText( ( *ite ) );

		String newStr( getLastCommonSubStr( cmds ) );

		if ( newStr != mDoc.getCurrentLine().getTextWithoutNewLine() ) {
			mDoc.replaceCurrentLine( newStr );
			mDoc.moveToEndOfLine();
		}
	} else if ( cmds.size() ) {
		mDoc.replaceCurrentLine( cmds.front() );
		mDoc.moveToEndOfLine();
	}
}

void UIConsole::updateCacheSize() {
	Int32 maxLines = maxLinesOnScreen();
	if ( maxLines > (Int64)mTextCache.size() )
		mTextCache.resize( maxLines );
}

void UIConsole::onSizeChange() {
	updateCacheSize();
	return UIWidget::onSizeChange();
}

void UIConsole::getFilesFrom( std::string txt, const Uint32& curPos ) {
	static char OSSlash = FileSystem::getOSSlash().at( 0 );
	size_t pos;

	if ( std::string::npos != ( pos = txt.find_last_of( OSSlash ) ) && pos <= curPos ) {
		size_t fpos = txt.find_first_of( OSSlash );

		std::string dir( txt.substr( fpos, pos - fpos + 1 ) );
		std::string file( txt.substr( pos + 1 ) );

		if ( FileSystem::isDirectory( dir ) ) {
			size_t count = 0, lasti = 0;
			std::vector<std::string> files = FileSystem::filesGetInPath( dir, true, true );
			String res;
			bool again = false;

			do {
				std::vector<std::string> foundFiles;
				res = "";
				count = 0;
				again = false;

				for ( size_t i = 0; i < files.size(); i++ ) {
					if ( !file.size() || String::startsWith( files[i], file ) ) {
						res += "\t" + files[i] + "\n";
						count++;
						lasti = i;
						foundFiles.push_back( files[i] );
					}
				}

				if ( count > 1 ) {
					bool allBigger = true;
					bool allStartsWith = true;

					do {
						allBigger = true;

						for ( size_t i = 0; i < foundFiles.size(); i++ ) {
							if ( foundFiles[i].size() < file.size() + 1 ) {
								allBigger = false;
								break;
							}
						}

						if ( allBigger ) {
							std::string tfile = foundFiles[0].substr( 0, file.size() + 1 );
							allStartsWith = true;

							for ( size_t i = 0; i < foundFiles.size(); i++ ) {
								if ( !String::startsWith( foundFiles[i], tfile ) ) {
									allStartsWith = false;
									break;
								}
							}

							if ( allStartsWith ) {
								file = tfile;
								again = true;
							}
						}
					} while ( allBigger && allStartsWith );
				}
			} while ( again );

			if ( count == 1 ) {
				std::string slash = "";

				if ( FileSystem::isDirectory( dir + files[lasti] ) ) {
					slash = FileSystem::getOSSlash();
				}

				mDoc.replaceCurrentLine( mDoc.getCurrentLine().getText().substr( 0, pos + 1 ) +
										 files[lasti] + slash );
			} else if ( count > 1 ) {
				privPushText( "Directory file list:" );
				pushText( res );
				mDoc.replaceCurrentLine( mDoc.getCurrentLine().getText().substr( 0, pos + 1 ) +
										 file );
			}
			mDoc.moveToEndOfLine();
			invalidateDraw();
		}
	}
}

void UIConsole::pushText( const String& str ) {
	if ( std::string::npos != str.find_first_of( '\n' ) ) {
		std::vector<String> Strings = String::split( String( str ) );

		for ( Uint32 i = 0; i < Strings.size(); i++ ) {
			privPushText( Strings[i] );
		}
	} else {
		privPushText( str );
	}
}

void UIConsole::pushText( const char* format, ... ) {
	int n, size = 256;
	std::string tstr( size, '\0' );

	va_list args;

	while ( 1 ) {
		va_start( args, format );

		n = vsnprintf( &tstr[0], size, format, args );

		if ( n > -1 && n < size ) {
			tstr.resize( n );

			pushText( tstr );

			va_end( args );

			return;
		}

		if ( n > -1 )	  // glibc 2.1
			size = n + 1; // precisely what is needed
		else			  // glibc 2.0
			size *= 2;	  // twice the old size

		tstr.resize( size );
	}
}

Float UIConsole::getLineHeight() const {
	return mFontStyleConfig.Font->getFontHeight(
		PixelDensity::dpToPx( mFontStyleConfig.CharacterSize ) );
}

bool UIConsole::getQuakeMode() const {
	return mQuakeMode;
}

void UIConsole::setQuakeMode( bool quakeMode ) {
	if ( mQuakeMode != quakeMode ) {
		mQuakeMode = quakeMode;

		if ( mQuakeMode ) {
			setParent( mUISceneNode->getRoot() );
			Sizef ps( mUISceneNode->getRoot()->getPixelsSize() );
			setPixelsSize( { ps.getWidth(), eefloor( ps.getHeight() * mQuakeModeHeightPercent ) } );
			setPosition( { 0, 0 } );
		}
	}
}

void UIConsole::show() {
	if ( !mQuakeMode ) {
		setVisible( true );
		setEnabled( true );
		return;
	}
	if ( mHiding )
		return;

	setVisible( true );
	setEnabled( true );
	toFront();
	mFading = true;
	auto* spawn = Actions::Spawn::New(
		{ Actions::FadeIn::New( Seconds( .25f ) ),
		  Actions::Move::New( { 0, -getSize().getHeight() }, { 0, 0 }, Seconds( .25f ) ) } );
	runAction( Actions::Sequence::New( { spawn, Actions::Runnable::New( [&] {
											 setVisible( true );
											 setEnabled( true );
											 mFading = false;
											 setFocus();
										 } ) } ) );
}

void UIConsole::hide() {
	if ( !mQuakeMode ) {
		setVisible( false );
		setEnabled( false );
		return;
	}
	if ( mFading )
		return;

	mHiding = true;
	setVisible( true );
	setEnabled( true );
	auto* spawn = Actions::Spawn::New(
		{ Actions::FadeOut::New( Seconds( .25f ) ),
		  Actions::Move::New( { 0, 0 }, { 0, -getSize().getHeight() }, Seconds( .25f ) ) } );
	runAction( Actions::Sequence::New( { spawn, Actions::Runnable::New( [&] {
											 setVisible( false );
											 setEnabled( false );
											 mHiding = false;
										 } ) } ) );
}

void UIConsole::toggle() {
	if ( isVisible() ) {
		hide();
	} else {
		show();
	}
}

bool UIConsole::isActive() const {
	return isVisible() && !mHiding;
}

Float UIConsole::getQuakeModeHeightPercent() const {
	return mQuakeModeHeightPercent;
}

void UIConsole::setQuakeModeHeightPercent( const Float& quakeModeHeightPercent ) {
	mQuakeModeHeightPercent = quakeModeHeightPercent;
}

static std::vector<String> splitCommandParams( String str ) {
	std::vector<String> params = String::split( str, ' ' );
	std::vector<String> rparams;
	String tstr;

	for ( size_t i = 0; i < params.size(); i++ ) {
		String tparam = params[i];

		if ( !tparam.empty() ) {
			if ( '"' == tparam[0] ) {
				tstr += tparam;
			} else if ( '"' == tparam[tparam.size() - 1] ) {
				tstr += " " + tparam;

				rparams.push_back( String::trim( tstr, '"' ) );

				tstr = "";
			} else if ( !tstr.empty() ) {
				tstr += " " + tparam;
			} else {
				rparams.push_back( tparam );
			}
		}
	}

	if ( !tstr.empty() ) {
		rparams.push_back( String::trim( tstr, '"' ) );
	}

	return rparams;
}

void UIConsole::processLine() {
	String str( mDoc.getCurrentLine().getTextWithoutNewLine() );
	std::vector<String> params = splitCommandParams( str );

	mLastCommands.push_back( str );
	mLastLogPos = (int)mLastCommands.size();

	if ( mLastCommands.size() > 20 )
		mLastCommands.pop_front();

	if ( str.size() > 0 ) {
		privPushText( "> " + str );

		if ( mCallbacks.find( params[0] ) != mCallbacks.end() ) {
			mCallbacks[params[0]]( params );
		} else {
			privPushText( "Unknown Command: '" + params[0] + "'" );
		}
	}
	mDoc.replaceCurrentLine( "" );
	invalidateDraw();
}

}} // namespace EE::UI
