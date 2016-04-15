// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <string>

namespace peparser
{
	// ====================================================================================
	// string conversion

	inline std::string WideStringToMultiByte(const std::wstring& in)
	{
		size_t size = 0;
		wcstombs_s(&size, NULL, 0, in.data(), in.size());

		if (size == 0) return std::string();

		std::string result(size, 0);
		if (0 != wcstombs_s(NULL, &result[0], result.size(), in.data(), in.size()))
			return std::string();

		if (*result.rbegin() == '\0')
			result.erase(result.end() - 1);

		return result;
	}

	inline std::wstring MultiByteToWideString(const std::string& in)
	{
		size_t size = 0;
		mbstowcs_s(&size, NULL, 0, in.data(), in.size());

		if (size == 0) return std::wstring();

		std::wstring result(size, 0);
		if (0 != mbstowcs_s(NULL, &result[0], result.size(), in.data(), in.size()))
			return std::wstring();

		if (*result.rbegin() == L'\0')
			result.erase(result.end() - 1);

		return result;
	}

	// ====================================================================================
	// searching for strings in PE files

	// string literals used by compiler when generating __FILE__, __DATE__ and __TIME__ macros
	template<class CharT>
	struct Literals
	{
		static const CharT* month[];
		static const CharT colon;
		static const CharT null;
		static const CharT slash;
		static const CharT backslash;
	};

	enum CharConversionType
	{
		None = 0
		, Path = 1 // will normalize path separators and lowercase
		, NoCase = 2 // will lowercase
	};

	template<class CharT, CharConversionType X> struct CharConversion { static CharT Convert(CharT c); };

	template<class CharT> struct CharConversion<CharT, None> { static CharT Convert(CharT c) { return c; } };
	template<class CharT> struct CharConversion<CharT, NoCase> { static CharT Convert(CharT c) { return tolower(c); } };
	template<class CharT> struct CharConversion<CharT, Path>
	{
		static CharT Convert(CharT c)
		{
			if (c == Literals<CharT>::slash)
				return Literals<CharT>::backslash;
			return tolower(c);
		}
	};

	template<class CharT, CharConversionType X> const CharT* FindChar(const CharT* str, CharT c, size_t size)
	{
		if (size <= 0)
			return nullptr;

		c = CharConversion<CharT, X>::Convert(c);

		for (size_t i = 0; i < size; ++i)
			if (CharConversion<CharT, X>::Convert(*(str + i)) == c)
				return str + i;

		return nullptr;
	}

	template<class CharT, CharConversionType X> const CharT* FindStringEntry(const CharT* entry, const std::basic_string<CharT>& str, size_t subStringSize, size_t size)
	{
		if (str.empty())
			return nullptr;

		const CharT* searchStart = entry;

		while (searchStart = FindChar<CharT, X>(searchStart + 1, str[0], size - 1))
		{
			size_t i = 0;
			for (; i < subStringSize; ++i)
				if (CharConversion<CharT, X>::Convert(*(searchStart + i)) != CharConversion<CharT, X>::Convert(str[i]))
					break;
			if (i == subStringSize)
				break;
		}
		return searchStart;
	}

	template<class CharT, CharConversionType X> bool CompareStrings(const CharT* str1, const CharT* str2, size_t size)
	{
		if (!str1 && str2)
			return false;
		if (!str2 && str1)
			return false;
		if (!str1 && !str2)
			return true;

		for (size_t i = 0; i < size; ++i)
			if (CharConversion<CharT, X>::Convert(*(str1 + i)) != CharConversion<CharT, X>::Convert(*(str2 + i)))
				return false;
		return true;
	}
}