// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace peparser
{
	class ResourcePathElement
	{
	public:
		ResourcePathElement() {}
		ResourcePathElement(int name) { m_intResource = name; m_useIntResource = true; }
		ResourcePathElement(const std::wstring& name) { m_stringResource = name; }

		operator const wchar_t*() const { return m_useIntResource ? MAKEINTRESOURCE(m_intResource) : m_stringResource.data(); }

	private:
		int m_intResource = 0;
		bool m_useIntResource = false;
		std::wstring m_stringResource;
	};

	// helper class for specifying path to resource table entries
	class ResourcePath
	{
	public:
		void SetType(int type) { m_type = type; }
		void SetName(int name) { m_name = name; }

		void SetType(const std::wstring& type) { m_type = type; }
		void SetName(const std::wstring& name) { m_name = name; }

		const wchar_t* Type() const { return m_type; }
		const wchar_t* Name() const { return m_name; }

	private:
		ResourcePathElement m_type;
		ResourcePathElement m_name;
	};

	// fills in an instance of ResourcePath for a string representation of the path ("type/name/language")
	// opens and reads the binary itself to retrieve language list the resource is available in
	// if allLanguages is set to true or resourcePath string didn't specify a language ("type/name"), lang_ids will contain a list of all available languages for the resource
	// writes to std::cerr
	bool ParsePath(const std::wstring& binaryPath, const std::wstring& resourcePath, ResourcePath& path, std::vector<WORD>& lang_ids, bool& allLanguages);
}