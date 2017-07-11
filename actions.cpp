// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "actions.h"

#include "peparser.h"
#include "widestring.h"
#include "resourcepath.h"
#include "signer.h"
#include "etoken.h"
#include "dependencycheck.h"

#pragma warning(push)
#pragma warning(disable : 4996)
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#pragma warning(pop)

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

namespace po = boost::program_options;

namespace peparser
{
	template <class CharT> std::basic_streambuf<CharT>* GetRdbuf();
	template <> std::basic_streambuf<char>* GetRdbuf() { return std::cout.rdbuf(); }
	template <> std::basic_streambuf<wchar_t>* GetRdbuf() { return std::wcout.rdbuf(); }

	template <class CharT>
	std::shared_ptr<std::basic_ostream<CharT>> OpenOutput(const po::variables_map& variables)
	{
		if (variables.count("output"))
		{
			auto outputPath = variables["output"].as<std::wstring>();
			std::shared_ptr<std::basic_ofstream<CharT>> out(new std::basic_ofstream<CharT>(outputPath.c_str(), std::ios_base::trunc | std::ios_base::binary));
			if (!out->is_open())
			{
				std::wcerr << L"Failed to open output file for writing: " << outputPath << std::endl;
				return std::shared_ptr<std::basic_ostream<CharT>>();
			}
			return out;
		}
		else
			return std::shared_ptr<std::basic_ostream<CharT>>(new std::basic_ostream<CharT>(GetRdbuf<CharT>()));
	}

	void Info(const po::variables_map& variables, int& retcode)
	{
		retcode = 1;

		if (!variables.count("input"))
		{
			std::wcerr << L"Error parsing options: must have some input files." << std::endl;
			return;
		}

		auto out = OpenOutput<wchar_t>(variables);
		if (!out) 
			return;

		retcode = 0;

		auto inputs = variables["input"].as<std::vector<std::wstring>>();
		for (auto& input : inputs)
		{
			*out << input << L":\n\n";

			PEParser pe(input);

			pe.Open();
			pe.PrintInfo(*out, true);

			if (!pe.IsValidPE())
				retcode = 1;

			*out << L"\n\n";
		}
	}

	void Pdb(const po::variables_map& variables, int& retcode)
	{
		retcode = 1;

		if (!variables.count("input"))
		{
			std::wcerr << L"Error parsing options: must have some input files." << std::endl;
			return;
		}

		auto out = OpenOutput<wchar_t>(variables);
		if (!out) 
			return;

		auto inputs = variables["input"].as<std::vector<std::wstring>>();
		for (auto& input : inputs)
		{
			PEParser pe(input);
			pe.Open();

			if (!pe.IsValidPE() || pe.PDBPath().empty())
				continue;

			*out << pe.PDBGUID() << " " << pe.PDBPath() << L'\n';
		}

		*out << std::flush;
		retcode = 0;
	}

	void Version(const po::variables_map& variables, int& retcode)
	{
		retcode = 1;

		if (!variables.count("input"))
		{
			std::wcerr << L"Error parsing options: must have one input file." << std::endl;
			return;
		}

		auto out = OpenOutput<wchar_t>(variables);
		if (!out) 
			return;

		auto inputs = variables["input"].as<std::vector<std::wstring>>();

		PEParser pe(inputs[0]);
		pe.Open();

		if (!pe.IsValidPE())
			return;

		*out << pe.FileVersion() << std::endl;

		retcode = 0;
	}

	void Imports(const po::variables_map& variables, int& retcode)
	{
		retcode = 1;

		if (variables.count("input") != 1)
		{
			std::wcerr << L"Error parsing options: must have one input file." << std::endl;
			return;
		}

		auto out = OpenOutput<wchar_t>(variables);
		if (!out) 
			return;

		retcode = 0;

		auto inputs = variables["input"].as<std::vector<std::wstring>>();

		PEParser pe(inputs[0]);
		pe.Open();

		if (!pe.IsValidPE())
		{
			retcode = 1;
			return;
		}

		auto imports = pe.AllDllImports();

		for (auto& import : imports)
			*out << MultiByteToWideString(import) << L'\n';
	}

