// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "signer.h"
#include "peparser.h"

#include <windows.h>
#include <wincrypt.h>

#pragma warning(push)
#pragma warning(disable : 4091)
#include <imagehlp.h>
#pragma warning(pop)

#include <iostream>
#include <memory>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Imagehlp.lib")

// The following hyperlinks have useful descriptions of the Microsoft Authenticode APIs used in this file:
// 
// API usage:
// http://blogs.msdn.com/b/alejacma/archive/2008/12/11/how-to-sign-exe-files-with-an-authenticode-certificate-part-2.aspx
// http://blogs.msdn.com/b/alejacma/archive/2010/02/17/signersignex-returns-error-0x80070020.aspx
// 
// Format of a signed file:
// http://www.cs.auckland.ac.nz/~pgut001/pubs/authenticode.txt
// (accessed on 2016-03-29).

// =========================================================================================
// Following structures are not in SDK headers and are defined only in MSDN (Cryptography Structures)
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa380258(v=vs.85).aspx
// (accessed on 2016-03-29).

typedef struct _SIGNER_FILE_INFO
{
	DWORD cbSize;
	LPCWSTR pwszFileName;
	HANDLE hFile;
} SIGNER_FILE_INFO, *PSIGNER_FILE_INFO;

typedef struct _SIGNER_BLOB_INFO
{
	DWORD cbSize;
	GUID *pGuidSubject;
	DWORD cbBlob;
	BYTE *pbBlob;
	LPCWSTR pwszDisplayName;
} SIGNER_BLOB_INFO, *PSIGNER_BLOB_INFO;

typedef struct _SIGNER_SUBJECT_INFO
{
	DWORD cbSize;
	DWORD *pdwIndex;
	DWORD dwSubjectChoice;
	union
	{
		SIGNER_FILE_INFO *pSignerFileInfo;
		SIGNER_BLOB_INFO *pSignerBlobInfo;
	};
} SIGNER_SUBJECT_INFO, *PSIGNER_SUBJECT_INFO;

typedef struct _SIGNER_CERT_STORE_INFO
{
	DWORD cbSize;
	PCCERT_CONTEXT pSigningCert;
	DWORD dwCertPolicy;
	HCERTSTORE hCertStore;
} SIGNER_CERT_STORE_INFO, *PSIGNER_CERT_STORE_INFO;

typedef struct _SIGNER_SPC_CHAIN_INFO
{
	DWORD cbSize;
	LPCWSTR pwszSpcFile;
	DWORD dwCertPolicy;
	HCERTSTORE hCertStore;
} SIGNER_SPC_CHAIN_INFO, *PSIGNER_SPC_CHAIN_INFO;

typedef struct _SIGNER_CERT
{
	DWORD cbSize;
	DWORD dwCertChoice;
	union
	{
		LPCWSTR pwszSpcFile;
		SIGNER_CERT_STORE_INFO *pCertStoreInfo;
		SIGNER_SPC_CHAIN_INFO *pSpcChainInfo;
	};
	HWND hwnd;
} SIGNER_CERT, *PSIGNER_CERT;

typedef struct _SIGNER_ATTR_AUTHCODE
{
	DWORD cbSize;
	BOOL fCommercial;
	BOOL fIndividual;
	LPCWSTR pwszName;
	LPCWSTR pwszInfo;
} SIGNER_ATTR_AUTHCODE, *PSIGNER_ATTR_AUTHCODE;

typedef struct _SIGNER_SIGNATURE_INFO
{
	DWORD cbSize;
	ALG_ID algidHash;
	DWORD dwAttrChoice;
	union
	{
		SIGNER_ATTR_AUTHCODE *pAttrAuthcode;
	};
	PCRYPT_ATTRIBUTES psAuthenticated;
	PCRYPT_ATTRIBUTES psUnauthenticated;
} SIGNER_SIGNATURE_INFO, *PSIGNER_SIGNATURE_INFO;

typedef struct _SIGNER_PROVIDER_INFO
{
	DWORD cbSize;
	LPCWSTR pwszProviderName;
	DWORD dwProviderType;
	DWORD dwKeySpec;
	DWORD dwPvkChoice;
	union
	{
		LPWSTR pwszPvkFileName;
		LPWSTR pwszKeyContainer;
	};
} SIGNER_PROVIDER_INFO, *PSIGNER_PROVIDER_INFO;

