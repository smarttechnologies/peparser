// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#pragma warning(push)
#pragma warning(disable : 4996)
#include <boost/program_options/variables_map.hpp>
#pragma warning(pop)

namespace peparser
{
	void Info(const boost::program_options::variables_map& variables, int& retcode);
	void Pdb(const boost::program_options::variables_map& variables, int& retcode);
	void Version(const boost::program_options::variables_map& variables, int& retcode);
	void Imports(const boost::program_options::variables_map& variables, int& retcode);
	void Signature(const boost::program_options::variables_map& variables, int& retcode);
	void DumpSection(const boost::program_options::variables_map& variables, int& retcode);
	void DumpResource(const boost::program_options::variables_map& variables, int& retcode);

	void Compare(const boost::program_options::variables_map& variables, int& retcode);

	void DeleteResource(const boost::program_options::variables_map& variables, int& retcode);
	void DeleteSignature(const boost::program_options::variables_map& variables, int& retcode);
	void Edit(const boost::program_options::variables_map& variables, int& retcode);

	void Sign(const boost::program_options::variables_map& variables, int& retcode);

	void CheckDependencies(const boost::program_options::variables_map& variables, int& retcode);
}