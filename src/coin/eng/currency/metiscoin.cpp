/*######   Copyright (c) 2013-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../eng.h"

namespace Coin {

class MetisBlockObj : public BlockObj {
	typedef BlockObj base;
public:
	MetisBlockObj() {
		Ver = 112;
	}
protected:
};

class MetisTxObj : public TxObj {
	typedef TxObj base;
public:
	String Comment;

protected:
	MetisTxObj *Clone() const override { return new MetisTxObj(_self); }
	String GetComment() const override { return Comment; }

	void WriteSuffix(BinaryWriter& wr) const {
		if (Ver >= 2)
			WriteString(wr, Comment);
	}

	void ReadSuffix(const BinaryReader& rd) {
		if (Ver >= 2)
			Comment = ReadString(rd);
	}
};

class COIN_CLASS MetisCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	MetisCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		MaxBlockVersion = 112;
	}
protected:
	BlockObj *CreateBlockObj() override { return new MetisBlockObj; }
	TxObj *CreateTxObj() override { return new MetisTxObj; }

	void SetChainParams(const Coin::ChainParams& p) override {
		base::SetChainParams(p);
		ChainParams.MaxPossibleTarget = Target(0x1E00FFFF);
	}
};

static CurrencyFactory<MetisCoinEng> s_metiscoin("MetisCoin");

} // Coin::