	void Compare(const po::variables_map& variables, int& retcode)
	{
		retcode = 1;

		std::vector<std::wstring> inputs;
		if (variables.count("input"))
			inputs = variables["input"].as<std::vector<std::wstring>>();

		if (inputs.size() != 2)
		{
			std::wcerr << L"Error parsing options: must have 2 input files." << std::endl;
			return;
		}

		auto out = OpenOutput<wchar_t>(variables);
		if (!out) 
			return;

		bool fast = variables["fast"].as<bool>();
		bool noHeuristics = variables["no-heuristics"].as<bool>();
		bool verbose = variables["verbose"].as<bool>();
        bool tlbTimestamp = variables["tlb-timestamp"].as<bool>();

		BlockList ignoredRanges = boost::lexical_cast<BlockList>(variables["r"].as<std::wstring>());
		BlockList ignoredRanges1 = boost::lexical_cast<BlockList>(variables["r1"].as<std::wstring>());
		BlockList ignoredRanges2 = boost::lexical_cast<BlockList>(variables["r2"].as<std::wstring>());

		PEParser pe1(inputs[0]);
		PEParser pe2(inputs[1]);

		pe1.AddIgnoredRange(ignoredRanges1);
		pe1.AddIgnoredRange(ignoredRanges);
		pe2.AddIgnoredRange(ignoredRanges2);
		pe2.AddIgnoredRange(ignoredRanges);

		pe1.Open();
		pe2.Open();

		*out
			<< inputs[0] << L":\n\n" << pe1
			<< inputs[1] << L":\n\n" << pe2
			<< std::endl;

		auto result = PEParser::Compare(pe1, pe2, fast, noHeuristics, verbose, tlbTimestamp);

		*out << result << std::endl;

		bool success = false;

		bool identical = variables["identical"].as<bool>();
		if (identical)
			success = result.IsIdentical();
		else
			success = result.IsEquivalent();

		retcode = (success) ? 0 : 1;
	}

	void Signature(const po::variables_map& variables, int& retcode)
	{
		auto foo = { 1, 2, 3 };
		foo.size();

		retcode = 1;
		if (!variables.count("input"))
		{
			std::wcerr << L"Error parsing options: must have some input files." << std::endl;
			return;
		}

		auto out = OpenOutput<wchar_t>(variables);
		if (!out) 
			return;

		retcode = 0;

		auto inputs = variables["input"].as<std::vector<std::wstring>>();
		for (auto& input : inputs)
		{
			PEParser pe(input);
			pe.Open();

			if (!pe.IsSigned()) retcode = 1;

			*out << ((pe.IsSigned()) ? L"signed" : L"unsigned") << L" : " << input << std::endl;
		}
	}

	void DumpSection(const po::variables_map& variables, int& retcode)
	{
		retcode = 1;

		if (!variables.count("input"))
		{
			std::wcerr << L"Error parsing options: must have one input file." << std::endl;
			return;
		}

		auto out = OpenOutput<char>(variables);
		if (!out) 
			return;

		retcode = 0;

		auto inputs = variables["input"].as<std::vector<std::wstring>>();

		PEParser pe(inputs[0]);
		pe.Open();

		if (!pe.IsValidPE())
		{
			retcode = 1;
			std::wcerr << L"Invalid PE format." << std::endl;
			return;
		}

		auto sectionName = variables["dump-section"].as<std::wstring>();

		*out << pe.SectionData(sectionName) << std::flush;
	}

