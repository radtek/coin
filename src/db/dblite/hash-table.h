/*######   Copyright (c) 2014-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include "filet.h"
#include "b-tree.h"

namespace Ext { namespace DB { namespace KV {

class HtCursor;

class HashTable : public PagedMap {
	typedef PagedMap base;
public:
	Filet PageMap;
	HashType HtType;
	const int MaxLevel;

	HashTable(DbTransactionBase& tx);
	TableType Type() override { return TableType::HashTable; }
	uint32_t Hash(const ConstBuf& key) const;
	int BitsOfHash() const;
	Page TouchBucket(uint32_t nPage);
	uint32_t GetPgno(uint32_t nPage) const;
	void Split(uint32_t nPage, int level);
private:
	void Init(const TableData& td) override {
		base::Init(td);
		HtType = (HashType)td.HtType;
		if (uint32_t pgno = letoh(td.RootPgNo))
			PageMap.SetRoot(Tx.OpenPage(pgno));
		PageMap.SetLength(letoh(td.PageMapLength));
	}

	TableData GetTableData() override;
};

class BTreeSubCursor : public BTreeCursor {
	typedef BTreeCursor base;
public:
	BTree m_btree;

	BTreeSubCursor(HtCursor& cHT);

	~BTreeSubCursor() {
		SetMap(nullptr);
	}
};

class HtCursor : public CursorObj {
	typedef CursorObj base;
	typedef HtCursor class_type;
public:
	observer_ptr<HashTable> Ht;

	HtCursor() {}

	HtCursor(DbTransaction& tx, DbTable& table);

	void SetMap(PagedMap *pMap) override {
		base::SetMap(pMap);
		Ht.reset(dynamic_cast<HashTable*>(pMap));
	}

	PagePos& Top() override { return SubCursor ? SubCursor->Top() : m_pagePos; }

	void Touch() override;
	bool SeekToFirst() override;
	bool SeekToLast() override;
	bool SeekToSibling(bool bToRight) override;
	bool SeekToNext() override;
	bool SeekToPrev() override;
	bool SeekToKey(const ConstBuf& k) override;	
	bool Get(const ConstBuf& k) { return SeekToKey(k); }
	void Put(ConstBuf k, const ConstBuf& d, bool bInsert = false) override;
	void Update(const ConstBuf& d) override;
	void Delete() override;


	void Drop() override;
#ifndef _DEBUG//!!!D
private:
#endif
	ptr<BTreeSubCursor> SubCursor;
	PagePos m_pagePos;

	bool SeekToKeyHash(const ConstBuf& k, uint32_t hash);				// returns bFound
	void UpdateFromSubCursor();

	HtCursor *Clone() override { return new HtCursor(*this); }

	friend class BTreeSubCursor;

	bool UpdateImpl(const ConstBuf& k, const ConstBuf& d, bool bInsert);
};


}}} // Ext::DB::KV::
