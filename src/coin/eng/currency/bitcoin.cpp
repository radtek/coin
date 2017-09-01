/*######   Copyright (c) 2011-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../eng.h"
#include "../script.h"
#include "../coin-model.h"

namespace Coin {

class COIN_CLASS BitcoinEng : public CoinEng {
	typedef CoinEng base;
public:
	BitcoinEng(CoinDb& cdb)
		:	base(cdb)
	{}
protected:
	void CheckForDust(const Tx& tx) override {
		EXT_FOR(const TxOut& txOut, tx.TxOuts()) {
			txOut.CheckForDust();
		}
	}

	void UpdateMinFeeForTxOuts(int64_t& minFee, const int64_t& baseFee, const Tx& tx) override {
	}
};


static CurrencyFactory<BitcoinEng> s_bitcoin("Bitcoin");
static CurrencyFactory<BitcoinEng> s_bitcoinTestnet("Bitcoin-testnet");

} // Coin::