	void DumpResource(const po::variables_map& variables, int& retcode)
	{
		retcode = 1;

		std::vector<std::wstring> inputs;

		if (variables.count("input"))
			inputs = variables["input"].as<std::vector<std::wstring> >();

		if (inputs.size() != 1)
		{
			std::wcerr << L"Error parsing options: must have 1 input file." << std::endl;
			retcode = 1;
			return;
		}

		auto out = OpenOutput<char>(variables);
		if (!out) 
			return;

		auto resource = variables["dump-resource"].as<std::wstring>();

		PEParser pe(inputs[0]);

		pe.Open();
		if (!pe.IsValidPE())
			return;

		ResourceEntryPtr res = pe.ResourceDirectory();
		if (!res)
		{
			std::wcerr << L"No resource section found." << std::endl;
			return;
		}

		ResourceEntryPtr target = res->AtPath(resource);

		if (target && !target->IsData() && target->Entries().size() == 1)
			target = target->Entries().begin()->second;

		if (!target)
		{
			std::wcerr << L"Resource not found." << std::endl;
			return;
		}

		target->Save(*out);

		retcode = 0;
	}

	void DeleteResource(const boost::program_options::variables_map& variables, int& retcode)
	{
		retcode = 1;

		std::vector<std::wstring> inputs;
		if (variables.count("input"))
			inputs = variables["input"].as<std::vector<std::wstring>>();
		if (inputs.size() != 1)
		{
			std::wcerr << L"Error parsing options: must have 1 input file." << std::endl;
			return;
		}

		auto binaryPath = inputs[0];

		if (variables.count("resource") != 1)
		{
			std::wcerr << L"Error parsing options: must have valid resource path (type/name/lang)." << std::endl;
			return;
		}

		auto resource = variables["delete-resource"].as<std::wstring>();

		retcode = 0;

		std::vector<WORD> lang_ids;
		bool allLanguages = true;
		ResourcePath path;

		if (!ParsePath(binaryPath, resource, path, lang_ids, allLanguages))
		{
			retcode = 1;
			return;
		}

		HANDLE binaryHandle = BeginUpdateResource(binaryPath.c_str(), FALSE);
		if (!binaryHandle)
		{
			std::wcerr << L"File is missing, locked or has invalid format." << std::endl;
			retcode = 1;
			return;
		}

		for (auto lang : lang_ids)
		{
			if (!UpdateResource(binaryHandle, path.Type(), path.Name(), lang, NULL, 0))
			{
				std::wcerr << L"Failed to update resource. Error: " << GetLastError() << std::endl;
				retcode = 1;
				break;
			}
		}

		if (!EndUpdateResource(binaryHandle, retcode != 0))
			retcode = 1;
	}

	void DeleteSignature(const po::variables_map& variables, int& retcode)
	{
		retcode = 1;
		if (!variables.count("input"))
		{
			std::wcerr << L"Error parsing options: must have some input files." << L'\n';
			return;
		}
		auto inputs = variables["input"].as<std::vector<std::wstring>>();

		retcode = 0;
		for (auto& input : inputs)
		{
			std::wcout << input << std::endl;

			StripSignature(input, retcode);
		}
	}

