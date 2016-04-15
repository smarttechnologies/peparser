// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma warning(push)
#pragma warning(disable : 4996)
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#pragma warning(pop)

#include "versionstring.h"

#include <sstream>
#include <vector>

namespace peparser
{
	VersionString::VersionString(const std::wstring& value)
	{
		Reset(value);
	}

	bool VersionString::Parse(const std::wstring& value)
	{
		m_valid = false;

		std::vector<std::wstring> versions;

		try
		{
			boost::split(versions, value, boost::is_any_of(L"."));

			if (versions.size() == 4)
			{
				m_valid = true;
				m_major = std::stoi(versions[0]);
				m_minor = std::stoi(versions[1]);
				m_build = std::stoi(versions[2]);
				m_patch = std::stoi(versions[3]);
			}
		}
		catch (std::exception&)
		{
			m_valid = false;
		}

		return m_valid;
	}

	std::wstring VersionString::Combine()
	{
		std::wstringstream res;

		res << m_major << L"." << m_minor << L"." << m_build << L"." << m_patch;

		return res.str();
	}

	VersionString& VersionString::operator=(const std::wstring& value)
	{
		Reset(value);
		return *this;
	}

	void VersionString::Reset(const std::wstring& value)
	{
		Parse(value);

		m_fullUnformatted = value;
		m_fullFormatted = Combine();
	}

	bool VersionString::operator<(const VersionString& vs) const
	{
		if (!this->IsValid())
		{
			if (!vs.IsValid()) // both are invalid, comparing strings
			{
				return this->m_fullUnformatted < vs.m_fullUnformatted;
			}
			return true; // Invalid string is less than valid
		}
		else
		{
			if (!vs.IsValid())
			{
				return false;	// Valid string is not less than invalid
			}
		}

		if (m_major < vs.m_major)
		{
			return true;
		}
		if (m_major > vs.m_major)
		{
			return false;
		}
		if (m_minor < vs.m_minor) // majors are equal
		{
			return true;
		}
		if (m_minor > vs.m_minor)
		{
			return false;
		}
		if (m_build < vs.m_build) // minors are equal too
		{
			return true;
		}
		if (m_build > vs.m_build)
		{
			return false;
		}
		if (m_patch < vs.m_patch) // builds are equal as well
		{
			return true;
		}

		return false;
	}

	bool VersionString::operator==(const VersionString& vs)const
	{
		if (!this->IsValid())
		{
			if (!vs.IsValid())
			{
				return this->m_fullUnformatted == vs.m_fullUnformatted;
			}
			return false; // Invalid string is not equal to valid one
		}

		if (!vs.IsValid())
		{
			return false;	// Valid string is not equal to invalid one
		}

		return ((!(vs < *this)) && (!(*this < vs)));
	}

	bool VersionString::operator>(const VersionString& vs)const
	{
		if (!this->IsValid())
		{
			if (!vs.IsValid())
			{
				return this->m_fullUnformatted > vs.m_fullUnformatted;
			}
			return false; // Invalid string is not greater than valid one
		}

		if (!vs.IsValid()) return true; // Valid string is greater than invalid one

		return (vs < *this);
	}

	VersionString::operator std::wstring() const
	{
		if (IsValid())
		{
			return m_fullFormatted;
		}
		else
		{
			return m_fullUnformatted;
		}
	}

	std::wstring VersionString::FullFormattedForResources() const
	{
		if (IsValid())
		{
			std::wstringstream res;

			res << m_major << L"." << m_minor << L"." << m_build << L"." << m_patch;

			return res.str();
		}
		else
			return m_fullUnformatted;
	}
}