// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <string>
#include <map>
#include <memory>

#include "pedirinfo.h"
#include "block.h"
#include "versionstring.h"

// Helper classes for parsing Win32 binary resource formats. See format description here:
// http://www.csn.ul.ie/~caolan/publink/winresdump/winresdump/doc/resfmt.txt

namespace peparser
{
	class ResourceEntry;
	typedef std::shared_ptr<ResourceEntry> ResourceEntryPtr;

	// describes a resource table entry, either a directory or a data node
	class ResourceEntry
	{
	public:
		typedef std::map<std::wstring, ResourceEntryPtr> EntryList;

		ResourceEntry()	{}
		virtual ~ResourceEntry() {}

		// all
		int Id() const { return m_id; }
		const std::wstring& Name() const { return m_name; }
		const std::wstring& Path() const { return m_path; }
		std::wstring FullPath() const;
		void SetId(int id) { m_id = id; }
		void SetName(const std::wstring& name) { m_name = name; }
		void SetPath(const std::wstring& path) { m_path = path; }
		bool IsData() const { return m_isData; }

		// retrieves ResourceEntry for a subdirectory specified by path
		ResourceEntryPtr AtPath(const std::wstring& path) const;

		// writes plain text description of the entry, path for data, path and children tree for directory
		void PrintInfo(std::wostream& out, const std::wstring& prefix = std::wstring()) const;

		// writes contents (binary) to the stream, does nothing for directories
		void Save(std::ostream& out) const;

		// directories
		const EntryList& Entries() const { return m_entries; }
		void AddChild(ResourceEntryPtr node) { m_entries[node->Name()] = node; }

		// data
		void* Address() const { return m_address; }
		size_t FileOffset() const { return m_fileOffset; }
		size_t Size() const { return m_size; }
		void SetAddress(void* address) { m_address = address; m_isData = true; }
		void SetFileOffset(size_t fileOffset) { m_fileOffset = fileOffset; m_isData = true; }
		void SetSize(size_t size) { m_size = size; m_isData = true; }

	private:
		int m_id = 0;
		std::wstring m_name;
		std::wstring m_path;
		EntryList m_entries;

		void* m_address = nullptr;
		size_t m_fileOffset = 0;
		size_t m_size = 0;

		bool m_isData = false;
	};

	// describes resource table
	class ResourceDirectoryTable
	{
	public:
		ResourceDirectoryTable(LPVOID fileBase, PEDirInfo<IMAGE_RESOURCE_DIRECTORY> base, BlockList& interesting);

		ResourceEntryPtr Root() { return m_root; }

	private:
		PEDirInfo<IMAGE_RESOURCE_DIRECTORY> m_base;
		BlockList& m_interesting;
		LPVOID m_fileBase = NULL;
		ResourceEntryPtr m_root;

		void ParseDir(PIMAGE_RESOURCE_DIRECTORY entry, const ResourceEntryPtr& parent);
		void ParseData(PIMAGE_RESOURCE_DATA_ENTRY entry, const ResourceEntryPtr& data);
	};

	class VS_Base
	{
	public:
		VS_Base(LPVOID data, size_t size);
		virtual ~VS_Base();

		bool IsWellFormed() const { return m_wellFormed; }

		void* OriginalData() const { return m_rawData; }
		size_t OriginalSize() const { return m_rawSize; }

	protected:
		bool m_wellFormed = false;
		void* m_rawData = nullptr;
		size_t m_rawSize = 0;

		void* m_updatedData = nullptr;
		size_t m_updatedSize = 0;
	};

	struct StringTableValue
	{
		WCHAR* originalData = nullptr;
		size_t originalSize = 0;
		std::wstring key;
		std::wstring newValue;

		StringTableValue() {}
		StringTableValue(WCHAR* data, size_t size, const std::wstring& key);
		StringTableValue(const std::wstring& key, const std::wstring& value) : key(key), newValue(value){}
	};

	class StringTable : public VS_Base
	{
	public:

		enum Field
		{
			  FileDescription
			, FileVersion
			, InternalName
			, LegalCopyright
			, OriginalFilename
			, ProductName
			, ProductVersion
		};

		typedef std::vector<StringTableValue> EntryList;

		StringTable(LPVOID data, size_t size);

		std::wstring Name() const { return m_tableName; }

		size_t NewSize() const;
		std::shared_ptr<BYTE> NewData();

		std::wstring Value(const std::wstring& key) const;
		// sets value for a field by name
		void SetValue(const std::wstring& key, const std::wstring& value);
		// sets value for a field by id
		void SetField(Field field, const std::wstring& value);

		bool OriginalValue(const std::wstring& key, void** data, size_t* size);

	private:
		std::wstring m_tableName;
		EntryList m_strings;
	};
	typedef std::shared_ptr<StringTable> StringTablePtr;

	class StringFileInfo : public VS_Base
	{
	public:
		typedef std::vector<StringTablePtr> EntryList;

		StringFileInfo(LPVOID data, size_t size);

		EntryList& Entries() { return m_strings; }

		size_t NewSize() const;
		std::shared_ptr<BYTE> NewData();

		// replaces value for given field in all existing string tables
		void SetField(StringTable::Field field, const std::wstring& value);

	private:
		EntryList m_strings;
		std::wstring m_key;
	};
	typedef std::shared_ptr<StringFileInfo> StringFileInfoPtr;

	// handles VS_VERSION_INFO structure which contains binary data for VS_FIXEDFILEINFO
	// plus one or more StringFileInfo entries each containing one or more StringTable entries
	// each containing text fields for FileDescription, FileVersion, etc
	class VS_VersionInfo : public VS_Base
	{
	public:
		typedef std::vector<StringFileInfoPtr> EntryList;

		VS_VersionInfo(LPVOID data, size_t size);

		VS_FIXEDFILEINFO* FixedFileInfo() const { return m_originalFixedFileInfo; }

		EntryList& Entries() { return m_strings; }

		// estimates size after modification, if the size is the same as it was, data can be directly written into PE file at appropriate offset without rebuilding resource table
		size_t NewSize() const;
		// returns binary representation of current state of VS_VERSION_INFO
		std::shared_ptr<BYTE> NewData();

		// field must be FileVersion or ProductVersion
		void SetField(StringTable::Field field, const VersionString& version);
		// field can be any of the available values
		void SetField(StringTable::Field field, const std::wstring& value);

	private:
		VS_FIXEDFILEINFO* m_originalFixedFileInfo = nullptr;
		VS_FIXEDFILEINFO m_fixedFileInfo;
		EntryList m_strings;
		std::wstring m_header;
	};
}