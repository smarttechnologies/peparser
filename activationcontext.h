// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <windows.h>
#include <string>

namespace peparser
{
	// Loads side-by-side manifest and activates activation context
	class ActivationContextHandler
	{
	public:
		// Takes an image path to PE binary.
		ActivationContextHandler(const std::wstring& imagePath, bool activate);
		// Takes an address of a function exported by a dll. Useful when dll needs to load and activate its own manifest.
		ActivationContextHandler(void* adress, bool activate);
		~ActivationContextHandler();

		HANDLE ContextHandle() const { return m_contextHandle; }
		bool IsActivated() const { return m_contextActivated; }

	private:
		HANDLE m_contextHandle = INVALID_HANDLE_VALUE;
		ULONG_PTR m_context_token = NULL;
		bool m_contextActivated = false;
	};
}