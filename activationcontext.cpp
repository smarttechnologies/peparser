// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "activationcontext.h"

#include <boost/filesystem/path.hpp>

namespace peparser
{
	ActivationContextHandler::ActivationContextHandler(const std::wstring& imagePath, bool activate)
	{
		boost::filesystem::path path(imagePath);
		std::wstring dir = path.parent_path().wstring();

		ACTCTX context;
		ZeroMemory(&context, sizeof(ACTCTX));
		context.cbSize = sizeof(ACTCTX);
		context.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID;
		context.lpSource = imagePath.c_str();

		// 24/1 for exe, 24/2 for dlls
		if (path.extension() == L".exe")
		{
			context.lpResourceName = CREATEPROCESS_MANIFEST_RESOURCE_ID;

			context.dwFlags = context.dwFlags | ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID | ACTCTX_FLAG_SET_PROCESS_DEFAULT;
			context.lpAssemblyDirectory = dir.c_str();
		}
		else
			context.lpResourceName = ISOLATIONAWARE_MANIFEST_RESOURCE_ID;

		m_contextHandle = CreateActCtx(&context);

		if (m_contextHandle == INVALID_HANDLE_VALUE)
			return;

		// Activating context. If successful, all calls to LoadLibrary and such will use the manifest to look for things.
		if (activate && m_contextHandle != INVALID_HANDLE_VALUE)
			m_contextActivated = TRUE == ActivateActCtx(m_contextHandle, &m_context_token);
	}

	ActivationContextHandler::ActivationContextHandler(void* address, bool activate)
	{
		// Getting handle of our own dll
		HMODULE dll = NULL;
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (char*)address, &dll);

		if (!dll) 
			return;

		// Loading manifest from resources (24/2, usual dll place)
		ACTCTX context;
		ZeroMemory(&context, sizeof(ACTCTX));
		context.cbSize = sizeof(ACTCTX);
		context.dwFlags = ACTCTX_FLAG_HMODULE_VALID | ACTCTX_FLAG_RESOURCE_NAME_VALID;
		context.hModule = dll;
		context.lpResourceName = ISOLATIONAWARE_MANIFEST_RESOURCE_ID;

		m_contextHandle = CreateActCtx(&context);

		// Activating context. If successful, all calls to LoadLibrary and such will use the manifest to look for things.
		if (activate && m_contextHandle != INVALID_HANDLE_VALUE)
			m_contextActivated = TRUE == ActivateActCtx(m_contextHandle, &m_context_token);
	}

	ActivationContextHandler::~ActivationContextHandler()
	{
		if (m_contextActivated)
			DeactivateActCtx(0, m_context_token);
		if (m_contextHandle != INVALID_HANDLE_VALUE)
			ReleaseActCtx(m_contextHandle);
	}
}