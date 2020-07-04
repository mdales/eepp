#ifndef EE_UICUIGRIDCELL_HPP
#define EE_UICUIGRIDCELL_HPP

#include <eepp/ui/uiitemcontainer.hpp>
#include <eepp/ui/uiwidget.hpp>

namespace EE { namespace UI {

class UITable;

class EE_API UITableCell : public UIWidget {
  public:
	static UITableCell* New();

	UITableCell();

	virtual ~UITableCell();

	virtual void setTheme( UITheme* Theme );

	void setCell( const Uint32& ColumnIndex, UINode* node );

	UINode* getCell( const Uint32& ColumnIndex ) const;

	bool isSelected() const;

	void unselect();

	void select();

	virtual Uint32 onMessage( const NodeMessage* Msg );

  protected:
	friend class UIItemContainer<UITable>;
	friend class UITable;

	std::vector<UINode*> mCells;

	UITable* gridParent() const;

	void fixCell();

	virtual Uint32 onMouseLeave( const Vector2i& position, const Uint32& flags );

	virtual void onStateChange();

	virtual void onParentChange();

	virtual void onAlphaChange();

	virtual void onAutoSize();
};

}} // namespace EE::UI

#endif
