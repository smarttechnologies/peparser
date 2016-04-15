// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "etoken.h"
#include <iostream>

#pragma comment(lib, "crypt32.lib")

namespace peparser
{
	SafeNetTokenLogin::SafeNetTokenLogin(const std::string& password)
	{
		if (password.empty())
			return;

		if (!CryptAcquireContextA(&m_cryptProvider, NULL, "eToken Base Cryptographic Provider", PROV_RSA_FULL, CRYPT_SILENT))
		{
			std::wcerr << L"Failed to open crypto context. Error: " << std::hex << GetLastError() << L", Provider: eToken Base Cryptographic Provider." << std::endl;
			return;
		}

		// 0x10003
		// The low 2 bits of this DWORD parameter have the following values:
		// 0 - default behavior: the eToken CSP may involve user interaction upon the need.
		// 1 - only password-related UI is allowed.
		// 2 - any UI is suppressed: it is similar to passing CRYPT_SILENT to all CryptAcquireContext calls.
		DWORD ui = 0x2;
		if (!CryptSetProvParam(m_cryptProvider, 0x10003, (const BYTE*)&ui, 0))
		{
			std::wcerr << L"Failed to set UI level on crypto context. Error: " << std::hex << GetLastError() << std::endl;
			return;
		}

		if (!CryptSetProvParam(m_cryptProvider, PP_KEYEXCHANGE_PIN, (const BYTE*)password.c_str(), 0))
		{
			std::wcerr << L"Failed to set eToken password. Error: " << std::hex << GetLastError() << L", Password: redacted." << std::endl;
			return;
		}
	}

	SafeNetTokenLogin::~SafeNetTokenLogin()
	{
		if (!m_cryptProvider)
			return;

		CryptSetProvParam(m_cryptProvider, PP_KEYEXCHANGE_PIN, NULL, 0);
		CryptReleaseContext(m_cryptProvider, NULL);
	}
}