// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <string>
#include <vector>

namespace peparser
{
	// helper class for signing binaries
	// currently supports only certificates in a certificate store
	class Signer
	{
	public:
		Signer();
		~Signer();

		// looks for certificate in a given store with a given hash
		// writes to std::cerr
		bool SelectCertificate(const std::wstring& certStore, const std::string& certHash);

		// signs and timestamps binary with selected certificate
		// tries timestamp urls in order (in case there are connection issues on the server side)
		// writes to std::cerr
		bool SignFile(const std::wstring& path, std::vector<std::wstring> timestampUrls);

	private:
		class Signer_pimpl* _m = nullptr;
	};

	// cleanly removes digital signature section and updates PE header appropriately
	// if updated incorrectly (by EndUpdateResource() for example) binary becomes unsignable by some tools
	void StripSignature(const std::wstring& path, int& retcode);
}