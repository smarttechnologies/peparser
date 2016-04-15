// Copyright (c) 2016 SMART Technologies. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <istream>
#include <memory>

namespace peparser
{
	// describes a range of bytes in a file
	class Block
	{
	public:
		// file offset for start of the block in bytes
		size_t offset = 0;
		// block size, bytes
		size_t size = 0;
		std::wstring description;
		std::wstring data;

		Block() {}
		Block(const std::wstring& description, size_t offset, size_t size) : description(description), offset(offset), size(size) {}

		// true if b starts before this starts
		bool operator <(const Block& b) const { return offset < b.offset; }

		// true if block is completely inside this
		bool Contains(const Block& block);

		void Print(std::wostream& out, const std::wstring& prefix) const;
	};

	// describes a (same) range of bytes in 2 files
	// might have different offsets depending on preceding ignored ranges 
	class Block2 : public Block
	{
	public:
		size_t offset2 = 0;
		std::wstring data2;

		Block2() { }
		Block2(const std::wstring& description, size_t offset1, size_t offset2, size_t size)
			: Block(description, offset1, size)
			, offset2(offset2)
		{}

		bool operator ==(const Block2& block) const
		{
			return offset == block.offset && offset2 == block.offset2 && size == block.size;
		}

		void Print(std::wostream& out, const std::wstring& prefix) const;
	};

	typedef std::vector<Block> BlockList;
	typedef std::vector<Block2> Block2List;

	typedef std::shared_ptr<class BlockNode> BlockNodePtr;

	// tree of nested blocks
	// children are always fully contained in the parent node
	class BlockNode
	{
	public:
		explicit BlockNode(const Block2& block);

		bool Add(const Block& block);
		bool Add(const Block2& block);
		void Add(const BlockList& blockList);
		void Add(const Block2List& blockList);

		void Sort();

		void Print(std::wostream& out, const std::wstring& prefix) const;

		bool operator <(const BlockNode& b) const { return m_self < b.m_self; }

	private:
		typedef std::list<BlockNodePtr> List;

		Block2 m_self;
		List m_children;

		void ConsumeOffspring(List& kids);
	};

	std::wistream& operator>>(std::wistream& in, Block& block);
	std::wistream& operator>>(std::wistream& in, BlockList& blockList);

	std::wostream& operator<<(std::wostream& out, const Block& block);
	std::wostream& operator<<(std::wostream& out, const BlockList& blockList);

	std::wostream& operator<<(std::wostream& out, const Block2& block);
	std::wostream& operator<<(std::wostream& out, const Block2List& blockList);
}
