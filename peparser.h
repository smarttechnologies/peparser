// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <windows.h>

#include "block.h"
#include "resourcetable.h"
#include "pedirinfo.h"

#include <vector>
#include <map>
#include <string>
#include <ostream>
#include <algorithm>
#include <iterator>

namespace peparser
{
	enum ModifiableBlocks
	{
		  FileVersion
		, ProductVersion
		, FileVersionString
		, ProductVersionString
		, SignatureDirectory
	};

	enum UsefulBlocks
	{
		MidlTimestampSegment
	};

	typedef std::multimap<ModifiableBlocks, Block> ModifiableBlockMap;
	typedef std::multimap<UsefulBlocks, Block> UsefulBlockMap;
	typedef std::pair<UsefulBlockMap::const_iterator, UsefulBlockMap::const_iterator> UsefulBlockMapRange;

	// describes PE comparison result 
	class CompareResult
	{
		friend class PEParser;
	public:
		// compared files are byte-for-byte identical
		bool IsIdentical() const;
		// compared files have differences, but they are all superficial
		bool IsEquivalent() const;
		bool IsError() const { return m_error; }
		bool IsWrongFormat() const { return m_wrongFormat; }
		bool IsDifferentPath() const { return m_differentPath; }
		// full file path of compilation units affects compiler memory layout and often causes big shifts in generated machine code
		// comparison is often hopeless in this case
		bool IsDifferentPathLength() const { return m_differentPathLength; }
		bool IsDifferentCompiler() const { return m_differentCompiler; }
		bool IsDifferentSize() const { return m_differentSize; }
		// failed to parse on of the files
		bool IsCorrupted() const { return m_corrupted; }

		float PercentDifferent() const;

		void PrintInfo(std::wostream& out) const;

		void SetVerbose(bool verbose) { m_verbose = verbose; }

	private:
		bool m_identical = false;
		bool m_equivalent = false;
		bool m_differentPath = false;
		bool m_differentPathLength = false;
		bool m_differentCompiler = false;
		bool m_wrongFormat = false;
		bool m_differentSize = false;
		bool m_error = false;
		bool m_fast = false;
		bool m_verbose = false;
		bool m_corrupted = false;

		__int64 m_same = 0;
		__int64 m_different = 0;
		BlockNodePtr m_tree;

		Block2List m_interesting;
		Block2List m_dynamicIgnored;
		Block2List m_diffs;
	};

	// handles Win32 PE binaries
	// prints to std::err
	class PEParser
	{
	public:
		explicit PEParser(const std::wstring& path);
		virtual ~PEParser();

		bool Open(bool readWrite = false);
		void Close();

		static bool IsPE(const std::wstring& path, bool& x64);

		bool IsOpen() const { return m_open; }
		bool IsValidPE() const { return m_validPE; }
		bool IsCorrupted() const { return m_corrupted; }
		bool Is64Bit() const { return m_pe32Plus; }
		bool IsSigned() const { return m_signed; }
		size_t FileSize() const { return m_fileSize; }
		std::wstring PDBPath() const { return m_pdbPath; }
		std::wstring PDBGUID() const { return m_pdbGuid; }
		std::wstring FileVersion() const { return m_fileVersion; }
		ResourceEntryPtr ResourceDirectory() const { return m_resources; }

		// returns raw contents of a PE section
		// can be used to look at contents of custom sections (#pragma section) among other things
		std::string SectionData(const std::wstring& name);

		const std::vector<std::string>& DllImports() const { return m_dllImports; }
		const std::vector<std::string>& DelayedDllImports() const { return m_dllDelayedImports; }

		std::vector<std::string> AllDllImports() const
		{
			std::vector<std::string> imports = m_dllImports;
			std::copy(m_dllDelayedImports.begin(), m_dllDelayedImports.end(), std::back_inserter(imports));
			return imports;
		}

		// Compares 2 PE binaries
		// use fast to only ignore known static fields (PE timestamps, file versions, etc) and not highlight unknown differences in verbose output
		// use noHeuristics to avoid searching for __FILE__, __DATE__ and other fussily matchable differences
		static CompareResult Compare(const PEParser& p1, const PEParser& p2, bool fast, bool noHeuristics, bool verbose);

		// manually mark a range as irrelevant when comparing binaries 
		void AddIgnoredRange(const Block& block);
		// manually mark a list of ranges as irrelevant when comparing binaries 
		void AddIgnoredRange(const BlockList& blocks);