typedef struct _SIGNER_CONTEXT
{
	DWORD cbSize;
	DWORD cbBlob;
	BYTE *pbBlob;
} SIGNER_CONTEXT, *PSIGNER_CONTEXT;

// EXPORTS 
typedef HRESULT(WINAPI* SignerFreeSignerContextType)(
	__in  SIGNER_CONTEXT *pSignerContext
);

typedef HRESULT(WINAPI *SignerSignExType)(
	__in      DWORD dwFlags,
	__in      SIGNER_SUBJECT_INFO *pSubjectInfo,
	__in      SIGNER_CERT *pSignerCert,
	__in      SIGNER_SIGNATURE_INFO *pSignatureInfo,
	__in_opt  SIGNER_PROVIDER_INFO *pProviderInfo,
	__in_opt  LPCWSTR pwszHttpTimeStamp,
	__in_opt  PCRYPT_ATTRIBUTES psRequest,
	__in_opt  LPVOID pSipData,
	__out     SIGNER_CONTEXT **ppSignerContext
);

typedef HRESULT(WINAPI *SignerTimeStampExType)(
	__reserved  DWORD dwFlags,
	__in        SIGNER_SUBJECT_INFO *pSubjectInfo,
	__in        LPCWSTR pwszHttpTimeStamp,
	__in        PCRYPT_ATTRIBUTES psRequest,
	__in        LPVOID pSipData,
	__out       SIGNER_CONTEXT **ppSignerContext
);

// MSDN
// =========================================================================================

namespace peparser
{
	class Signer_pimpl
	{
	public:
		Signer_pimpl()
		{
			dll = LoadLibrary(L"Mssign32.dll");

			if (!dll)
				return;

			signerSignEx = (SignerSignExType)GetProcAddress(dll, "SignerSignEx");
			signerFreeSignerContext = (SignerFreeSignerContextType)GetProcAddress(dll, "SignerFreeSignerContext");
			signerTimestampEx = (SignerTimeStampExType)GetProcAddress(dll, "SignerTimeStampEx");

			if (!signerSignEx || !signerFreeSignerContext || !signerTimestampEx)
				std::wcerr << L"Failed to load Mssign32.dll." << std::endl;
		}

		~Signer_pimpl()
		{
			if (certContext)
				CertFreeCertificateContext(certContext);
			if (certStore)
				CertCloseStore(certStore, CERT_CLOSE_STORE_CHECK_FLAG);
			if (dll)
				FreeLibrary(dll);
		}

		bool SelectCertificate(const std::wstring& certStoreName, const std::string& certHash)
		{
			certStore = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, NULL, CERT_SYSTEM_STORE_CURRENT_USER, certStoreName.c_str());

			if (!certStore)
			{
				std::wcerr << L"Failed to open cert store. Error: " << std::hex << GetLastError() << L", Store: " << certStoreName << std::endl;
				return false;
			}

			CRYPT_HASH_BLOB hashBlob;
			hashBlob.pbData = (BYTE*)certHash.data();
			hashBlob.cbData = (DWORD)certHash.size();

			CERT_ID id;
			id.dwIdChoice = CERT_ID_SHA1_HASH;
			id.HashId = hashBlob;
			certContext = CertFindCertificateInStore(certStore, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_CERT_ID, (void *)&id, NULL);

			if (!certContext)
			{
				std::cerr << "Failed to open cert context. Error: " << std::hex << GetLastError() << ", Certificate: " << certHash << std::endl;
				return false;
			}

			return true;
		}

