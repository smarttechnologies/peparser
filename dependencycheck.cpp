// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "dependencycheck.h"
#include "activationcontext.h"
#include "peparser.h"
#include "widestring.h"
#include "json/json.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <set>

namespace fs = boost::filesystem;

namespace peparser
{
	bool IsNonExecutablePE(const fs::path& path)
	{
		auto ext = boost::algorithm::to_lower_copy(path.extension().wstring());
		return ext == L"" || ext == L".dll" || ext == L".cpl" || ext == L".sys";
	}

	std::list<fs::path> ListFiles(const fs::path& path, const std::function<bool(const fs::path&)>& filter)
	{
		std::list<fs::path> files;

		if (fs::is_directory(path))
			std::for_each(fs::directory_iterator(path), fs::directory_iterator(), [&](const fs::path& path)
		{
			files.splice(files.end(), ListFiles(path, filter));
		});
		else
			if (filter(path))
				files.push_back(path);

		return files;
	}

	std::wstring GetModuleFileName(HMODULE handle)
	{
		if (!handle)
			return std::wstring();

		std::wstring path;
		path.resize(MAX_PATH);

		auto size = GetModuleFileName(handle, &path[0], MAX_PATH);

		auto err = GetLastError();

		if (size > 0)
			path.resize(size);

		return path;
	}

	std::wstring FindLoadedDll(boost::filesystem::path name, const boost::filesystem::path& parentPath)
	{
		if (name.extension().empty())
			name.replace_extension(L".");

		// when dll is loaded by an executable, loader will have the path to the folder it is in, and will pick up dll's dependencies from there (among other places)
		// when loading dll's dependencies on its own, have to simulate that with PATH

		std::wstring pathEnv;
		wchar_t* buf = nullptr;
		size_t size = 0;
		if (_wdupenv_s(&buf, &size, L"PATH"))
		{
			if (buf)
				pathEnv.assign(buf, size);
			free(buf);
		}

		_wputenv((L"PATH=" + parentPath.wstring() + L";" + pathEnv).c_str());

		auto handle = LoadLibraryEx(name.wstring().c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);

		auto path = GetModuleFileName(handle);

		FreeLibrary(handle);

		_wputenv((L"PATH=" + pathEnv).c_str());

		return path;
	}

	std::wstring FindDll(const boost::filesystem::path& name, const boost::filesystem::path& parentPath)
	{
		std::wstring path;
		path.resize(MAX_PATH);

		auto size = SearchPath(NULL, name.wstring().c_str(), NULL, MAX_PATH, &path[0], NULL);

		path.resize(size);

		return path;
	}

	PEBinaryPtr CollectDependencies(const boost::filesystem::path& path, PEBinaryMap& cache)
	{
		if (path.empty())
			return PEBinaryPtr();

		bool x64 = false;
		if (!PEParser::IsPE(path.wstring(), x64))
			return PEBinaryPtr();

#ifdef _WIN64
		if (!x64) 
			return PEBinaryPtr();
#else
		if (x64) 
			return PEBinaryPtr();
#endif

		std::vector<std::string> imports, delayedImports;
		{
			PEParser pe(path.wstring());
			pe.Open(false);
			imports = pe.DllImports();
			delayedImports = pe.DelayedDllImports();
		}

		ActivationContextHandler context(path.wstring(), true);

		PEBinaryPtr peBinary(new PEBinary);
		peBinary->path = path;
		peBinary->resolved = true;
		peBinary->manifestLoaded = context.IsActivated();
		cache[boost::algorithm::to_lower_copy(path.wstring())] = peBinary;

		auto processImports = [&](const std::vector<std::string>& imports, bool delayed)
		{
			for (auto& import : imports)
			{
				auto loadedPath = FindLoadedDll(import, path.parent_path());

				ImportPtr dll(new Import);
				dll->name = MultiByteToWideString(import);
				dll->delayLoad = delayed;
				peBinary->dependencies.push_back(dll);

				auto knownDll = cache.find(boost::algorithm::to_lower_copy(loadedPath));
				if (knownDll != cache.end())
					dll->pe = knownDll->second;
				else
					dll->pe = CollectDependencies(loadedPath, cache);

				if (!dll->delayLoad && (!dll->pe || !dll->pe->resolved))
					peBinary->resolved = false;
			}
		};

		processImports(imports, false);
		processImports(delayedImports, true);

		return peBinary;
	}

