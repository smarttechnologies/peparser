// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "peparser.h"
#include "widestring.h"
#include "versionstring.h"

#include "debugdirectory.h"

#include <boost/io/ios_state.hpp>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <wchar.h>
#include <fstream>

// ================================================================================================

#define MAKE_PTR( cast, ptr, addValue ) (cast)( (DWORD_PTR)(ptr) + (DWORD_PTR)(addValue))

// ================================================================================================

namespace peparser
{
	bool CompareResult::IsIdentical() const
	{
		return m_identical;
	}

	bool CompareResult::IsEquivalent() const
	{
		return IsIdentical() || m_equivalent;
	}

	float CompareResult::PercentDifferent() const
	{
		if(m_fast)
			return 100.0f;
		else
		{
			if(m_different == 0)
				return 0.0f;
			else
				return 100.0f * ((float)m_different/(m_same + m_different));
		}
	}

	void CompareResult::PrintInfo(std::wostream& out) const
	{
		boost::io::ios_base_all_saver ofs(out);

		if(IsError())
		{
			out << L"Error opening one of the files." << L'\n';
			return;
		}

		if(IsIdentical())
			out << L"Identical.";
		else if(IsEquivalent())
			out << L"Functionally equivalent.";
		else 
			out << L"Not equivalent.";
		out << L'\n' << L'\n';

		if(IsWrongFormat())
			out << L"One of the files is not a valid PE." << L'\n';

		if(IsCorrupted())
			out << L"One of the files is a corrupted PE." << L'\n';

		if(IsDifferentSize())
			out << L"Different file size." << L'\n';

		if(IsDifferentCompiler())
			out << L"Different compiler." << L'\n';

		if(IsDifferentPath())
			out << L"Different build path." << L'\n';

		if(IsDifferentPathLength())
			out << L"Different build path length." << L'\n';

		if(!m_fast)
		{
			out.precision(2);
			out << L"Difference: " << std::fixed << PercentDifferent() << L"% (" << m_different << " bytes)"<< L'\n';
		}

		out << std::endl;

		if(!m_verbose) 
			return;

		out.setf(std::ios::hex, std::ios::basefield);

		m_tree->Add(m_interesting);
		m_tree->Add(m_dynamicIgnored);
		m_tree->Add(m_diffs);

		m_tree->Sort();

		m_tree->Print(out, L"");
	}

	// ================================================================================================

	PEParser::PEParser(const std::wstring& path)
		: m_path(path)
	{
	}

	PEParser::~PEParser()
	{
		Close();
	}

	bool PEParser::IsPE(const std::wstring& path, bool& x64)
	{
		// not sure if this is actually faster than mapping whole file into memory...

		std::ifstream file(path.c_str(), std::ios_base::binary | std::ios_base::in);

		if(!file.is_open())
			return false;

		std::string buffer;
		buffer.resize(sizeof(IMAGE_DOS_HEADER));

		if(!file.read(&buffer[0], sizeof(IMAGE_DOS_HEADER)))
			return false;

		PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)&buffer[0];

		if(dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
			return false;

		file.seekg(dosHeader->e_lfanew);

		std::string ntHeaderBuffer;
		ntHeaderBuffer.resize(sizeof(IMAGE_NT_HEADERS));
		if(!file.read(&ntHeaderBuffer[0], sizeof(IMAGE_NT_HEADERS)))
			return false;

		PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)&ntHeaderBuffer[0];
	
		x64 = ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC;

