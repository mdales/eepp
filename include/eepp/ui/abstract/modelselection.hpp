#ifndef EE_UI_MODEL_MODELSELECTION_HPP
#define EE_UI_MODEL_MODELSELECTION_HPP

#include <algorithm>
#include <eepp/ui/abstract/modelindex.hpp>
#include <functional>
#include <unordered_set>

namespace EE { namespace UI { namespace Abstract {

class UIAbstractView;

class EE_API ModelSelection {
  public:
	ModelSelection( UIAbstractView* view ) : mView( view ) {}

	int size() const { return mIndexes.size(); }
	bool isEmpty() const { return mIndexes.empty(); }
	bool contains( const ModelIndex& index ) const {
		return std::find( mIndexes.begin(), mIndexes.end(), index ) != mIndexes.end();
	}
	bool containsRow( int row ) const {
		for ( auto& index : mIndexes ) {
			if ( index.row() == row )
				return true;
		}
		return false;
	}

	void set( const ModelIndex& );
	void add( const ModelIndex& );
	void toggle( const ModelIndex& );
	bool remove( const ModelIndex& );
	void clear();

	template <typename Callback> void forEachIndex( Callback callback ) {
		for ( auto& index : indexes() )
			callback( index );
	}

	template <typename Callback> void forEachIndex( Callback callback ) const {
		for ( auto& index : indexes() )
			callback( index );
	}

	std::vector<ModelIndex> indexes() const {
		std::vector<ModelIndex> selectedIndexes;
		for ( auto& index : mIndexes )
			selectedIndexes.push_back( index );
		return selectedIndexes;
	}

	ModelIndex first() const {
		if ( mIndexes.empty() )
			return {};
		return *mIndexes.begin();
	}

	void removeMatching( std::function<bool( const ModelIndex& )> );

  protected:
	UIAbstractView* mView;
	std::vector<ModelIndex> mIndexes;
};

}}} // namespace EE::UI::Abstract

#endif // EE_UI_MODEL_MODELSELECTION_HPP
