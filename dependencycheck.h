// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "boost/filesystem/path.hpp"

#include <string>
#include <list>
#include <map>
#include <set>

namespace peparser
{
	typedef std::shared_ptr<struct PEBinary> PEBinaryPtr;
	typedef std::shared_ptr<struct Import> ImportPtr;
	typedef std::map<std::wstring, PEBinaryPtr> PEBinaryMap;
	typedef std::set<std::string> PrintedSet;

	// describes PE binary and its import dependencies
	struct PEBinary
	{
		boost::filesystem::path path;
		bool resolved = false;
		// false if ActivationContext couldn't be activated for that binary
		// usually happens when SxS system couldn't resolve all assembly dependencies, but might also mean broken manifest
		bool manifestLoaded = false;
		std::list<ImportPtr> dependencies;
	};

	// describes an entry in import table
	struct Import
	{
		std::wstring name;
		bool delayLoad = false;
		PEBinaryPtr pe;
	};

	// returns a flat list of files in path (if it is a directory) satisfying the filter
	// if the path is a file, will return only that (if satisfies filter)
	std::list<boost::filesystem::path> ListFiles(const boost::filesystem::path& path, const std::function<bool(const boost::filesystem::path&)>& filter);

	// loads PATH from system environment, ignoring values in user environment and current environment this executable is running in
	void LoadSystemPath();

	// returns a recursive dependency tree for a given filePath, cache will contain entries for all encountered binaries
	PEBinaryPtr CollectDependencies(const boost::filesystem::path& filePath, PEBinaryMap& cache);

	// writes plain text dependency tree of root binary
	// pass missingOnly to filter out binaries with fully satisfied dependencies
	void PrintDependencyTree(std::ostream& out, const PEBinaryPtr& root, PrintedSet cache, bool missingOnly);

	// writes dependency tree of root binary in json
	// pass missingOnly to filter out binaries with fully satisfied dependencies
	std::string PrintDependencyTreeJson(const PEBinaryPtr& rootPE, const PEBinaryMap& cache, bool missingOnly);
}