		bool SignFile(const std::wstring& path, std::vector<std::wstring> timestampUrls)
		{
			if (!signerSignEx || !signerFreeSignerContext || !signerTimestampEx)
				return false;

			HANDLE file = CreateFile(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

			if (file == INVALID_HANDLE_VALUE)
			{
				std::wcerr << L"Failed to open file. Error: " << std::hex << GetLastError() << ", File: " << path << std::endl;
				return false;
			}

			std::shared_ptr<int> doomOfFile(nullptr, [file](void*) { CloseHandle(file); });

			SIGNER_FILE_INFO signerFileInfo;
			signerFileInfo.cbSize = sizeof(SIGNER_FILE_INFO);
			signerFileInfo.pwszFileName = path.c_str();
			signerFileInfo.hFile = file;

			SIGNER_SUBJECT_INFO signerSubjectInfo;
			signerSubjectInfo.cbSize = sizeof(SIGNER_SUBJECT_INFO);
			DWORD index = 0;
			signerSubjectInfo.pdwIndex = &index;
			signerSubjectInfo.dwSubjectChoice = 1; // SIGNER_SUBJECT_FILE
			signerSubjectInfo.pSignerFileInfo = &signerFileInfo;

			SIGNER_CERT_STORE_INFO signerCertStoreInfo;
			signerCertStoreInfo.cbSize = sizeof(SIGNER_CERT_STORE_INFO);
			signerCertStoreInfo.pSigningCert = certContext;
			signerCertStoreInfo.dwCertPolicy = 2; // SIGNER_CERT_POLICY_CHAIN
			signerCertStoreInfo.hCertStore = NULL;

			SIGNER_CERT signerCert;
			signerCert.cbSize = sizeof(SIGNER_CERT);
			signerCert.dwCertChoice = 2; // SIGNER_CERT_STORE
			signerCert.pCertStoreInfo = &signerCertStoreInfo;
			signerCert.hwnd = NULL;

			SIGNER_SIGNATURE_INFO signerSignatureInfo;
			signerSignatureInfo.cbSize = sizeof(SIGNER_SIGNATURE_INFO);
			signerSignatureInfo.algidHash = CALG_SHA_256;
			signerSignatureInfo.dwAttrChoice = 0; // SIGNER_NO_ATTR
			signerSignatureInfo.pAttrAuthcode = NULL;
			signerSignatureInfo.psAuthenticated = NULL;
			signerSignatureInfo.psUnauthenticated = NULL;

			HRESULT res = signerSignEx(0, &signerSubjectInfo, &signerCert, &signerSignatureInfo, NULL, NULL, NULL, NULL, NULL);

			if (res != S_OK)
			{
				std::wcerr << L"Failed to sign file. Error: " << res << ", File: " << path << std::endl;
				return false;
			}
			else
				std::wcerr << L"Signed: " << path << std::endl;

			for (auto it = timestampUrls.begin(); it != timestampUrls.end(); ++it)
			{
				res = signerTimestampEx(0, &signerSubjectInfo, it->c_str(), NULL, NULL, NULL);
				if (res == S_OK)
				{
					std::wcerr << L"Timestamped: " << path << std::endl;
					break;
				}
				else
					std::wcerr << L"Failed to timestamp file. Error: " << res << ", Url: " << *it << std::endl;
			}

			return res == S_OK;
		}

	private:
		HMODULE dll = nullptr;

		SignerSignExType signerSignEx = nullptr;
		SignerFreeSignerContextType signerFreeSignerContext = nullptr;
		SignerTimeStampExType signerTimestampEx = nullptr;

		HCERTSTORE certStore = nullptr;
		PCCERT_CONTEXT certContext = nullptr;
	};

	// ==========================================================================================================

	Signer::Signer()
	{
		_m = new Signer_pimpl();
	}

	Signer::~Signer()
	{
		delete _m;
	}

	bool Signer::SelectCertificate(const std::wstring& certStore, const std::string& certHash)
	{
		return _m->SelectCertificate(certStore, certHash);
	}

	bool Signer::SignFile(const std::wstring& path, std::vector<std::wstring> timestampUrls)
	{
		return _m->SignFile(path, timestampUrls);
	}

	// ==========================================================================================================

	void StripSignature(const std::wstring& path, int& retcode)
	{
		HANDLE file = CreateFile(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
		{
			std::wcerr << "Failed to open file for writing." << std::endl;
			retcode = GetLastError();
			return;
		}

		bool enumerateFailed = false;
		DWORD count = 0;
		if (!ImageEnumerateCertificates(file, CERT_SECTION_TYPE_ANY, &count, NULL, 0))
		{
			std::wcerr << "Failed to enumerate certificates. " << GetLastError() << std::endl;
			enumerateFailed = true;
		}
		else
		{
			for (DWORD k = 0; k < count; ++k)
			{
				if (!ImageRemoveCertificate(file, k))
				{
					retcode = GetLastError();
					std::wcerr << "Failed to remove certificate: " << k << L". " << retcode << std::endl;
				}
			}
		}

		CloseHandle(file);

		if (enumerateFailed)
		{
			PEParser pe(path);
			pe.Open(true);
			if (pe.IsCorrupted())
				std::wcerr << L"Fixing signature directory entry." << std::endl;

			pe.EraseSignatureDirectory();
		}
	}
}