		return ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC || ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
	}

	bool PEParser::Open(bool rw)
	{
		WIN32_FILE_ATTRIBUTE_DATA fileInfo;
		ZeroMemory(&fileInfo, sizeof(fileInfo));
		if(!GetFileAttributesEx(m_path.c_str(), GetFileExInfoStandard, (LPVOID)&fileInfo))
		{
			std::wcerr << L"Can't read file. " << GetLastError() << std::endl;
			return false;
		}

		if(fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			std::wcerr << L"File is a directory. " << GetLastError() << std::endl;
			return false;
		}

		m_fileSize = fileInfo.nFileSizeLow;

		if(rw)
			return OpenRW();
		else
			return OpenReadOnly();
	}

	bool PEParser::OpenReadOnly()
	{
		m_file = CreateFile(m_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if(m_file == INVALID_HANDLE_VALUE)
		{
			std::wcerr << L"Failed to open file. " << GetLastError() << std::endl;
			return false;
		}

		m_fileMap = CreateFileMapping(m_file, NULL, PAGE_READONLY, 0, 0, NULL); 
		if(!m_fileMap)
		{
			std::wcerr << L"Failed to map file into memory. " << GetLastError() << std::endl;
			return false;
		}

		m_view = MapViewOfFile(m_fileMap, FILE_MAP_READ, 0, 0, 0); 
		if(!m_view)
		{
			std::wcerr << L"Failed to open view. " << GetLastError() << std::endl;
			return false;
		}

		MEMORY_BASIC_INFORMATION memInfo;
		ZeroMemory(&memInfo, sizeof(memInfo));
		if(VirtualQuery(m_view, &memInfo, sizeof(memInfo)) == 0)
		{
			std::wcerr << L"Failed to query view. " << GetLastError() << std::endl;
			return false;
		}

		m_viewSize = memInfo.RegionSize;

		m_open = true;
		m_openForWrite = false;

		return Initialize();
	}

	bool PEParser::OpenRW()
	{
		m_file = CreateFile(m_path.c_str(), GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if(!m_file || m_file == INVALID_HANDLE_VALUE)
		{
			std::wcerr << L"Failed to open file. " << GetLastError() << std::endl;
			return false;
		}

		m_fileMap = CreateFileMapping(m_file, NULL, PAGE_READWRITE, 0, 0, NULL); 
		if(!m_fileMap)
		{
			std::wcerr << L"Failed to map file into memory. " << GetLastError() << std::endl;
			return false;
		}

		m_view = MapViewOfFile(m_fileMap, FILE_MAP_WRITE, 0, 0, 0); 
		if(!m_view)
		{
			std::wcerr << L"Failed to open view. " << GetLastError() << std::endl;
			return false;
		}

		MEMORY_BASIC_INFORMATION memInfo;
		ZeroMemory(&memInfo, sizeof(memInfo));
		if(VirtualQuery(m_view, &memInfo, sizeof(memInfo)) == 0)
		{
			std::wcerr << L"Failed to query view. " << GetLastError() << std::endl;
			return false;
		}

		m_viewSize = memInfo.RegionSize;

		m_open = true;
		m_openForWrite = true;

		return Initialize();
	}

	void PEParser::Close()
	{
		if(m_view)
			UnmapViewOfFile(m_view);
		if(m_fileMap)
			CloseHandle(m_fileMap);
		if(m_file && m_file != INVALID_HANDLE_VALUE)
		{
			if(m_openForWrite)
				FlushFileBuffers(m_file);
			CloseHandle(m_file);
		}
	}

	bool PEParser::Initialize()
	{
		if(!m_view)
			return false;

		PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)m_view;

		if(IsBadReadPtr(dosHeader, sizeof(IMAGE_DOS_HEADER)))
			return false;

		if(dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
			return false;

		m_interesting.push_back(Block(L"DOS Stub", FileOffset(dosHeader), dosHeader->e_lfanew));

		PIMAGE_NT_HEADERS ntHeaders = MAKE_PTR(PIMAGE_NT_HEADERS, dosHeader, dosHeader->e_lfanew);

		if(!ntHeaders)
			return false;

		if(IsBadReadPtr(ntHeaders, sizeof(ntHeaders->Signature)))
			return false;

		if(ntHeaders->Signature != IMAGE_NT_SIGNATURE)
			return false;

		if(IsBadReadPtr(&ntHeaders->FileHeader, sizeof(IMAGE_FILE_HEADER)))
			return false;

		m_interesting.push_back(Block(L"PE header", FileOffset(ntHeaders), sizeof(ntHeaders->FileHeader) + ntHeaders->FileHeader.SizeOfOptionalHeader));

		if(IsBadReadPtr(&ntHeaders->OptionalHeader, ntHeaders->FileHeader.SizeOfOptionalHeader))
			return false;

		PIMAGE_SECTION_HEADER sectionHeaders = IMAGE_FIRST_SECTION(ntHeaders); 

		if(IsBadReadPtr(sectionHeaders, ntHeaders->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER)))
			return false;

		if(ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
			m_pe32Plus = false;
		else 
			if(ntHeaders->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
				m_pe32Plus = true;
			else
				return false;
		m_validPE = true;

		// Ignoring linker timestamp in PE header
		m_ignored.push_back(Block(L"PE timestamp", FileOffset(&(ntHeaders->FileHeader.TimeDateStamp)), sizeof(ntHeaders->FileHeader.TimeDateStamp)));
		// Ignoring linker checksum in PE header
		m_ignored.push_back(Block(L"PE checksum", FileOffset(&(ntHeaders->OptionalHeader.CheckSum)), sizeof(ntHeaders->OptionalHeader.CheckSum)));

		ReadSections(ntHeaders);
		ReadImportsDirectory(ntHeaders);
		ReadExportsDirectory(ntHeaders);
		ReadDebugDirectory(ntHeaders);
		ReadDigitalSignatureDirectory(ntHeaders);
		ReadResourceDirectory(ntHeaders);

		std::sort(m_ignored.begin(), m_ignored.end());
		std::sort(m_interesting.begin(), m_interesting.end());

		return true;
	}

	bool PEParser::ReadSections(PIMAGE_NT_HEADERS ntHeaders)
	{
		PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders); 

		m_interesting.push_back(Block(
			  L"Sections directory"
			, FileOffset(sectionHeader)
			, ntHeaders->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER)
		));

		for(int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++, sectionHeader++)
		{
			m_sections.push_back(Block(
				  MultiByteToWideString(std::string((char*)sectionHeader->Name, IMAGE_SIZEOF_SHORT_NAME))
				, sectionHeader->PointerToRawData
				, sectionHeader->SizeOfRawData
			));
			m_interesting.push_back(Block(
				L"Section: " + MultiByteToWideString(std::string((char*)sectionHeader->Name, IMAGE_SIZEOF_SHORT_NAME))
				, sectionHeader->PointerToRawData
				, sectionHeader->SizeOfRawData
			));
		}

		return true;
	}

	std::string PEParser::SectionData(const std::wstring& name)
	{
		auto block = std::find_if(m_sections.begin(), m_sections.end(), [&](const Block& block) { return block.description == name; });
		if(block == m_sections.end())
			return "";

		return std::string((const char*)((LPBYTE)m_view + block->offset), block->size);
	}

	bool PEParser::FindFileOffsetFromRva(PIMAGE_NT_HEADERS ntHeaders, DWORD rva, DWORD& fileOffset)
	{
		// file offset
		PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders); 

		bool found = false;
		for(int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++, sectionHeader++)
		{
			DWORD sectionSize = sectionHeader->Misc.VirtualSize;

			if(sectionSize == 0) // compensate for Watcom linker strangeness, according to Matt Pietrek 
				sectionSize = sectionHeader->SizeOfRawData; 

			if((rva >= sectionHeader->VirtualAddress) && (rva < sectionHeader->VirtualAddress + sectionSize) )
			{
				found = true;
				break;
			}
		}

		if(found) 
			fileOffset = rva + (int)sectionHeader->PointerToRawData - sectionHeader->VirtualAddress;

		return found;
	}

	bool PEParser::ReadImportsDirectory(PIMAGE_NT_HEADERS ntHeaders)
	{
		PEDirInfo<IMAGE_IMPORT_DESCRIPTOR> info;
		if(!DirectoryInfo(ntHeaders, info))
			return false;

		IMAGE_IMPORT_DESCRIPTOR* descriptor = info[0];
		while(descriptor->Characteristics)
		{
			DWORD fileOffset = descriptor->Name + info.SectionOffset();

			m_dllImports.push_back((char*)((LPBYTE)m_view + fileOffset));

			++descriptor;
		}

		PEDirInfo<ImgDelayDescr> delayLoadInfo;
		if(!DirectoryInfo(ntHeaders, delayLoadInfo))
			return true;

		ImgDelayDescr* delayLoadDescriptor = delayLoadInfo[0];
		while(delayLoadDescriptor->grAttrs == dlattrRva && delayLoadDescriptor->rvaDLLName)
		{
			DWORD fileOffset = 0;
			if(FindFileOffsetFromRva(ntHeaders, delayLoadDescriptor->rvaDLLName, fileOffset))
				m_dllDelayedImports.push_back((char*)((LPBYTE)m_view + fileOffset));
			++delayLoadDescriptor;
		}
	
		return true;
	}

	bool PEParser::ReadExportsDirectory(PIMAGE_NT_HEADERS ntHeaders)
	{
		PEDirInfo<IMAGE_EXPORT_DIRECTORY> info;
		if(!DirectoryInfo(ntHeaders, info))
			return false;

		m_ignored.push_back(Block(L"Export table timestamp", FileOffset(&info[0]->TimeDateStamp), sizeof(info[0]->TimeDateStamp)));

		return true;
	}

	void PEParser::AddIgnoredRange(const Block& block)
	{
		m_ignored.push_back(block);
	}

	void PEParser::AddIgnoredRange(const BlockList& blocks)
	{
		m_ignored.insert(m_ignored.end(), blocks.begin(), blocks.end());
	}

	void PEParser::PrintInfo(std::wostream& stream, bool verbose) const
	{
		boost::io::ios_base_all_saver ofs(stream);

		if(IsValidPE() || IsCorrupted())
		{
			stream.setf(std::ios::dec, std::ios::basefield);
			stream << L"Arch    : " << ((Is64Bit())?L"64 bit":L"32 bit") << L'\n';
			stream << L"Size    : " << (float)m_fileSize/1024 << L" Kb" << L'\n';
			stream << L"Version : " << FileVersion() << L'\n';
			stream << L"PDB     : " << PDBPath() << L'\n';
			stream << L"PDB GUID: " << PDBGUID() << L'\n'; 
			stream << L"Signed  : " << ((IsSigned())?"true (not verified)":"false") << L'\n';

			if(IsCorrupted())
				stream << L"\n\nFile is corrupted!!\n\n";

			stream.setf(std::ios::hex, std::ios::basefield);

			stream << L'\n' << L"Ignored offsets:" << L'\n';
			stream << m_ignored << L'\n';

			stream << L'\n';

			stream << L"Imports:" << L"\n";
			for(size_t i = 0 ; i < m_dllImports.size(); ++i)
				stream << "  " << MultiByteToWideString(m_dllImports[i]) << L'\n';
			stream << L'\n';
			stream << L"Delayed imports:" << L"\n";
			for(size_t i = 0 ; i < m_dllDelayedImports.size(); ++i)
				stream << "  " << MultiByteToWideString(m_dllDelayedImports[i]) << L'\n';
			stream << L'\n';

			if(verbose)
			{
				stream << L"File layout:" << L"\n";
				BlockNode tree(Block2(L"Whole file", 0, 0, FileSize()));
				tree.Add(m_interesting);
				tree.Add(m_ignored);
				tree.Add(m_resourceBlocks);
				tree.Sort();

				tree.Print(stream, L"");
			}
		}
		else
		{
			if(m_view)
				stream << L"Invalid PE format?" << L'\n';
			else
				stream << L"Failed to open file" << L'\n';
		}

		stream << std::flush;
	}

	bool PEParser::ReadResourceDirectory(PIMAGE_NT_HEADERS ntHeaders)
	{
		PEDirInfo<IMAGE_RESOURCE_DIRECTORY> info;
		if(!DirectoryInfo(ntHeaders, info))
			return false;

		ResourceDirectoryTable table(m_view, info, m_resourceBlocks);

		std::sort(m_resourceBlocks.begin(), m_resourceBlocks.end());
		m_resources = table.Root();

		ReadTypeLibrary(m_resources->AtPath(L"@TYPELIB"));
		ReadVsVersionInfo(m_resources->AtPath(L"16"));

		return true;
	}

	bool PEParser::ReadTypeLibrary(ResourceEntryPtr node)
	{
		if(!node) 
			return false;

		if(node->IsData())
			return ReadTypeLibrary(node->Address(), node->Size());

		bool ret = true;
		for(auto& entry : node->Entries())
			ret = ReadTypeLibrary(entry.second) && ret;
		return ret;
	}

	bool PEParser::ReadTypeLibrary(LPVOID data, size_t size)
	{
		if(!data) 
			return false;

        return false;
	
		// disabled because of dependency on GPL'ed code
	}

	bool PEParser::ReadVsVersionInfo(ResourceEntryPtr node)
	{
		if(!node) 
			return false;

		if(node->IsData())
			return ReadVsVersionInfo(node->Address(), node->Size());

		bool ret = true;
		for(auto& entry : node->Entries())
			ret = ReadVsVersionInfo(entry.second) && ret;
		return ret;
	}

	bool PEParser::ReadVsVersionInfo(LPVOID data, size_t size)
	{
		if(!data) 
			return false;

		VS_VersionInfo info(data, size);

		if(!info.IsWellFormed())
			return false;

		VS_FIXEDFILEINFO* fixedInfo = info.FixedFileInfo();

		m_ignored.push_back(Block(L"VS fixed: file version", FileOffset(&fixedInfo->dwFileVersionMS), 2*sizeof(fixedInfo->dwFileVersionMS)));
		m_modifiable.emplace(ModifiableBlocks::FileVersion, m_ignored.back());
		m_ignored.push_back(Block(L"VS fixed: product version", FileOffset(&fixedInfo->dwProductVersionMS), 2*sizeof(fixedInfo->dwProductVersionMS)));
		m_modifiable.emplace(ModifiableBlocks::ProductVersion, m_ignored.back());

		std::wstringstream ss; ss
			<< (fixedInfo->dwFileVersionMS >> 8*sizeof(WORD)) << L"."
			<< (fixedInfo->dwFileVersionMS & 0x0000FFFF) << L"."
			<< (fixedInfo->dwFileVersionLS >> 8*sizeof(WORD)) << L"." 
			<< (fixedInfo->dwFileVersionLS & 0x0000FFFF) 
		;
		m_fileVersion = ss.str();

		m_ignored.push_back(Block(L"VS fixed: date", FileOffset(&fixedInfo->dwFileDateMS), 2*sizeof(DWORD)));

		for(auto& entry : info.Entries())
		{
			if(!entry || !entry->IsWellFormed()) 
				continue;

			for(auto& table : entry->Entries())
			{
				if(!table || !table->IsWellFormed()) 
					continue;

				std::wstring comment = L"VS " + table->Name() + L":";

				void* value = 0;
				size_t valSize = 0;
				if(table->OriginalValue(L"FileVersion", &value, &valSize))
				{
					m_ignored.push_back(Block(comment + L" FileVersion", FileOffset(value), valSize));
					m_modifiable.insert(std::make_pair(FileVersionString, m_ignored.back()));
				}

				if(table->OriginalValue(L"ProductVersion", &value, &valSize))
				{
					m_ignored.push_back(Block(comment + L" ProductVersion", FileOffset(value), valSize));
					m_modifiable.insert(std::make_pair(ProductVersionString, m_ignored.back()));
				}
			}
		}

		return true;
	}

	bool PEParser::ReadDigitalSignatureDirectory(PIMAGE_NT_HEADERS ntHeaders)
	{
		DWORD certTableRva = 0;
		DWORD certTableSize = 0;

		if(m_pe32Plus)
		{
			PIMAGE_OPTIONAL_HEADER64 header = (PIMAGE_OPTIONAL_HEADER64)&ntHeaders->OptionalHeader;
			certTableRva = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress;
			certTableSize = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size;

			// Always ignoring security entry
			m_ignored.push_back(Block(
				  L"Digital signature directory entry"
				, FileOffset(&header->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY])
				, sizeof(header->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY])
			));	

			m_modifiable.insert(std::make_pair(SignatureDirectory, m_ignored.back()));
		}
		else
		{
			PIMAGE_OPTIONAL_HEADER32 header = (PIMAGE_OPTIONAL_HEADER32)&ntHeaders->OptionalHeader;
			certTableRva = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].VirtualAddress;
			certTableSize = header->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY].Size;

			// Always ignoring security entry
			m_ignored.push_back(Block(
				  L"Digital signature directory entry"
				, FileOffset(&header->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY])
				, sizeof(header->DataDirectory[IMAGE_DIRECTORY_ENTRY_SECURITY])
			));

			m_modifiable.insert(std::make_pair(SignatureDirectory, m_ignored.back()));
		}

		if(certTableRva == 0 && certTableSize == 0)
			return false;

		m_signed = true;

		m_ignored.push_back(Block(
			  L"Digital signature section"
			, certTableRva
			, certTableSize
		));

		if(certTableRva + certTableSize > FileSize())
		{
			m_validPE = false;
			m_signed = false;
			m_corrupted = true;
			std::wcerr << "Digital signature section points beyond file limits" << std::endl;
		}

		return true;
	}

	bool PEParser::ReadDebugDirectory(PIMAGE_NT_HEADERS ntHeaders)
	{
		PEDirInfo<IMAGE_DEBUG_DIRECTORY> info;
		if(!DirectoryInfo(ntHeaders, info))
			return false;

		for(size_t i = 0; i < info.Count(); i++)
		{
			m_ignored.push_back(Block(L"Debugger timestamp", FileOffset(&(info[i]->TimeDateStamp)), sizeof(info[i]->TimeDateStamp)));
			switch(info[i]->Type)
			{
			case IMAGE_DEBUG_TYPE_UNKNOWN: 
				continue;
			case IMAGE_DEBUG_TYPE_CODEVIEW:
				{
					LPBYTE debugInfo = (LPBYTE)m_view + info[i]->PointerToRawData;
					if(IsBadReadPtr(debugInfo, info[i]->SizeOfData))
						return false;

					if(info[i]->SizeOfData < sizeof(DWORD))				
						return false;

					DWORD cvSignature = *(DWORD*)debugInfo;

					m_ignored.push_back(Block(L"PDB section", FileOffset(debugInfo), info[i]->SizeOfData));

					if (cvSignature == 'SDSR')
					{
						if(IsBadReadPtr(debugInfo, sizeof(_RSDSI)))
							return false;

						_RSDSI* cvInfo = (_RSDSI*)debugInfo;

						if(IsBadStringPtrA((CHAR*)cvInfo->szPdb, UINT_MAX)) 
							return false; 

						m_pdbGuid.resize(39);
						int size = ::StringFromGUID2(cvInfo->guidSig, &m_pdbGuid[0], (int)m_pdbGuid.size());
						m_pdbGuid.resize(size - 1); // \0

						m_ignored.push_back(Block(L"PDB 7.00 guid", FileOffset(&cvInfo->guidSig), sizeof(cvInfo->guidSig)));
						m_ignored.push_back(Block(L"PDB 7.00 age", FileOffset(&cvInfo->age), sizeof(cvInfo->age)));

						size_t length = strlen((char*)cvInfo->szPdb);
						m_pdbPath = MultiByteToWideString(std::string((char*)cvInfo->szPdb, length));
						m_ignored.push_back(Block(L"PDB 7.00 file path", FileOffset(&cvInfo->szPdb), length*sizeof(char)));
					} 
				}
			}
		}

		return true;
	}

	template <class T>
	bool PEParser::DirectoryInfo(PIMAGE_NT_HEADERS ntHeaders, PEDirInfo<T>& dir)
	{
		if(m_pe32Plus)
		{
			PIMAGE_OPTIONAL_HEADER64 header = (PIMAGE_OPTIONAL_HEADER64)&ntHeaders->OptionalHeader;
			dir.SetRva(header->DataDirectory[dir.Index()].VirtualAddress);
			dir.SetSize(header->DataDirectory[dir.Index()].Size);
		}
		else
		{
			PIMAGE_OPTIONAL_HEADER32 header = (PIMAGE_OPTIONAL_HEADER32)&ntHeaders->OptionalHeader;
			dir.SetRva(header->DataDirectory[dir.Index()].VirtualAddress);
			dir.SetSize(header->DataDirectory[dir.Index()].Size);
		}

		if(dir.Rva() == 0 && dir.Size() == 0)
			return false; 

		if(dir.Size() < sizeof(T))
			return false;

		DWORD fileOffset = 0;
		if(!FindFileOffsetFromRva(ntHeaders, dir.Rva(), fileOffset))
			return false;

		dir.SetFileOffset(fileOffset);
		dir.SetSectionOffset(dir.FileOffset() - dir.Rva());

		// directory
		T* directory = MAKE_PTR(T*, m_view, dir.FileOffset());
		if(IsBadReadPtr(directory, dir.Size()))
			return false;

		dir.SetDirectory(directory);

		return true;
	}

	size_t PEParser::FileOffset(void* pointer) const
	{
		return (size_t)pointer - (size_t)m_view;
	}

	size_t PEParser::TotalIgnoredSize() const
	{
		size_t size = 0;
		for(size_t i = 0; i < m_ignored.size(); i++)
			size += m_ignored[i].size;
		return size;
	}

	size_t PEParser::NextOffset(size_t currentOffset, size_t& sizeOfBlock, size_t maxSize) const
	{
		size_t endOfBlock = maxSize;
		size_t newOffset = currentOffset;

		for(size_t i = 0; i < m_ignored.size(); i++)
		{
			Block block = m_ignored[i];

			if(block.offset + block.size < newOffset)
				continue; // past whole block already

			if(block.offset <= newOffset && block.offset + block.size > newOffset)
			{
				newOffset = block.offset + block.size;
				continue; // inside the block, moving current offset to the end of the block
			}

			if(block.offset > newOffset)
			{
				endOfBlock = block.offset;
				break; // outside of the block, setting size of next relevant block
			}
		}

		if(newOffset > maxSize)
			sizeOfBlock = endOfBlock - maxSize;
		else
			sizeOfBlock = endOfBlock - newOffset;

		return newOffset;
	}

	CompareResult PEParser::Compare(const PEParser& p1, const PEParser& p2, bool fast, bool noHeuristics, bool verbose)
	{
		CompareResult result;

		result.SetVerbose(verbose);

		if(!p1.IsOpen() || !p2.IsOpen())
		{
			result.m_error = true;
			return result;
		}

		if(!p1.IsValidPE() || !p2.IsValidPE())
			result.m_wrongFormat = true;

		result.m_tree.reset(new BlockNode(Block2(L"File 1", 0, 0, p1.FileSize())));
		if(verbose)
		{
			result.m_tree->Add(p1.m_interesting);
			result.m_tree->Add(p1.m_ignored);
			result.m_tree->Add(p1.m_resourceBlocks);
		}

		result.m_differentSize = p1.FileSize() - p1.TotalIgnoredSize() != p2.FileSize() - p2.TotalIgnoredSize();

		result.m_differentPath = lstrcmpi(p1.PDBPath().c_str(), p2.PDBPath().c_str()) != 0;
		result.m_differentPathLength = p1.PDBPath().size() != p1.PDBPath().size();

		// Comparing for identical
		if(p1.FileSize() == p2.FileSize() && 0 == memcmp(p1.m_view, p2.m_view, p1.FileSize()))
		{
			result.m_identical = true;
			result.m_equivalent = true;
			return result;
		}

		bool firstBlockFound = false;
		// Comparing for equivalent
		if(fast)
		{
			result.m_fast = true;
			if(!result.m_identical && !result.m_differentSize)
			{ 
				LPBYTE index1 = (LPBYTE)p1.m_view;
				LPBYTE index2 = (LPBYTE)p2.m_view;

				size_t offset1 = 0;
				size_t offset2 = 0;

				result.m_equivalent = true;
				while(true)
				{
					size_t size1 = 0;
					size_t size2 = 0;

					offset1 = p1.NextOffset(offset1, size1, p1.FileSize());
					offset2 = p2.NextOffset(offset2, size2, p2.FileSize());

					size_t currentBlockSize = min(size1, size2);

					if(currentBlockSize == 0)
						break;

					if(0 != memcmp(index1 + offset1, index2 + offset2, currentBlockSize))
					{
						result.m_interesting.push_back(Block2(L"First different block (1)", offset1, offset2, currentBlockSize));
						result.m_equivalent = false;
						break;
					}

					offset1 += currentBlockSize;
					offset2 += currentBlockSize;
				}
			}
		}
		else
		{
			result.m_fast = false;

			LPBYTE start1 = (LPBYTE)p1.m_view;
			LPBYTE start2 = (LPBYTE)p2.m_view;

			size_t index = 0;
			size_t offset1 = 0;
			size_t offset2 = 0;
			size_t currentBlockSize = 0;

			size_t diffStart1 = 0;
			size_t diffStart2 = 0;
			size_t diffSize = 0;
			size_t diffShift = 0;
			bool diff = false;

			enum
			{
				  NEXT_BLOCK
				, DIFF_START
				, DIFF_END
				, END_BLOCK
				, COMPARE_BLOCK
			};

			int step = NEXT_BLOCK;
			while(true)
			{
				switch(step)
				{
				case NEXT_BLOCK:
					{
						size_t size1 = 0;
						size_t size2 = 0;

						offset1 = p1.NextOffset(offset1, size1, p1.FileSize());
						offset2 = p2.NextOffset(offset2, size2, p2.FileSize());

						currentBlockSize = min(size1, size2);

						if(currentBlockSize == 0)
							if(size1 == 0)
								result.m_different += p2.FileSize() - offset2;
							else
								result.m_different += p1.FileSize() - offset1;

						index = 0;

						step = COMPARE_BLOCK;
						break;
					}
				case END_BLOCK:
					{
						offset1 += currentBlockSize;
						offset2 += currentBlockSize;

						step = NEXT_BLOCK;
						break;
					}
				case DIFF_START:
					{
						diff = true;
						diffStart1 = offset1 + index-1;
						diffStart2 = offset2 + index-1;

						step = COMPARE_BLOCK;
						break;
					}
				case DIFF_END:
					{
						diff = false;
						step = COMPARE_BLOCK;
						diffSize = offset1 + index-1 - diffStart1;

						if(diffSize == 0) break; // one file ended before another one

						if(!noHeuristics && FilterDifference(result, p1, p2, diffStart1, diffStart2, diffSize, diffShift))
						{
							result.m_same += diffSize;
							index += diffShift;
						}
						else
						{
							result.m_different += diffSize;
							if(verbose)
								result.m_diffs.push_back(Block2(L">-< Difference >-<", diffStart1, diffStart2, diffSize));

							if(!firstBlockFound)
							{
								result.m_interesting.push_back(Block2(L">-< Difference >-<", diffStart1, diffStart2, diffSize));
								firstBlockFound = true;
							}
						}
						break;
					}
				case COMPARE_BLOCK:
					{
						if(index >= currentBlockSize)
						{
							if(diff)
								step = DIFF_END;
							else
								step = END_BLOCK;
							break;
						}

						if(*(LPBYTE)(start1 + offset1 + index) == *(LPBYTE)(start2 + offset2 + index))
						{
							++result.m_same;
							if(diff) step = DIFF_END;
						}
						else
						{
							if(!diff) step = DIFF_START;
						}

						++index;
						break;
					}
				}

				if(currentBlockSize == 0)
					break;
			}

			result.m_equivalent = result.m_different == 0;
		}

		result.m_equivalent = result.m_equivalent && !result.IsWrongFormat();

		return result;
	}

	bool PEParser::FilterDifference(CompareResult& result, const PEParser& p1, const PEParser& p2, size_t start1, size_t start2, size_t size, size_t& diffShift)
	{
		diffShift = 0;

		if(size == 0) 
			return false;

		if(DetectFILEMacro(p1, p2, start1, start2, size, diffShift))
		{
			result.m_dynamicIgnored.push_back(Block2(L"__FILE__", start1, start2, size));
			return true;
		}
		if(DetectTIMEMacro(p1, p2, start1, start2, size, diffShift))
		{
			result.m_dynamicIgnored.push_back(Block2(L"__TIME__", start1, start2, diffShift));
			return true;
		}
		if(DetectDATEMacro(p1, p2, start1, start2, size, diffShift))
		{
			result.m_dynamicIgnored.push_back(Block2(L"__DATE__", start1, start2, diffShift));
			return true;
		}
		if(DetectMIDLMarker(p1, p2, start1, start2, size, diffShift))
		{
			result.m_dynamicIgnored.push_back(Block2(L"MIDL marker", start1, start2, diffShift));
			return true;
		}

		return false;
	}

	template<> std::basic_string<wchar_t> PEParser::PDBPathT<wchar_t>() const
	{
		return PDBPath();
	}

	template<> std::basic_string<char> PEParser::PDBPathT<char>() const
	{
		return WideStringToMultiByte(PDBPath());
	}

	template <class CharT>
	bool PEParser::DetectFILEMacro(size_t diffStart, size_t diffSize) const
	{
		if(diffSize > 5) 
			return false; 

		std::basic_string<CharT> pdb = PDBPathT<CharT>();

		if(pdb.size() < 3) 
			return false;
		if(diffStart < sizeof(CharT)*pdb.size()) 
			return false;

		CharT* diffPoint = (CharT*)((LPBYTE)m_view + diffStart);
		const CharT* pathStart = FindStringEntry<CharT, Path>(diffPoint - pdb.size(), pdb, 3, pdb.size());

		if(!pathStart) 
			return false;
		if(size_t(diffPoint - pathStart) > pdb.size()) 
			return false; 

		return CompareStrings<CharT, Path>(pathStart, pdb.data(), diffPoint - pathStart);
	}

	bool PEParser::DetectFILEMacro(const PEParser& p1, const PEParser& p2, size_t diffStart1, size_t diffStart2, size_t diffSize, size_t& diffShift)
	{
		// __FILE__ full path to a header or cpp

		// find start of difference (p1)
		// find end of difference (p4)
		// go back by pdb path length (p2)
		// find if there is drive letter (1st 3 chars from pdb path) between 1 and 2 (p3)
		// find if string (p3-p4) is matches start of pdb path
		// if that is true for both files, ignore the difference
		// no shift

		diffShift = 0;
		return 
			(
			   p1.DetectFILEMacro<wchar_t>(diffStart1, diffSize) 
			&& p2.DetectFILEMacro<wchar_t>(diffStart2, diffSize)
			)
			|| 
			(
			   p1.DetectFILEMacro<char>(diffStart1, diffSize) 
			&& p2.DetectFILEMacro<char>(diffStart2, diffSize)
			)
		;
	}

	template <class CharT>
	bool PEParser::DetectTIMEMacro(size_t diffStart, size_t diffSize, size_t& diffShift) const
	{
		diffShift = 0;

		if(diffSize > 2) 
			return false; // wide char will have 1 byte diff, narrow will have max 2 bytes (between colons)

		const CharT* diffPoint = (CharT*)((LPBYTE)m_view + diffStart);

		const CharT* colon = NULL;
		const int colonPoints[] = { -2, -1, +1, +2 }; 

		for(size_t i = 0; i < sizeof(colonPoints)/sizeof(int); ++i)
			if(*(diffPoint + colonPoints[i]) == Literals<char>::colon)
			{
				colon = diffPoint + colonPoints[i];
				break;
			}

		if(!colon) 
			return false;

		const CharT* null = NULL;
		const int nullPoints[] = { +3, +6 }; 

		for(size_t i = 0; i < sizeof(nullPoints)/sizeof(int); ++i)
			if(*(colon + nullPoints[i]) == Literals<char>::null)
			{
				null = colon + nullPoints[i];
				break;
			}

		if(!null) 
			return false;

		int hours = 0;
		int minutes = 0;
		int seconds = 0;

		std::basic_stringstream<CharT> ss;
	
		ss.str(std::basic_string<CharT>(null - 8, 2));
		if(!(ss >> hours) || 2 != ss.tellg()  || hours > 23 || hours < 0) 
			return false;

		ss.clear();
		ss.str(std::basic_string<CharT>(null - 5, 2));
		if(!(ss >> minutes) || 2 != ss.tellg() || minutes > 59 || minutes < 0) 
			return false;

		ss.clear();
		ss.str(std::basic_string<CharT>(null - 2, 2));
		if(!(ss >> seconds) || 2 != ss.tellg() || seconds > 59 || seconds < 0) 
			return false;

		const CharT* diffEnd = (CharT*)((LPBYTE)m_view + diffStart + diffSize);
		diffShift = sizeof(CharT)*size_t(null - diffEnd);

		// TODO: verify that time is close to time of build

		return true;
	}

	bool PEParser::DetectTIMEMacro(const PEParser& p1, const PEParser& p2, size_t start1, size_t start2, size_t size, size_t& diffShift)
	{
		// __TIME__ "hh:mm:ss\0"

		// difference must be no more than 2 chars 
		// find start of difference (p1)
		// find end of difference (p2)
		// find ':' at p1-1c, p1-2c, p1+1c or p1+2c (p3)
		// find '\0' on position p3.x+3c or p3.x+6c (p4)
		// make sure there is ':' on positions p4-3c and p4-6c
		// read time and make sure all numbers are in range
		// fuzzily verify that time is close to some other known time of build
		// shifts to p4 (by p4-p2)

		size_t diffShift1 = 0;
		size_t diffShift2 = 0;

		if(p1.DetectTIMEMacro<wchar_t>(start1, size, diffShift1) && p2.DetectTIMEMacro<wchar_t>(start2, size, diffShift2))
		{
			if(diffShift1 == diffShift2)
			{
				diffShift = diffShift1;
				return true;
			}
		}

		if(p1.DetectTIMEMacro<char>(start1, size, diffShift1) && p2.DetectTIMEMacro<char>(start2, size, diffShift2))
		{
			if(diffShift1 == diffShift2)
			{
				diffShift = diffShift1;
				return true;
			}
		}

		return false;
	}

	template <class CharT> 
	bool PEParser::DetectDATEMacro(size_t diffStart, size_t diffSize, size_t& diffShift) const
	{
		diffShift = 0;

		if(diffSize > 4) 
			return false; // wide string would have difference of 1 byte, narrow would have up to 4 (year)

		const CharT* diffPoint = (CharT*)((LPBYTE)m_view + diffStart);

		const CharT* dateStart = NULL;
		size_t length = 0;
		for(size_t i = 0; i < 11; ++i)
		{
			std::basic_string<CharT> month(Literals<CharT>::month[i]);
		
			dateStart = FindStringEntry<CharT, None>(diffPoint - (11 - diffSize), month, month.size(), 11);
			if(!dateStart)
				continue;

			dateStart += month.size();
			length = 11 - month.size();
			break;
		}

		if(!dateStart) 
			return false;

		std::basic_stringstream<CharT> ss(std::basic_string<CharT>(dateStart, length));

		int day = 0;
		int year = 0;
		if(!(ss >> day) || !(ss >> year)) 
			return false;

		if(length == ss.tellg())
		{
			const CharT* diffEnd = (CharT*)((LPBYTE)m_view + diffStart + diffSize);

			diffShift = sizeof(CharT)*size_t(dateStart + length - diffEnd);
			return true;
		}

		// TODO: verify that this is the date of the build somehow

		return false;
	}

	bool PEParser::DetectDATEMacro(const PEParser& p1, const PEParser& p2, size_t start1, size_t start2, size_t size, size_t& diffShift)
	{
		// __DATE__ "Mmm dd yyyy\0"

		// find start of difference (p1)
		// find end of difference (p2)
		// go back by 11-(p2-p1) -- date string will start within 11 chars of the end of difference
		// find one of the months names (p3)
		// read day and year
		// check against some other date
		// if verified for both binaries ignore the difference
		// shifts to p3+11 (by p3+11-p2)

		size_t diffShift1 = 0;
		size_t diffShift2 = 0;

		if(p1.DetectDATEMacro<wchar_t>(start1, size, diffShift1) && p2.DetectDATEMacro<wchar_t>(start2, size, diffShift2))
		{
			if(diffShift1 == diffShift2)
			{
				diffShift = diffShift1;
				return true;
			}
		}

		if(p1.DetectDATEMacro<char>(start1, size, diffShift1) && p2.DetectDATEMacro<char>(start2, size, diffShift2))
		{
			if(diffShift1 == diffShift2)
			{
				diffShift = diffShift1;
				return true;
			}
		}

		return false;
	}

	bool PEParser::DetectMIDLMarker(const PEParser& p1, const PEParser& p2, size_t start1, size_t start2, size_t size, size_t& diffShift)
	{
		diffShift = 0;

		size_t diffShift1 = 0;
		size_t diffShift2 = 0;

		if(p1.DetectMIDLMarker(start1, size, diffShift1) && p2.DetectMIDLMarker(start2, size, diffShift2))
		{
			if(diffShift1 == diffShift2)
			{
				diffShift = diffShift1;
				return true;
			}
		}

		return false;
	}

	bool PEParser::DetectMIDLMarker(size_t diffStart, size_t diffSize, size_t& diffShift) const
	{
        diffShift = 0;

        const auto it = std::find_if(cbegin(m_resourceBlocks), cend(m_resourceBlocks), [](const Block& block) {
            return std::wstring::npos != block.description.find(L"@TYPELIB");
        });

        if (cend(m_resourceBlocks) != it)
        {
            const std::string magic = "Created by MIDL version";

            const char* stringStart = FindStringEntry< char, None >(
                (char*)m_view + it->offset, magic, magic.size(), it->size);

            if (stringStart == nullptr)
                return false;

            const struct
            {
                const unsigned char byte;
                const size_t offset;

                bool check(const void* data) const noexcept {
                    return data && byte == *((unsigned char *)data + offset);
                }
            }
            nl{ 0x0a, 61 }, dc3{ 0x13, 62 };

            if (nl.check(stringStart) && dc3.check(stringStart))
            {
                const size_t tlbStampLength = 65;
                const auto diffPos = (char*)m_view + diffStart;

                if (stringStart < diffPos && diffPos < stringStart + tlbStampLength)
                    return true;
            }
        }

        return false;
	}

	bool PEParser::SetVersion(const VersionString& version, VersionField field) const
	{
		if(!m_openForWrite) 
			return false;
		if(!version.IsValid()) 
			return false;

		for (auto& entry : m_modifiable)
		{
			if((entry.first == ModifiableBlocks::FileVersion && field != ProductOnly) || (entry.first == ModifiableBlocks::ProductVersion && field != FileOnly))
			{
				if(entry.second.size != 2*sizeof(DWORD))
					return false;

				DWORD lower = (WORD)version.Major();
				lower = (lower << 8*sizeof(WORD)) + (WORD)version.Minor();
				DWORD higher = (WORD)version.Build();
				higher = (higher << 8*sizeof(WORD)) + (WORD)version.Patch();

				*(DWORD*)((LPBYTE)m_view + (DWORD)entry.second.offset) = lower;
				*(DWORD*)((LPBYTE)m_view + (DWORD)entry.second.offset + sizeof(DWORD)) = higher;

				continue;
			}

			if((entry.first == ModifiableBlocks::FileVersionString && field != ProductOnly) || (entry.first == ModifiableBlocks::ProductVersionString && field != FileOnly))
			{
				std::wstring newVer = version.FullFormatted();

				if(size_t(entry.second.size) < sizeof(wchar_t) * (newVer.size() + 1))
				{
					std::wcerr << L"New version won't fit. Rebuilding resource table not implemented" << std::endl;
					return false;
				}

				if(size_t(entry.second.size) > sizeof(wchar_t) * (newVer.size() + 1))
				{
					std::wstring padding((entry.second.size/sizeof(wchar_t)) - newVer.size() - 1, L' ');
					newVer = newVer + padding;
				}

				memcpy_s(
					  ((LPBYTE)m_view + (DWORD)entry.second.offset)
					, entry.second.size
					, newVer.c_str()
					, sizeof(wchar_t) * newVer.size()
				);

				continue;
			}
		}

		return true;
	}

	void PEParser::EraseSignatureDirectory() const
	{
		ModifiableBlockMap::const_iterator it = m_modifiable.find(SignatureDirectory);
		if(it == m_modifiable.end()) 
			return;
	
		memset((LPBYTE)m_view + (DWORD)it->second.offset, 0, it->second.size);
	}
// ================================================================================================
}
