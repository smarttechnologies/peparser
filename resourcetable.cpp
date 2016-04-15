// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "resourcetable.h"

#include <iomanip>

namespace peparser
{
	ResourceEntryPtr ResourceEntry::AtPath(const std::wstring& path) const
	{
		if (IsData() || Entries().size() == 0) 
			return ResourceEntryPtr();

		size_t pos = path.find_first_of(L"/");

		std::wstring current = path.substr(0, pos);

		EntryList::const_iterator it = Entries().find(current);
		if (it == Entries().end() || !it->second)
			return ResourceEntryPtr();

		if (pos == std::wstring::npos)
			return it->second;

		std::wstring remaining = path.substr(pos + 1);

		return it->second->AtPath(remaining);
	}

	void ResourceEntry::PrintInfo(std::wostream& out, const std::wstring& prefix) const
	{
		out << prefix << FullPath() << L'\n';

		for (auto& entry : Entries())
		{
			if (!entry.second)
				continue;

			entry.second->PrintInfo(out, prefix);
		}
	}

	std::wstring ResourceEntry::FullPath() const
	{
		if (Path().empty()) 
			return Name();

		return Path() + L"/" + Name();
	}

	void ResourceEntry::Save(std::ostream& out) const
	{
		if (!IsData()) 
			return;

		out.write((const char*)Address(), Size());
	}

	ResourceDirectoryTable::ResourceDirectoryTable(LPVOID fileBase, PEDirInfo<IMAGE_RESOURCE_DIRECTORY> base, BlockList& interesting)
		: m_base(base)
		, m_fileBase(fileBase)
		, m_interesting(interesting)
		, m_root(new ResourceEntry())
	{
		m_root->SetName(L"");
		ParseDir(m_base[0], m_root);
	}

