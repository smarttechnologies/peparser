// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "actions.h"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>

#include <iostream>

namespace po = boost::program_options;
using namespace peparser;

int wmain(int argc, wchar_t* argv[])
{
	std::locale::global(std::locale("C"));

	int retcode = -1;

	try 
	{
		po::variables_map variables;
		std::vector<po::options_description> options;

		options.push_back(po::options_description("Common"));
		options.back().add_options()
			("help", "This message.")
			("silent", po::value<bool>()->zero_tokens()->default_value(false), "Suppress standard output.")
			("verbose", po::value<bool>()->zero_tokens()->default_value(false), "Print dynamically ignored ranges and other info.")
			("input", po::wvalue<std::vector<std::wstring>>()->composing(), "Input files.")
			("output", po::wvalue<std::wstring>(), "Output file path, if omitted uses standard out.")
		;

		options.push_back(po::options_description("Info"));
		options.back().add_options()
			("info", po::value<bool>()->zero_tokens()->notifier(std::bind(&Info, std::ref(variables), std::ref(retcode))), "Print full file information. Returns 0 if all files are valid PE binaries.")
			("pdb", po::value<bool>()->zero_tokens()->notifier(std::bind(&Pdb, std::ref(variables), std::ref(retcode))), "Print pdb path and guid. Returns 0 if all files have debug information.")
			("imports", po::value<bool>()->zero_tokens()->notifier(std::bind(&Imports, std::ref(variables), std::ref(retcode))), "Print a list of imported dlls.")
			("signature", po::value<bool>()->zero_tokens()->notifier(std::bind(&Signature, std::ref(variables), std::ref(retcode))), "Check if binary has a digital signature section (does not validate signature). Returns 0 if all files have a DS section.")
			("version-info", po::value<bool>()->zero_tokens()->notifier(std::bind(&Version, std::ref(variables), std::ref(retcode))), "Print version.")
			("dump-section", po::wvalue<std::wstring>()->notifier(std::bind(&DumpSection, std::ref(variables), std::ref(retcode))), "Dump contents of a named PE section. Takes a single input file.")
			("dump-resource", po::wvalue<std::wstring>()->notifier(std::bind(&DumpResource, std::ref(variables), std::ref(retcode))), "Extract a resource by path. See contents of .rsrc section in output of --info for available entries.")
		;

		options.push_back(po::options_description("Compare"));
		options.back().add_options()
			("compare"
				, po::value<bool>()->zero_tokens()->notifier(std::bind(&Compare, std::ref(variables), std::ref(retcode)))
				, "Compare 2 files disregarding linker timestamp, "
				"debug info, digital signature, version info section in resources, "
				//"MIDL vanity stub for embedded type libraries, "
				"__FILE__, __DATE__ and __TIME__ macros when they are used as literal strings.\n"
				"  Turn off 'link time code generation' option when building binaries to compare and keep full build path length stable between builds. "
				"If done right, rebuilds with the same source will be flagged as 'functionally equivalent'. \n"
				"  Returns 0 if files are functionally equivalent.\n"
			)
			("r", po::wvalue<std::wstring>()->default_value(L"{}", ""), "List of ranges to ignore when comparing:\n{comment1:offset1:size1,comment2:offset2:size2,...}.")
			("r1", po::wvalue<std::wstring>()->default_value(L"{}", ""), "List of ranges to ignore when comparing (first binary).")
			("r2", po::wvalue<std::wstring>()->default_value(L"{}", ""), "List of ranges to ignore when comparing (second binary).")
			("fast", po::value<bool>()->zero_tokens()->default_value(false), "Use faster comparison. Only static diffs are ignored, no difference percentage.")
			("identical", po::value<bool>()->zero_tokens()->default_value(false), "Return 0 only if files are byte-for-byte identical.")
			("no-heuristics", po::value<bool>()->zero_tokens()->default_value(false), "Do not try to interpret differences at unknown offsets.")
            ("tlb-timestamp", po::value<bool>()->zero_tokens()->default_value(false), "Experimental workaround for TLB timestamp (tested on MIDL version 7.00.0555)")
		;

		options.push_back(po::options_description("Edit"));
		options.back().add_options()
			("delete-resource", po::wvalue<std::wstring>()->notifier(std::bind(&DeleteResource, std::ref(variables), std::ref(retcode))), "Delete resource by path.")
			("delete-signature", po::value<bool>()->zero_tokens()->notifier(std::bind(&DeleteSignature, std::ref(variables), std::ref(retcode))), "Delete signature.\n")
			("edit-vsversion", po::wvalue<bool>()->zero_tokens()->notifier(std::bind(&Edit, std::ref(variables), std::ref(retcode))), "Modify VS_VERSIONINFO. Binary must already contain VS_VERSIONINFO resource.")
			("set-version", po::wvalue<std::wstring>(), "Set new version (both file and product), file is modified in-place.")
			("set-file-version", po::wvalue<std::wstring>(), "Set new file version.")
			("set-product-version", po::wvalue<std::wstring>(), "Set new product version.")
			("set-file-description", po::wvalue<std::wstring>(), "Set file description field.")
			("set-internal-name", po::wvalue<std::wstring>(), "Set internal name field.")
			("set-copyright", po::wvalue<std::wstring>(), "Set copyright field.")
			("set-original-name", po::wvalue<std::wstring>(), "Set original name field.")
			("set-product-name", po::wvalue<std::wstring>(), "Set product name field.")
			("no-resource-rebuild", po::wvalue<bool>()->zero_tokens()->default_value(false), "Avoid rebuilding resources, only works with set-version, set-file-version and set-product-version and only if there is enough space in string table to fit new version string.")
		;

		options.push_back(po::options_description("Sign"));
		options.back().add_options()
			("sign", po::value<bool>()->zero_tokens()->notifier(std::bind(&Sign, std::ref(variables), std::ref(retcode))), "Sign file.\n")
			("cert-store", po::wvalue<std::wstring>()->default_value(L"MY", ""), "Certificate store. Default value is 'MY'.")
			("cert-hash", po::value<std::string>(), "Certificate thumbprint (copy from Details/Thumbprint).")
			("timestamp", po::wvalue<std::vector<std::wstring>>()->composing(), "URL to a timestamp server. Repeat for multiple URLs (to be tried if previous URL failed). For example\nhttp://timestamp.verisign.com/scripts/timstamp.dll")
			("etoken-password", po::value<std::string>()->default_value(""), "SafeNet etoken password. Set to avoid GUI password prompt if chosen certificate is on a token.")
		;

		options.push_back(po::options_description("Dependency check"));
		options.back().add_options()
			("check-dependencies"
				, po::value<bool>()->zero_tokens()->notifier(std::bind(&CheckDependencies, std::ref(variables), std::ref(retcode)))
				, "Checks dependencies of a PE binary and everything it links to. "
				  "Use --verbose for full dependency tree, otherwise prints binaries with missing dependencies only. "
				  "Returns 2 if a dependency is missing, 1 on any other error and 0 on success. "
				  "Architecture of this executable (x86/x64) must match architectures of checked binaries."
			)
			("json", po::value<bool>()->zero_tokens()->default_value(false), "Output in json.")
			("batch-dlls", po::value<bool>()->zero_tokens()->default_value(false), "Check dependency on all non executables in folders. Executables can't be batched and must be checked one by one in order to set up default activation context. The tool loads dlls in the process, so use matching architecture.")
			("pe-extensions", po::wvalue<std::wstring>()->default_value(L"", ""), "A semi-colon separated list of file extension to check when batching dlls. For example 'dll;cpl;sys'. Omit to test all files except executables.")
			("use-system-path", po::value<bool>()->zero_tokens()->default_value(false), "Load system PATH instead of using PATH from current environment.")
		;

		po::options_description cmdLine;
		for (auto& option : options)
			cmdLine.add(option);

		po::positional_options_description positional;
		positional.add("input", -1);

		po::wparsed_options parsed = po::wcommand_line_parser(argc, argv)
			.options(cmdLine)
			.positional(positional)
			.allow_unregistered()
			.run();

		std::vector<std::wstring> unrecognized = po::collect_unrecognized(parsed.options, po::exclude_positional);
		if (unrecognized.size() > 0)
		{
			std::wcerr << L"\nUnrecognized options:\n";
			for(auto& str : unrecognized) 
				std::wcerr << L"   " << str << L"\n";
			std::wcerr << std::endl;
			return 1;
		}

		po::store(parsed, variables);
		if (variables.count("help"))
		{
			for (auto& option : options)
				std::cout << option << "\n";
			return 0;
		}

		if (variables["silent"].as<bool>())
			std::wcout.rdbuf(NULL);

		po::notify(variables);

		if (retcode == -1) // nothing was done
		{
			for (auto& option : options)
				std::cout << option << "\n";
			return 1;
		}
	}
	catch (std::exception& e)
	{
		std::wcerr 
			<< L"\nError parsing options: \n" 
			<< e.what() << L"\n" << std::endl;
		return 1;
	}

	return retcode;
}