/*######   Copyright (c) 2013-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "../util/prime-util.h"
#include "../eng.h"

namespace Coin {

double GetFractionalDifficulty(double length) {
	return 1/(1-(length - floor(length)));
}

double DifficultyToFractional(double diff) {
	return 1 - 1/diff;
}


class PrimeCoinBlockObj : public BlockObj {
	typedef BlockObj base;
public:
	BigInteger PrimeChainMultiplier;

	double GetTargetLength() const {
		return double(DifficultyTargetBits)/0x1000000;
	}
protected:
	void WriteHeader(BinaryWriter& wr) const override {
		base::WriteHeader(wr);
		CoinSerialized::WriteBlob(wr, PrimeChainMultiplier.ToBytes());
	}

	void ReadHeader(const BinaryReader& rd, bool bParent, const HashValue *pMerkleRoot) override {
		base::ReadHeader(rd, bParent, pMerkleRoot);
		Blob blob = CoinSerialized::ReadBlob(rd);
		PrimeChainMultiplier = BigInteger(blob.constData(), blob.Size);

		TRC(6, "PrimeChainMultiplier: " << PrimeChainMultiplier);
	}

	using base::Write;
	using base::Read;

	void Write(DbWriter& wr) const override {
		base::Write(wr);
		wr << PrimeChainMultiplier;
	}

	void Read(const DbReader& rd) override {
		base::Read(rd);
		rd >> PrimeChainMultiplier;
	}

	BigInteger GetWork() const override {
		return 1;
	}

	Coin::HashValue Hash() const override {
		if (!m_hash) {
			MemoryStream ms;
			WriteHeader(BinaryWriter(ms).Ref());
			m_hash = Coin::Hash(ms);
		}
		return m_hash.get();
	}

	HashValue PowHash() const override {
		return Coin::Hash(EXT_BIN(Ver << PrevBlockHash << MerkleRoot() << (uint32_t)to_time_t(Timestamp) << get_DifficultyTarget() << Nonce));
	}

	void CheckPow(const Target& target) override {
		HashValue hashPow = PowHash();
		BigInteger bnHash = HashValueToBigInteger(hashPow);
		BigInteger origin = bnHash * PrimeChainMultiplier;

		if (hashPow.ToConstBuf().P[31]<0x80 || origin>GetPrimeMax())
			Throw(CoinErr::ProofOfWorkFailed);

		double targ = GetTargetLength();
		PrimeTester tester;
		CunninghamLengths cl = tester.ProbablePrimeChainTest(Bn(origin));
		if (!cl.TargetAchieved(targ)) {
#ifdef X_DEBUG//!!!D
			tester.ProbablePrimeChainTest(Bn(origin));
#endif
			Throw(CoinErr::ProofOfWorkFailed);
		}
		pair<PrimeChainType, double> tl = cl.BestTypeLength();
		if (!PrimeChainMultiplier.TestBit(0) && !origin.TestBit(1) && tester.ProbablePrimeChainTest(Bn(origin/2)).BestTypeLength().second > tl.second) {
#ifdef X_DEBUG//!!!D
			double le = ProbablePrimeChainTest(origin/2, targ).BestTypeLength().second;
#endif
			Throw(CoinErr::ProofOfWorkFailed);
		}
	}
};

class PrimeCoinEng : public CoinEng {
	typedef CoinEng base;
public:
	PrimeCoinEng(CoinDb& cdb)
		:	base(cdb)
	{
		MaxBlockVersion = 5;
	}

	void SetChainParams(const Coin::ChainParams& p) override {
		base::SetChainParams(p);
		ChainParams.MedianTimeSpan = 99;
	}
protected:
	BlockObj *CreateBlockObj() override { return new PrimeCoinBlockObj; }

	double ToDifficulty(const Target& target) override {
		return double(target.m_value)/(1 << FRACTIONAL_BITS);
	}

	int64_t GetSubsidy(int height, const HashValue& prevBlockHash, double difficulty, bool bForCheck) override {
		int64_t cents = (int64_t)floor(999 * 100 / (difficulty*difficulty));
		return cents * (ChainParams.CoinValue/100);
	}

	Target GetNextTargetRequired(const BlockHeader& headerLast, const Block& block) override {
		double r = 7;
		if (headerLast.Height > 1) {
			const int INTERVAL = 7*24*60;
			seconds nActualSpacing = duration_cast<seconds>(headerLast.get_Timestamp() - headerLast.GetPrevHeader().get_Timestamp());
			double targ = ((PrimeCoinBlockObj*)headerLast.m_pimpl.get())->GetTargetLength();
			double d = GetFractionalDifficulty(targ);
			const seconds nTargetSpacing = ChainParams.BlockSpan;
			
			BigInteger bn = BigInteger((int64_t)floor(d * (1ULL << 32))) * (INTERVAL+1) * nTargetSpacing.count() / ((INTERVAL-1) * nTargetSpacing.count() + 2 * nActualSpacing.count());
			d = double(explicit_cast<int64_t>(bn)) / (1ULL << 32); 

			d = max(1., min(double(1 << FRACTIONAL_BITS), d));
			r = targ;
			if (d > 256)
				r = floor(targ)+1;
			else if (d == 1 && floor(targ) > 6)
				r = floor(targ) - 1 + DifficultyToFractional(256);
			else
				r = floor(targ) + DifficultyToFractional(d);
		}
		return Target((uint32_t)ceil(r * 0x1000000));
	}

	Target GetNextTarget(const BlockHeader& headerLast, const Block& block) override {
		return GetNextTargetRequired(headerLast, block);
	}
};

static CurrencyFactory<PrimeCoinEng> s_primecoin("Primecoin");

} // Coin::