		void PEParser::PrintInfo(std::wostream& stream, bool verbose) const;

		enum VersionField
		{
			  Both
			, FileOnly
			, ProductOnly
		};
		// modifies file version in place without rebuilding resources 
		// fails if new version strings are bigger than existing strings (resource table rebuild would be required then)
		bool SetVersion(const VersionString& version, VersionField field) const;

		// EndUpdateResources() strips signature without wiping appropriate directory entry. This call "fixes" that.
		void EraseSignatureDirectory() const;

	private:
		std::wstring m_path;

		HANDLE m_file = INVALID_HANDLE_VALUE;
		HANDLE m_fileMap = NULL;
		LPVOID m_view = NULL;

		size_t m_viewSize = 0;
		size_t m_fileSize = 0;

		bool m_open = false;
		bool m_openForWrite = false;
		bool m_validPE = false;
		bool m_corrupted = false;
		bool m_pe32Plus = false;
		bool m_signed = false;
		std::wstring m_pdbPath;
		std::wstring m_pdbGuid;
		std::wstring m_fileVersion;
		ResourceEntryPtr m_resources;
		std::vector<std::string> m_dllImports;
		std::vector<std::string> m_dllDelayedImports;

		BlockList m_ignored;
		BlockList m_interesting;
		BlockList m_resourceBlocks;
		BlockList m_sections;
		ModifiableBlockMap m_modifiable;
		UsefulBlockMap m_useful;

		bool OpenReadOnly();
		bool OpenRW();

		bool Initialize();

		template <class CharT> std::basic_string<CharT> PDBPathT() const;

		bool ReadSections(PIMAGE_NT_HEADERS ntHeaders);
		bool ReadImportsDirectory(PIMAGE_NT_HEADERS ntHeaders);
		bool ReadExportsDirectory(PIMAGE_NT_HEADERS ntHeaders);
		bool ReadDebugDirectory(PIMAGE_NT_HEADERS ntHeaders);
		bool ReadDigitalSignatureDirectory(PIMAGE_NT_HEADERS ntHeaders);
		bool ReadResourceDirectory(PIMAGE_NT_HEADERS ntHeaders);
		bool ReadTypeLibrary(ResourceEntryPtr node);
		bool ReadTypeLibrary(LPVOID data, size_t size);
		bool ReadVsVersionInfo(ResourceEntryPtr node);
		bool ReadVsVersionInfo(LPVOID data, size_t size);

		bool FindFileOffsetFromRva(PIMAGE_NT_HEADERS ntHeaders, DWORD rva, DWORD& fileOffset);

		template <class T> bool DirectoryInfo(PIMAGE_NT_HEADERS ntHeaders, PEDirInfo<T>& dir);
		size_t FileOffset(void* pointer) const;

		size_t TotalIgnoredSize() const;
		size_t NextOffset(size_t currentOffset, size_t& sizeOfBlock, size_t maxSize) const;

		static bool FilterDifference(CompareResult& result, const PEParser& p1, const PEParser& p2, size_t start1, size_t start2, size_t size, size_t& diffShift);

		template <class CharT> bool DetectFILEMacro(size_t diffStart, size_t diffSize) const;
		static bool DetectFILEMacro(const PEParser& p1, const PEParser& p2, size_t start1, size_t start2, size_t size, size_t& diffShift);

		template <class CharT> bool DetectTIMEMacro(size_t diffStart, size_t diffSize, size_t& diffShift) const;
		static bool DetectTIMEMacro(const PEParser& p1, const PEParser& p2, size_t start1, size_t start2, size_t size, size_t& diffShift);

		template <class CharT> bool DetectDATEMacro(size_t diffStart, size_t diffSize, size_t& diffShift) const;
		static bool DetectDATEMacro(const PEParser& p1, const PEParser& p2, size_t start1, size_t start2, size_t size, size_t& diffShift);

		static bool DetectMIDLMarker(const PEParser& p1, const PEParser& p2, size_t start1, size_t start2, size_t size, size_t& diffShift);
		bool DetectMIDLMarker(size_t diffStart, size_t diffSize, size_t& diffShift) const;
	};

	// ====================================================================================================

	inline std::wostream& operator<<(std::wostream& in, const PEParser& pe)
	{
		pe.PrintInfo(in, false);
		return in;
	}

	inline std::wostream& operator<<(std::wostream& in, const CompareResult& res)
	{
		res.PrintInfo(in);
		return in;
	}
}