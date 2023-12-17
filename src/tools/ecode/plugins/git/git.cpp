#include "git.hpp"
#include <eepp/system/filesystem.hpp>
#include <eepp/system/log.hpp>
#include <eepp/system/luapattern.hpp>
#include <eepp/system/process.hpp>
#include <eepp/system/sys.hpp>

using namespace EE;
using namespace EE::System;

using namespace std::literals;

namespace ecode {

static constexpr auto sNotCommitedYetHash = "0000000000000000000000000000000000000000";

Git::Blame::Blame( const std::string& error ) : error( error ), line( 0 ) {}

Git::Blame::Blame( std::string&& author, std::string&& authorEmail, std::string&& date,
				   std::string&& commitHash, std::string&& commitShortHash,
				   std::string&& commitMessage, std::size_t line ) :
	author( std::move( author ) ),
	authorEmail( std::move( authorEmail ) ),
	date( std::move( date ) ),
	commitHash( std::move( commitHash ) ),
	commitShortHash( std::move( commitShortHash ) ),
	commitMessage( std::move( commitMessage ) ),
	line( line ) {}

Git::Git( const std::string& projectDir, const std::string& gitPath ) : mGitPath( gitPath ) {
	if ( gitPath.empty() )
		mGitPath = Sys::which( "git" );
	if ( !projectDir.empty() )
		setProjectPath( projectDir );
}

void Git::git( const std::string& args, const std::string& projectDir, std::string& buf ) const {
	Process p;
	p.create( mGitPath, args, Process::CombinedStdoutStderr | Process::Options::NoWindow,
			  { { "LC_ALL", "en_US.UTF-8" } }, projectDir.empty() ? mProjectPath : projectDir );
	p.readAllStdOut( buf );
}

std::string Git::branch( std::string projectDir ) {
	std::string buf;
	git( "rev-parse --abbrev-ref HEAD", projectDir, buf );
	return String::rTrim( buf, '\n' );
}

bool Git::setProjectPath( std::string projectPath ) {
	mProjectPath = "";
	FileInfo f( projectPath );
	if ( !f.isDirectory() )
		return false;
	std::string oriPath( f.getDirectoryPath() );
	std::string path( oriPath );
	std::string lPath;
	FileSystem::dirAddSlashAtEnd( path );
	while ( path != lPath ) {
		if ( FileSystem::fileExists( path + ".git" ) ) {
			mProjectPath = path;
			return true;
		}
		lPath = path;
		path = FileSystem::removeLastFolderFromPath( path );
	}
	return false;
}

Git::Status Git::status( std::string projectDir ) {
	Status s;
	std::string buf;
	git( "diff --numstat", projectDir, buf );
	auto lastNL = 0;
	auto nextNL = buf.find_first_of( '\n' );
	while ( nextNL != std::string_view::npos ) {
		LuaPattern pattern( "(%d+)%s+(%d+)%s+(.+)" );
		LuaPattern::Range matches[4];
		if ( pattern.matches( buf.c_str(), lastNL, matches, nextNL ) ) {
			auto inserted = buf.substr( matches[1].start, matches[1].end - matches[1].start );
			auto deleted = buf.substr( matches[2].start, matches[2].end - matches[2].start );
			auto file = buf.substr( matches[3].start, matches[3].end - matches[3].start );
			int inserts;
			int deletes;
			if ( String::fromString( inserts, inserted ) &&
				 String::fromString( deletes, deleted ) ) {
				s.modified.push_back( { std::move( file ), inserts, deletes } );
				s.totalInserts += inserts;
				s.totalDeletions += deletes;
			}
		}
		lastNL = nextNL;
		nextNL = buf.find_first_of( '\n', nextNL + 1 );
	}
	return s;
}

Git::Blame Git::blame( const std::string& filepath, std::size_t line ) const {
	std::string buf;
	const auto getText = [&buf]( const std::string_view& txt ) -> std::string {
		std::string search = "\n" + txt + " ";
		auto pos = buf.find( search );
		if ( pos != std::string::npos ) {
			pos = pos + search.length();
			auto endPos = buf.find_first_of( '\n', pos );
			if ( endPos != std::string::npos )
				return buf.substr( pos, endPos - pos );
		}
		return "";
	};

	std::string workingDir( FileSystem::fileRemoveFileName( filepath ) );
	git( String::format( "blame %s -p -L%zu,%zu", filepath.data(), line, line ), workingDir, buf );

	if ( String::startsWith( buf, "fatal: " ) )
		return { buf.substr( 7 ) };

	auto hashEnd = buf.find_first_of( ' ' );

	if ( hashEnd == std::string::npos )
		return { "No commit hash found" };

	auto commitHash = buf.substr( 0, hashEnd );

	if ( commitHash == sNotCommitedYetHash )
		return { "Not Committed Yet" };

	auto author = getText( "author"sv );
	auto authorEmail = getText( "author-mail"sv );
	if ( authorEmail.size() > 3 )
		authorEmail = authorEmail.substr( 1, authorEmail.size() - 2 );
	auto datetime = getText( "author-time"sv );
	auto tz = getText( "author-tz"sv );
	Uint64 epoch;
	if ( !datetime.empty() && String::fromString( epoch, datetime ) )
		datetime = Sys::epochToString( epoch ) + ( tz.empty() ? "" : " " + tz );

	auto commitMessage = getText( "summary"sv );

	git( String::format( "rev-parse --short %s", commitHash.c_str() ), workingDir, buf );

	auto commitShortHash = String::rTrim( buf, '\n' );

	return { std::move( author ),
			 std::move( authorEmail ),
			 std::move( datetime ),
			 std::move( commitHash ),
			 std::move( commitShortHash ),
			 std::move( commitMessage ),
			 line };
}

} // namespace ecode