	std::shared_ptr<BYTE> ProcessNode(HANDLE handle, ResourceEntryPtr node, const po::variables_map& variables)
	{
		VS_VersionInfo info(node->Address(), node->Size());

		if (variables.count("set-version"))
		{
			VersionString version = variables["set-version"].as<std::wstring>();
			info.SetField(StringTable::FileVersion, version);
			info.SetField(StringTable::ProductVersion, version);
		}
		if (variables.count("set-file-version"))
		{
			VersionString fileVersion = variables["set-file-version"].as<std::wstring>();
			info.SetField(StringTable::FileVersion, fileVersion);
		}
		if (variables.count("set-product-version"))
		{
			VersionString productVersion = variables["set-product-version"].as<std::wstring>();
			info.SetField(StringTable::ProductVersion, productVersion);
		}
		if (variables.count("set-file-description"))
		{
			info.SetField(StringTable::FileDescription, variables["set-file-description"].as<std::wstring>());
		}
		if (variables.count("set-internal-name"))
		{
			info.SetField(StringTable::InternalName, variables["set-internal-name"].as<std::wstring>());
		}
		if (variables.count("set-copyright"))
		{
			info.SetField(StringTable::LegalCopyright, variables["set-copyright"].as<std::wstring>());
		}
		if (variables.count("set-original-name"))
		{
			info.SetField(StringTable::OriginalFilename, variables["set-original-name"].as<std::wstring>());
		}
		if (variables.count("set-product-name"))
		{
			info.SetField(StringTable::ProductName, variables["set-product-name"].as<std::wstring>());
		}

		std::shared_ptr<BYTE> data = info.NewData();

		if (!UpdateResource(handle, MAKEINTRESOURCE(16), MAKEINTRESOURCE(1), (WORD)std::stoi(node->Name()), data.get(), (WORD)info.NewSize()))
		{
			std::wcerr << L"Failed to update resource. Error: " << GetLastError() << L" Path: 16/1/" << node->Name() << std::endl;
			return std::shared_ptr<BYTE>();
		}

		return data;
	}

	void Edit(const po::variables_map& variables, int& retcode)
	{
		retcode = 1;
		if (!variables.count("input"))
		{
			std::wcerr << L"Error parsing options: must have some input files.\n";
			return;
		}

		auto inputs = variables["input"].as<std::vector<std::wstring>>();
		bool noResourceRebuild = variables["no-resource-rebuild"].as<bool>();

		retcode = 0;
		for (auto& input : inputs)
		{
			StripSignature(input, retcode);

			if (!noResourceRebuild)
			{
				HANDLE binaryHandle = NULL;
				{
					PEParser pe(input);
					pe.Open();
					if (!pe.IsValidPE())
					{
						std::wcerr << L"Can't open file for reading or invalid format." << std::endl;
						continue;
					}

					std::vector<std::shared_ptr<BYTE>> data;

					binaryHandle = BeginUpdateResource(input.c_str(), FALSE);
					if (!binaryHandle)
					{
						std::wcerr << L"Can't lock file for updating resources." << std::endl;
						continue;
					}

					ResourceEntryPtr node;
					if (pe.ResourceDirectory())
						node = pe.ResourceDirectory()->AtPath(L"16/1");

					if (!node) 
						continue;

					bool success = true;
					if (node->IsData())
						data.push_back(ProcessNode(binaryHandle, node, variables));
					else
						for (auto& entry : node->Entries())
							data.push_back(ProcessNode(binaryHandle, entry.second, variables));
				}

				if (!EndUpdateResource(binaryHandle, false))
				{
					std::wcerr << L"Failed to update resources" << std::endl;
					retcode = 1;
				}
			}
			else
			{
				PEParser pe(input);
				pe.Open(true);
				if (!pe.IsValidPE())
				{
					std::wcerr << L"Can't open file for reading or invalid format." << std::endl;
					continue;
				}

				VersionString version;
				PEParser::VersionField versionField = PEParser::Both;

				if (variables.count("set-version"))
				{
					version = variables["set-version"].as<std::wstring>();
					versionField = PEParser::Both;
				}
				else if (variables.count("set-file-version"))
				{
					version = variables["set-file-version"].as<std::wstring>();
					versionField = PEParser::FileOnly;
				}
				else if (variables.count("set-product-version"))
				{
					version = variables["set-product-version"].as<std::wstring>();
					versionField = PEParser::ProductOnly;
				}

				if (!pe.SetVersion(version, versionField))
					retcode = 1;

				pe.Close();
			}
		}
	}

