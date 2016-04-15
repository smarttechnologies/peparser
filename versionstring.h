// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <string>

namespace peparser
{
	class VersionString
	{
	public:
		VersionString() { }
		VersionString(const std::wstring& value);
		virtual ~VersionString() { }

		int Major() const { return m_major; }
		int Minor() const { return m_minor; }
		int Build() const { return m_build; }
		int Patch() const { return m_patch; }
		std::wstring FullFormatted() const { return m_fullFormatted; }
		std::wstring FullFormattedForResources() const;

		virtual bool operator<(const VersionString& vs)const;
		virtual bool operator==(const VersionString& vs)const;
		virtual bool operator>(const VersionString& vs)const;

		virtual operator std::wstring() const;
		virtual VersionString& operator=(const std::wstring& value);

		bool IsValid() const { return m_valid; }

	protected:
		int m_major = 0;
		int m_minor = 0;
		int m_build = 0;
		int m_patch = 0;

		std::wstring m_fullFormatted;
		std::wstring m_fullUnformatted;

		bool m_valid = false;

		std::wstring Combine();
		bool Parse(const std::wstring& value);
		void Reset(const std::wstring& value);
	};
}