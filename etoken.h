// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <windows.h>
#include <wincrypt.h>

#include <string>

namespace peparser
{
	// helper class for authentication with password protected SafeNet etokens 
	// logs in on construction, logs out on destruction
	// while an instance is alive all calls to CryptoAPI made from this process have access to certificates stored on the token
	// writes to std::cerr
	class SafeNetTokenLogin
	{
	public:
		SafeNetTokenLogin(const std::string& password);
		~SafeNetTokenLogin();

	private:
		HCRYPTPROV m_cryptProvider = NULL;
	};
}