	void Sign(const boost::program_options::variables_map& variables, int& retcode)
	{
		retcode = 1;

		std::string tokenPassword = variables["etoken-password"].as<std::string>();
		std::vector<std::wstring> inputs = variables["input"].as<std::vector<std::wstring>>();
		std::wstring certStore = variables["cert-store"].as<std::wstring>();
		std::string certHash = variables["cert-hash"].as<std::string>();
		std::vector<std::wstring> timestampUrls;
		if (variables.count("timestamp"))
			timestampUrls = variables["timestamp"].as<std::vector<std::wstring>>();

		// Certificate/Details/Thumbprint, copy-through via plain-text editor to filter out non-alphanumeric fluff if any, leave spaces alone.
		// For example: "01 32 45 67 78 90 ab cd ef 01 32 45 67 78 90 ab cd ef 01 32"
		if (certHash.size() != 59)
		{
			std::cerr << " Certificate hash seems to be malformed. The only supported format looks like this: \'01 32 45 67 78 90 ab cd ef 01 32 45 67 78 90 ab cd ef 01 32\'" << std::endl;
			return;
		}

		std::string decodedHash;
		std::stringstream ss(certHash);
		ss >> std::hex;
		int c = 0;
		while (ss >> c) decodedHash.push_back((unsigned char)c);

		SafeNetTokenLogin login(tokenPassword);

		Signer signer;

		if (!signer.SelectCertificate(certStore, decodedHash))
			return;

		bool ret = true;
		for (auto it = inputs.begin(); it != inputs.end(); ++it)
			ret = signer.SignFile(*it, timestampUrls) && ret;

		if (ret)
			retcode = 0;
	}

	void CheckDependencies(const po::variables_map& variables, int& retcode)
	{
		namespace fs = boost::filesystem;

		retcode = 1;

		auto inputs = variables["input"].as<std::vector<std::wstring>>();
		bool verbose = variables["verbose"].as<bool>();
		bool json = variables["json"].as<bool>();
		bool batchDlls = variables["batch-dlls"].as<bool>();
		auto peExtensions = variables["pe-extensions"].as<std::wstring>();
		bool useSystemPath = variables["use-system-path"].as<bool>();

		if (inputs.size() == 0)
		{
			std::wcerr << L"Need input.";
			return;
		}

		auto out = OpenOutput<char>(variables);
		if (!out) 
			return;

		if (useSystemPath)
			LoadSystemPath();

		retcode = 0;

		PrintedSet pathCache;

		if (!batchDlls)
		{
			PEBinaryMap cache;
			auto pe = CollectDependencies(inputs[0], cache);

			if (pe && !pe->resolved)
				retcode = 2;

			if (json)
				*out << PrintDependencyTreeJson(pe, cache, !verbose);
			else
				PrintDependencyTree(*out, pe, pathCache, !verbose);
		}
		else
		{
			std::vector<std::wstring> extensions;
			if (!peExtensions.empty())
				boost::split(extensions, boost::algorithm::to_lower_copy(peExtensions), boost::is_any_of(L";"));
			auto isNonExecutablePE = [&extensions](const boost::filesystem::path& path)
			{
				auto ext = path.extension().wstring();
				boost::algorithm::to_lower(ext);
				boost::algorithm::trim_left_if(ext, boost::is_any_of(L"."));
				if (ext == L"exe")
					return false;

				return extensions.size() == 0 || std::find(extensions.begin(), extensions.end(), ext) != extensions.end();
			};

			// all PE files that are not executables
			std::list<fs::path> files;
			for (auto& input : inputs)
				files.splice(files.end(), ListFiles(input, isNonExecutablePE));

			// process them all over one cache
			PEBinaryMap peCache;
			std::list<PEBinaryPtr> peBinaries;
			for (auto& file : files)
				peBinaries.push_back(CollectDependencies(file, peCache));

			if (json)
				*out << PrintDependencyTreeJson(PEBinaryPtr(), peCache, !verbose);

			for (auto& pe : peBinaries)
			{
				if (!pe)
					continue;

				if (!pe->resolved)
					retcode = 2;

				if (!json)
					PrintDependencyTree(*out, pe, pathCache, !verbose);
			}
		}
	}
}