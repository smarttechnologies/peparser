// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <windows.h>
#include <delayimp.h>

namespace peparser
{
	// helper wrapper around various PE directory structs (IMAGE_*_DIRECTORY)
	template <class T>
	class PEDirInfo
	{
	public:
		DWORD Index() const;
		DWORD Size() const { return m_size; }
		DWORD Rva() const { return m_rva; }
		DWORD FileOffset() const { return m_fileOffset; }
		DWORD SectionOffset() const { return m_sectionOffset; }

		size_t Count() const;
		T* operator[](size_t i) const
		{
			if (i >= Count()) return NULL;
			return m_directory + i;
		}

		void SetSectionOffset(int offset) { m_sectionOffset = offset; }
		void SetRva(DWORD rva) { m_rva = rva; }
		void SetSize(DWORD size) { m_size = size; }
		void SetFileOffset(DWORD fileOffset) { m_fileOffset = fileOffset; }
		void SetDirectory(T* directory) { m_directory = directory; }

	private:
		DWORD m_rva = 0;
		DWORD m_size = 0;
		DWORD m_fileOffset = 0;
		int m_sectionOffset = 0;
		T* m_directory = NULL;
	};

	template<> inline DWORD PEDirInfo<IMAGE_DEBUG_DIRECTORY>::Index() const { return IMAGE_DIRECTORY_ENTRY_DEBUG; }
	template<> inline DWORD PEDirInfo<IMAGE_RESOURCE_DIRECTORY>::Index() const { return IMAGE_DIRECTORY_ENTRY_RESOURCE; }
	template<> inline DWORD PEDirInfo<IMAGE_IMPORT_DESCRIPTOR>::Index() const { return IMAGE_DIRECTORY_ENTRY_IMPORT; }
	template<> inline DWORD PEDirInfo<IMAGE_EXPORT_DIRECTORY>::Index() const { return IMAGE_DIRECTORY_ENTRY_EXPORT; }
	template<> inline DWORD PEDirInfo<ImgDelayDescr>::Index() const { return IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT; }

	template<> inline size_t PEDirInfo<IMAGE_DEBUG_DIRECTORY>::Count() const { return Size() / sizeof(IMAGE_DEBUG_DIRECTORY); }
	template<> inline size_t PEDirInfo<IMAGE_RESOURCE_DIRECTORY>::Count() const { return m_directory->NumberOfIdEntries + m_directory->NumberOfNamedEntries; }
	template<> inline size_t PEDirInfo<IMAGE_IMPORT_DESCRIPTOR>::Count() const { return 1; }
	template<> inline size_t PEDirInfo<IMAGE_EXPORT_DIRECTORY>::Count() const { return 1; }
	template<> inline size_t PEDirInfo<ImgDelayDescr>::Count() const { return 1; }
}