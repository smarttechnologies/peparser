// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "widestring.h"

namespace peparser
{
	template<> const char Literals<char>::colon = ':';
	template<> const wchar_t Literals<wchar_t>::colon = L':';

	template<> const char Literals<char>::null = '\0';
	template<> const wchar_t Literals<wchar_t>::null = L'\0';

	template<> const char* Literals<char>::month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	template<> const wchar_t* Literals<wchar_t>::month[] = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };

	template<> const char Literals<char>::slash = '/';
	template<> const wchar_t Literals<wchar_t>::slash = L'/';

	template<> const char Literals<char>::backslash = '\\';
	template<> const wchar_t Literals<wchar_t>::backslash = L'\\';
}
