// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "block.h"

#include <boost/io/ios_state.hpp>

#include <sstream>
#include <iomanip>
#include <algorithm>

namespace peparser
{
	std::wistream& operator>>(std::wistream& in, BlockList& blockList)
	{
		boost::io::ios_flags_saver ifs(in);

		in >> std::skipws >> std::setbase(16);

		wchar_t bracket = '\0';
		std::wstringbuf buffer(L"");

		in.get(bracket);
		if (in.fail() || bracket != L'{')
			throw std::exception("error parsing block list");

		if (in.peek() == L'}')
		{
			in.get();
			return in;
		}

		while (true)
		{
			in >> std::ws;

			Block block;

			buffer.str(L"");
			in.get(buffer, L':');
			block.description = buffer.str();

			in.get(bracket);
			if (in.fail() || bracket != L':')
				throw std::exception("error parsing block list");

			in >> block.offset >> std::ws;

			in.get(bracket);
			if (in.fail() || bracket != L':')
				throw std::exception("error parsing block list");

			in >> block.size >> std::ws;

			if (in.fail())
				throw std::exception("error parsing block list");

			blockList.push_back(block);

			in.get(bracket);
			if (bracket == L'|')
				continue;
			if (bracket == L'}')
				break;

			throw std::exception("error parsing block list");
		}

		return in;
	}

	std::wostream& operator<<(std::wostream& out, const Block& block)
	{
		block.Print(out, L"");

		return out;
	}

	std::wostream& operator<<(std::wostream& out, const Block2& block)
	{
		block.Print(out, L"");

		return out;
	}

	std::wostream& operator<<(std::wostream& out, const BlockList& blockList)
	{
		for (size_t i = 0; i < blockList.size(); i++)
			out << L'\t' << blockList[i] << L'\n';
		return out;
	}

	std::wostream& operator<<(std::wostream& out, const Block2List& blockList)
	{
		for (size_t i = 0; i < blockList.size(); i++)
			out << L'\t' << blockList[i] << L'\n';
		return out;
	}

	// =========================================================================================

	bool Block::Contains(const Block& block)
	{
		if (size == 0) 
			return false;
		if (offset > block.offset) 
			return false;
		if (offset + size < block.offset) 
			return false;
		if (offset + size < block.offset + block.size) 
			return false;
		return true;
	}

	void Block::Print(std::wostream& out, const std::wstring& prefix) const
	{
		out
			<< L"offset: " << std::left << std::setw(10) << offset
			<< L" size: " << std::left << std::setw(8) << size
			<< L' ' << prefix << description
			;
		if (!data.empty())
			out << L" data: [" << data << "]";
	}

	void Block2::Print(std::wostream& out, const std::wstring& prefix) const
	{
		out
			<< L"offset: " << std::left << offset << L" | " << std::setw(10) << offset2
			<< L" size: " << std::left << std::setw(8) << size
			<< L' ' << prefix << description
			;
		if (!data.empty())
			out << L" data: [" << data << "]";
	}

	// =========================================================================================

	BlockNode::BlockNode(const Block2& block)
	{
		m_self = block;
	}

	bool BlockNode::Add(const Block2& block)
	{
		if (!m_self.Contains(block)) 
			return false;
		if (m_self == block) 
			return true;

		for (auto& entry : m_children)
			if (entry && entry->Add(block)) 
				return true;

		BlockNodePtr newChild(new BlockNode(block));

		// pushing tree levels down 
		newChild->ConsumeOffspring(m_children);

		m_children.push_back(newChild);

		return true;
	}

	bool BlockNode::Add(const Block& block)
	{
		return Add(Block2(block.description, block.offset, 0, block.size));
	}

	void BlockNode::Add(const Block2List& blockList)
	{
		for (auto& block : blockList)
			Add(block);
	}

	void BlockNode::Add(const BlockList& blockList)
	{
		for (auto& block : blockList)
			Add(block);
	}

	void BlockNode::ConsumeOffspring(List& kids)
	{
		List::iterator next = kids.begin();
		while (next != kids.end())
		{
			List::iterator current = next++;

			if (!m_self.Contains((*current)->m_self)) 
				continue;

			m_children.push_back(*current);
			kids.erase(current);
		}
	}

	void BlockNode::Print(std::wostream& out, const std::wstring& prefix) const
	{
		((Block)m_self).Print(out, prefix);

		out << L'\n';

		for (auto& entry : m_children)
			entry->Print(out, prefix + L"\t");
	}

	bool operator<(const BlockNodePtr& b1, const BlockNodePtr& b2)
	{
		if (!b1 && !b2) return false;
		if (!b1 && b2) return true;
		if (b1 && !b2) return false;

		return *b1 < *b2;
	}

	void BlockNode::Sort()
	{
		m_children.sort();
		for (auto& entry : m_children)
			entry->Sort();
	}
}