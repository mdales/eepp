#ifndef ECODE_PLUGINMANAGER_HPP
#define ECODE_PLUGINMANAGER_HPP

#include <eepp/ui/models/model.hpp>
#include <eepp/ui/uicodeeditor.hpp>
#include <eepp/ui/uiscenenode.hpp>
#include <eepp/ui/uiwindow.hpp>
#include <memory>
#include <string>

using namespace EE;
using namespace EE::System;
using namespace EE::UI;
using namespace EE::UI::Models;

namespace ecode {

class PluginManager;

typedef std::function<UICodeEditorPlugin*( const PluginManager* pluginManager )> PluginCreatorFn;

struct PluginVersion {
	PluginVersion() {}

	PluginVersion( Uint8 major, Uint8 minor, Uint8 patch ) :
		major( major ),
		minor( minor ),
		patch( patch ),
		string( String::format( "%d.%d.%d", major, minor, patch ) ) {}

	Uint8 major; /**< major version */
	Uint8 minor; /**< minor version */
	Uint8 patch; /**< update version */
	std::string string;

	Uint32 getVersion() const { return major * 1000 + minor * 100 + patch; }

	const std::string& getVersionString() const { return string; }
};

struct PluginDefinition {
	std::string id;
	std::string name;
	std::string description;
	PluginCreatorFn creatorFn;
	PluginVersion version;
};

class PluginManager {
  public:
	static constexpr int versionNumber( int major, int minor, int patch ) {
		return ( (major)*1000 + (minor)*100 + ( patch ) );
	}

	static std::string versionString( int major, int minor, int patch ) {
		return String::format( "%d.%d.%.d", major, minor, patch );
	}

	PluginManager( const std::string& resourcesPath, const std::string& pluginsPath,
				   std::shared_ptr<ThreadPool> pool );

	~PluginManager();

	void registerPlugin( const PluginDefinition& def );

	UICodeEditorPlugin* get( const std::string& id );

	bool setEnabled( const std::string& id, bool enable );

	bool isEnabled( const std::string& id ) const;

	const std::string& getResourcesPath() const;

	const std::string& getPluginsPath() const;

	const std::map<std::string, bool>& getPluginsEnabled() const;

	void onNewEditor( UICodeEditor* editor );

	void setPluginsEnabled( const std::map<std::string, bool>& pluginsEnabled );

	const std::shared_ptr<ThreadPool>& getThreadPool() const;

	std::function<void( UICodeEditorPlugin* )> onPluginEnabled;

	const std::map<std::string, PluginDefinition>& getDefinitions() const;

	const PluginDefinition* getDefinitionIndex( const Int64& index ) const;

  protected:
	std::string mResourcesPath;
	std::string mPluginsPath;
	std::map<std::string, UICodeEditorPlugin*> mPlugins;
	std::map<std::string, bool> mPluginsEnabled;
	std::map<std::string, PluginDefinition> mDefinitions;
	std::shared_ptr<ThreadPool> mThreadPool;

	bool hasDefinition( const std::string& id );
};

class PluginsModel : public Model {
  public:
	enum Columns { Id, Title, Enabled, Description, Version };

	static std::shared_ptr<PluginsModel> New( PluginManager* manager );

	PluginsModel( PluginManager* manager ) : mManager( manager ) {}

	virtual ~PluginsModel() {}

	virtual size_t rowCount( const ModelIndex& ) const;

	virtual size_t columnCount( const ModelIndex& ) const { return mColumnNames.size(); }

	virtual std::string columnName( const size_t& col ) const;

	virtual void setColumnName( const size_t& index, const std::string& name ) {
		eeASSERT( index <= Columns::Version );
		mColumnNames[index] = name;
	}

	virtual Variant data( const ModelIndex& index, ModelRole role = ModelRole::Display ) const;

	virtual void update() { onModelUpdate(); }

	PluginManager* getManager() const;

  protected:
	PluginManager* mManager;
	std::vector<std::string> mColumnNames{ "Id", "Title", "Enabled", "Description", "Version" };
};

class UIPluginManager {
  public:
	static UIWindow* New( UISceneNode* sceneNode, PluginManager* manager,
						  std::function<void( const std::string& )> loadFileCb );
};

} // namespace ecode

#endif // ECODE_PLUGINMANAGER_HPP