	void PrintDependencyTree(std::wostream& out, ImportPtr node, int depth, PrintedSet& cache, bool missingOnly)
	{
		if (!node)
			return;

		if (missingOnly && node->pe && node->pe->resolved)
			return;

		std::wstring offset;
		if (depth > 0)
			offset.resize(depth, L'\t');

		out
			<< offset
			<< ((node->pe && node->pe->resolved) ? L"[ ]" : L"[!]")
			<< ((node->delayLoad) ? L"[D]" : L"[ ]")
			<< ((node->pe && node->pe->manifestLoaded) ? L"[M]" : L"[ ]")
			<< L" "
			<< node->name
			<< L" -> "
			<< (node->pe ? node->pe->path.wstring() : L"")
			<< L"\n";

		if (!node->pe)
			return;

		auto lowercasePath = boost::algorithm::to_lower_copy(node->pe->path.string());
		if (cache.end() != cache.find(lowercasePath))
			return;

		cache.insert(lowercasePath);

		for (auto it = node->pe->dependencies.begin(); it != node->pe->dependencies.end(); ++it)
			PrintDependencyTree(out, *it, depth + 1, cache, missingOnly);
	}

	void PrintDependencyTree(std::wostream& out, const PEBinaryPtr& root, PrintedSet cache, bool missingOnly)
	{
		if (!root)
			return;

		if (missingOnly && root->resolved)
			return;

		out
			<< (root->resolved ? L"[ ]" : L"[!]")
			<< L"[ ]"
			<< (root->manifestLoaded ? L"[M] " : L"[ ] ")
			<< root->path.wstring()
			<< L"\n";

		for (auto& pe : root->dependencies)
			PrintDependencyTree(out, pe, 1, cache, missingOnly);
	}

	std::string NormalizePath(const std::string& in)
	{
		return boost::replace_all_copy(in, "\\", "/");
	}

	std::string PrintDependencyTreeJson(const PEBinaryPtr& rootPE, const PEBinaryMap& binaries, bool missingOnly)
	{
		json::Object root;

		json::Array binariesArray;
		for (auto& pe : binaries)
		{
			if (!pe.second)
				continue;

			if (missingOnly && pe.second->resolved)
				continue;

			json::Object object;
			object["id"] = NormalizePath(pe.second->path.string());
			object["resolved"] = pe.second->resolved;
			object["manifest"] = pe.second->manifestLoaded;

			json::Array imports;
			for (auto& import : pe.second->dependencies)
			{
				if (missingOnly && import->pe && import->pe->resolved)
					continue;

				json::Object object;
				object["id"] = NormalizePath((import->pe) ? import->pe->path.string() : WideStringToMultiByte(import->name));
				object["delayed"] = import->delayLoad;
				object["name"] = WideStringToMultiByte(import->name);
				imports.push_back(object);
			};
			object["imports"] = imports;

			binariesArray.push_back(object);
		};
		root["binaries"] = binariesArray;

		if (rootPE)
		{
			root["id"] = NormalizePath(rootPE->path.string());
			root["resolved"] = rootPE->resolved;
			root["type"] = "singlefile";
		}
		else
			root["type"] = "cachedump";

		return json::Serialize(root);
	}

	void LoadSystemPath()
	{
		HKEY key = NULL;
		bool success = false;
		std::wstring rawValue;

		LSTATUS err = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_READ, &key);
		if (err == ERROR_SUCCESS)
		{
			DWORD size = 0;
			err = RegQueryValueEx(key, L"Path", 0, NULL, NULL, &size);
			if (err == ERROR_SUCCESS)
			{
				rawValue.resize(size / sizeof(std::wstring::value_type), L'\0');

				err = RegQueryValueEx(key, L"Path", 0, NULL, (BYTE*)&rawValue[0], &size);
				if (err == ERROR_SUCCESS)
				{
					DWORD expandedSize = ExpandEnvironmentStrings(rawValue.c_str(), NULL, 0);
					if (expandedSize != 0)
					{
						std::wstring expandedValue;
						expandedValue.resize(expandedSize, L'\0');

						expandedSize = ExpandEnvironmentStrings(rawValue.c_str(), &expandedValue[0], expandedSize);

						if (expandedSize != 0)
						{
							rawValue.swap(expandedValue);
							success = true;
						}
					}
				}
			}
			RegCloseKey(key);
		}

		if (success && !rawValue.empty())
			SetEnvironmentVariableW(L"Path", rawValue.c_str());
	}
}