	void ResourceDirectoryTable::ParseDir(PIMAGE_RESOURCE_DIRECTORY entry, const ResourceEntryPtr& parent)
	{
		if (!entry) 
			return;

		size_t count = entry->NumberOfIdEntries + entry->NumberOfNamedEntries;
		++entry;
		for (size_t i = 0; i < count; ++i)
		{
			PIMAGE_RESOURCE_DIRECTORY_ENTRY subEntry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)entry + i;

			ResourceEntryPtr directory(new ResourceEntry());
			if (parent->Path().empty())
				directory->SetPath(parent->Name());
			else
				directory->SetPath(parent->Path() + L"/" + parent->Name());

			if (subEntry->NameIsString)
			{
				PIMAGE_RESOURCE_DIR_STRING_U name = (PIMAGE_RESOURCE_DIR_STRING_U)((LPBYTE)m_base[0] + subEntry->NameOffset);
				directory->SetName(L"@" + std::wstring(name->NameString, name->Length));
			}
			else
			{
				directory->SetId(subEntry->Id);
				directory->SetName(std::to_wstring(subEntry->Name));
			}

			if (subEntry->DataIsDirectory)
			{
				ParseDir((PIMAGE_RESOURCE_DIRECTORY)((LPBYTE)m_base[0] + subEntry->OffsetToDirectory), directory);
				parent->AddChild(directory);
			}
			else
			{
				ParseData((PIMAGE_RESOURCE_DATA_ENTRY)((LPBYTE)m_base[0] + subEntry->OffsetToData), directory);
				parent->AddChild(directory);
			}
		}
	}

	void ResourceDirectoryTable::ParseData(PIMAGE_RESOURCE_DATA_ENTRY entry, const ResourceEntryPtr& data)
	{
		if (!entry || !data) 
			return;

		data->SetSize(entry->Size);
		data->SetAddress((LPBYTE)(m_base[0]) + entry->OffsetToData - m_base.Rva());
		data->SetFileOffset((size_t)data->Address() - (size_t)m_fileBase);

		m_interesting.push_back(Block(L"Resource: " + data->FullPath(), data->FileOffset(), data->Size()));
	}

	// ================================================================================================

	struct VersionInfoHeader
	{
		WORD length; // Length of the version resource
		WORD valueLength; // Length of the value field for this block
		WORD type; // type of information:  1==string, 0==binary
	};

	size_t Alignment(size_t size)
	{
		if (size % sizeof(DWORD) != 0)
			return sizeof(WORD);
		else
			return 0;
	}

	LPVOID ShiftByWideString(LPVOID start, int max, size_t addToAlign, std::wstring* copy = NULL)
	{
		if (!start) 
			return NULL;
		if (max == 0) 
			return start;

		WCHAR* str = (WCHAR*)start;
		size_t length = wcsnlen(str, max);
		size_t size = sizeof(WCHAR) * (length + 1); // with \0

		if (copy)
			copy->assign(str, length + 1);

		size += Alignment(addToAlign + size);

		return ((LPBYTE)start) + size;
	}

	void WriteString(LPBYTE dest, const std::wstring& str, size_t addToAlign, size_t& shift, bool noAlign = false)
	{
		size_t size = str.size() * sizeof(wchar_t);
		memcpy(dest + shift, str.data(), size);
		shift += size;

		if (!noAlign)
			shift += Alignment(addToAlign + size);
	}

	// ================================================================================================

	VS_Base::VS_Base(LPVOID data, size_t size)
	{
		m_rawData = data;
		m_rawSize = size;
	}

	VS_Base::~VS_Base()
	{
	}

	// ================================================================================================

	StringTableValue::StringTableValue(WCHAR* data, size_t size, const std::wstring& key)
		: originalData(data)
		, originalSize(size * sizeof(wchar_t))
		, key(key)
	{
		newValue.assign(originalData, size);
	}

	StringTable::StringTable(LPVOID data, size_t size)
		: VS_Base(data, size)
	{
		VersionInfoHeader* header = (VersionInfoHeader*)data;
		if (header->length != size) 
			return;

		LPVOID entryOffset = header + 1;

		entryOffset = ShiftByWideString(entryOffset, header->length, sizeof(VersionInfoHeader), &m_tableName);

		while (entryOffset < (LPBYTE)data + header->length)
		{
			LPVOID currentOffset = entryOffset;
			VersionInfoHeader* stringHeader = (VersionInfoHeader*)currentOffset;

			if (stringHeader->length == 0) 
				break;

			currentOffset = stringHeader + 1;

			std::wstring key;
			currentOffset = ShiftByWideString(currentOffset, stringHeader->length, sizeof(VersionInfoHeader), &key);

			m_strings.push_back(StringTableValue((WCHAR*)currentOffset, stringHeader->valueLength, key));

			entryOffset = (LPBYTE)entryOffset + stringHeader->length + Alignment(stringHeader->length);
		}

		m_wellFormed = true;
	}

	bool StringTable::OriginalValue(const std::wstring& key, void** data, size_t* size)
	{
		for (auto& value : m_strings)
		{
			if (value.key != key) 
				continue;

			*data = value.originalData;
			*size = value.originalSize;

			return true;
		}
		return false;
	}

	std::wstring StringTable::Value(const std::wstring& key) const
	{
		for (auto& value : m_strings)
		{
			if (value.key != key) 
				continue;

			return value.newValue;
		}

		return L"";
	}

	void StringTable::SetValue(const std::wstring& key, const std::wstring& newValue)
	{
		for (size_t i = 0; i < m_strings.size(); ++i)
		{
			if (m_strings[i].key != key) 
				continue;

			m_strings[i].newValue = newValue + L'\0';
			return;
		}

		m_strings.push_back(StringTableValue(key, newValue));
	}

	size_t StringTable::NewSize() const
	{
		if (!IsWellFormed())
			return OriginalSize();

		size_t newSize = 0;
		newSize += sizeof(VersionInfoHeader);
		newSize += m_tableName.size() * sizeof(wchar_t);
		newSize += Alignment(sizeof(VersionInfoHeader) + m_tableName.size() * sizeof(wchar_t));

		for (size_t i = 0; i < m_strings.size(); ++i)
		{
			newSize += sizeof(VersionInfoHeader);
			newSize += m_strings[i].key.size() * sizeof(wchar_t);
			newSize += Alignment(sizeof(VersionInfoHeader) + m_strings[i].key.size() * sizeof(wchar_t));
			newSize += m_strings[i].newValue.size() * sizeof(wchar_t);
			if (i == m_strings.size() - 1) 
				continue;
			newSize += Alignment(m_strings[i].newValue.size() * sizeof(wchar_t));
		}

		return newSize;
	}

	std::shared_ptr<BYTE> StringTable::NewData()
	{
		WORD size = (WORD)NewSize();
		std::shared_ptr<BYTE> data((LPBYTE)operator new(size));
		memset(data.get(), 0, size);

		if (!IsWellFormed())
			memcpy(data.get(), OriginalData(), size);
		else
		{
			size_t shift = 0;

			VersionInfoHeader header;
			header.length = size;
			header.type = 1;
			header.valueLength = 0;

			memcpy(data.get() + shift, &header, sizeof(VersionInfoHeader));
			shift += sizeof(VersionInfoHeader);

			WriteString(data.get(), m_tableName, sizeof(VersionInfoHeader), shift);

			for (size_t i = 0; i < m_strings.size(); ++i)
			{
				VersionInfoHeader header;
				header.valueLength = (WORD)m_strings[i].newValue.size(); // in chars
				header.length = (WORD)(sizeof(VersionInfoHeader) + header.valueLength * sizeof(wchar_t) + m_strings[i].key.size() * sizeof(wchar_t) + Alignment(sizeof(VersionInfoHeader) + m_strings[i].key.size() * sizeof(wchar_t)));
				header.type = 1;

				memcpy(data.get() + shift, &header, sizeof(VersionInfoHeader));
				shift += sizeof(VersionInfoHeader);

				WriteString(data.get(), m_strings[i].key, sizeof(VersionInfoHeader), shift);
				WriteString(data.get(), m_strings[i].newValue, 0, shift, i == m_strings.size() - 1);
			}
		}

		return data;
	}

	void StringTable::SetField(Field field, const std::wstring& value)
	{
		std::wstring key;
		switch (field)
		{
		case FileDescription:
			key = L"FileDescription";
			break;
		case FileVersion:
			key = L"FileVersion";
			break;
		case InternalName:
			key = L"InternalName";
			break;
		case LegalCopyright:
			key = L"LegalCopyright";
			break;
		case OriginalFilename:
			key = L"OriginalFilename";
			break;
		case ProductName:
			key = L"ProductName";
			break;
		case ProductVersion:
			key = L"ProductVersion";
			break;
		default:
			return;
		}

		key.push_back('\0');
		SetValue(key, value);
	}

	// ================================================================================================

	StringFileInfo::StringFileInfo(LPVOID data, size_t size)
		:VS_Base(data, size)
	{
		VersionInfoHeader* header = (VersionInfoHeader*)data;
		if (header->length != size) 
			return;

		LPVOID currentOffset = header + 1;

		currentOffset = ShiftByWideString(currentOffset, header->length, sizeof(VersionInfoHeader), &m_key);

		if (m_key.c_str() != std::wstring(L"StringFileInfo")) 
			return;

		while (currentOffset < (LPBYTE)data + header->length)
		{
			VersionInfoHeader* stringTableHeader = (VersionInfoHeader*)currentOffset;

			if (stringTableHeader->length == 0) 
				break;

			m_strings.push_back(StringTablePtr(new StringTable(currentOffset, stringTableHeader->length)));

			currentOffset = (LPBYTE)currentOffset + stringTableHeader->length + Alignment(stringTableHeader->length);
		}

		m_wellFormed = true;
	}

	size_t StringFileInfo::NewSize() const
	{
		if (!IsWellFormed()) 
			return OriginalSize();

		size_t newSize = 0;

		newSize += sizeof(VersionInfoHeader);
		newSize += m_key.size() * sizeof(wchar_t);
		newSize += Alignment(sizeof(VersionInfoHeader) + m_key.size() * sizeof(wchar_t));

		for (size_t i = 0; i < m_strings.size(); ++i)
		{
			size_t blockSize = m_strings[i]->NewSize();
			newSize += blockSize;
			if (i == m_strings.size() - 1) 
				continue;
			newSize += Alignment(blockSize);
		}

		return newSize;
	}

	std::shared_ptr<BYTE> StringFileInfo::NewData()
	{
		size_t size = NewSize();
		std::shared_ptr<BYTE> data((LPBYTE)operator new(size));
		memset(data.get(), 0, size);

		if (!IsWellFormed())
			memcpy(data.get(), OriginalData(), size);
		else
		{
			size_t shift = 0;

			VersionInfoHeader header;
			header.length = (WORD)size;
			header.type = 1;
			header.valueLength = 0;

			memcpy(data.get() + shift, &header, sizeof(VersionInfoHeader));
			shift += sizeof(VersionInfoHeader);

			WriteString(data.get(), m_key, sizeof(VersionInfoHeader), shift);

			for (size_t i = 0; i < m_strings.size(); ++i)
			{
				size_t nextSize = m_strings[i]->NewSize();

				memcpy(data.get() + shift, m_strings[i]->NewData().get(), nextSize);
				shift += nextSize;

				if (i != m_strings.size() - 1)
					shift += Alignment(nextSize);
			}
		}

		return data;
	}

	void StringFileInfo::SetField(StringTable::Field field, const std::wstring& value)
	{
		for (auto& entry : m_strings)
			entry->SetField(field, value);
	}

	// ================================================================================================

	VS_VersionInfo::VS_VersionInfo(LPVOID data, size_t size)
		:VS_Base(data, size)
	{
		VersionInfoHeader* info = (VersionInfoHeader*)data;
		if (info->length != size) 
			return;
		if (info->valueLength != sizeof(VS_FIXEDFILEINFO)) 
			return;

		VS_FIXEDFILEINFO* fixedInfo = (VS_FIXEDFILEINFO*)ShiftByWideString(info + 1, info->length, sizeof(VersionInfoHeader), &m_header);
		if (fixedInfo->dwSignature != 0xfeef04bd) 
			return;

		m_originalFixedFileInfo = fixedInfo;

		memcpy(&m_fixedFileInfo, fixedInfo, sizeof(VS_FIXEDFILEINFO));

		LPVOID currentOffset = fixedInfo + 1;

		while (currentOffset < (LPBYTE)info + info->length)
		{
			VersionInfoHeader* header = (VersionInfoHeader*)currentOffset;

			m_strings.push_back(StringFileInfoPtr(new StringFileInfo(currentOffset, header->length)));
			currentOffset = (LPBYTE)currentOffset + header->length + Alignment(header->length);
		}

		m_wellFormed = true;
	}

	size_t VS_VersionInfo::NewSize() const
	{
		if (!IsWellFormed()) return OriginalSize();

		size_t newSize = 0;

		newSize += sizeof(VersionInfoHeader);
		newSize += m_header.size() * sizeof(wchar_t);
		newSize += Alignment(sizeof(VersionInfoHeader) + m_header.size() * sizeof(wchar_t));
		newSize += sizeof(VS_FIXEDFILEINFO);

		for (auto& entry : m_strings)
		{
			size_t blockSize = entry->NewSize();
			newSize += blockSize + Alignment(blockSize);
		}

		return newSize;
	}

	std::shared_ptr<BYTE> VS_VersionInfo::NewData()
	{
		WORD size = (WORD)NewSize();
		std::shared_ptr<BYTE> data((LPBYTE)operator new(size));
		memset(data.get(), 0, size);

		if (!IsWellFormed())
			memcpy(data.get(), OriginalData(), size);
		else
		{
			size_t shift = 0;

			VersionInfoHeader header;
			header.length = size;
			header.type = 0;
			header.valueLength = sizeof(VS_FIXEDFILEINFO);

			memcpy(data.get() + shift, &header, sizeof(VersionInfoHeader));
			shift += sizeof(VersionInfoHeader);

			WriteString(data.get(), m_header, sizeof(VersionInfoHeader), shift);

			memcpy(data.get() + shift, &m_fixedFileInfo, sizeof(VS_FIXEDFILEINFO));
			shift += sizeof(VS_FIXEDFILEINFO);

			for (auto& entry : m_strings)
			{
				size_t nextSize = entry->NewSize();

				memcpy(data.get() + shift, entry->NewData().get(), nextSize);
				shift += nextSize + Alignment(nextSize);
			}
		}

		return data;
	}

	void VS_VersionInfo::SetField(StringTable::Field field, const VersionString& version)
	{
		DWORD ms = version.Major();
		ms <<= 8 * sizeof(WORD);
		ms += version.Minor();

		DWORD ls = version.Build();
		ls <<= 8 * sizeof(WORD);
		ls += version.Patch();

		if (field == StringTable::FileVersion)
		{
			m_fixedFileInfo.dwFileVersionMS = ms;
			m_fixedFileInfo.dwFileVersionLS = ls;
		}
		else if (field == StringTable::ProductVersion)
		{
			m_fixedFileInfo.dwProductVersionMS = ms;
			m_fixedFileInfo.dwProductVersionLS = ls;
		}
		SetField(StringTable::ProductVersion, version.FullFormattedForResources());
	}

	void VS_VersionInfo::SetField(StringTable::Field field, const std::wstring& value)
	{
		for (auto& entry : m_strings)
			entry->SetField(field, value);
	}
}