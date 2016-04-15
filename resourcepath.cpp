// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma warning(push)
#pragma warning(disable : 4996)
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#pragma warning(pop)

#include "resourcepath.h"
#include "peparser.h"

#include <iostream>

namespace peparser
{
	bool ParsePath(const std::wstring& binaryPath, const std::wstring& resourcePath, ResourcePath& path, std::vector<WORD>& lang_ids, bool& allLanguages)
	{
		std::vector<std::wstring> pathElements;
		boost::split(pathElements, resourcePath, boost::is_any_of(L"/"));

		try
		{
			if (pathElements[0][0] != L'@')
				path.SetType(std::stoi(pathElements[0]));
			else
			{
				pathElements[0] = pathElements[0].substr(1, pathElements[0].size() - 1);
				path.SetType(pathElements[0]);
			}

			if (pathElements[1][0] != L'@')
				path.SetName(std::stoi(pathElements[1]));
			else
			{
				pathElements[1] = pathElements[1].substr(1, pathElements[1].size() - 1);
				path.SetName(pathElements[1]);
			}

			if (pathElements.size() > 2)
			{
				allLanguages = false;
				lang_ids.push_back(std::stoi(pathElements[2]));
			}

			if (allLanguages)
			{
				PEParser pe(binaryPath);
				pe.Open();
				if (!pe.IsValidPE())
				{
					std::wcerr << L"Can't open file for reading or invalid format." << std::endl;
					return false;
				}

				ResourceEntryPtr node;
				if (pe.ResourceDirectory())
					node = pe.ResourceDirectory()->AtPath(resourcePath);

				if (node)
					for (auto& entry : node->Entries())
					{
						if (!entry.second) continue;
						if (entry.second->Name().size() > 0 && entry.second->Name()[0] != L'@')
							lang_ids.push_back(std::stoi(entry.second->Name()));
					}
			}
		}
		catch (...)
		{
			std::wcerr << L"Error parsing options: must have valid resource path (type/name/lang)" << std::endl;
			return false;
		}

		return true;
	}
}