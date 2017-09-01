/*######   Copyright (c) 2012-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/bignum.h>

#include <el/crypto/hash.h>
#include <el/crypto/ecdsa.h>
using namespace Ext::Crypto;

#include "coin-model.h"
#include "eng.h"
#include "coin-protocol.h"
#include "script.h"


namespace Coin {

void PeerInfoBase::Write(BinaryWriter& wr) const {
	wr << Services;
	Blob blob = Ep.Address.GetAddressBytes();
	if (blob.Size == 4) {
		const byte buf[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF };
		wr.Write(buf, sizeof buf);
	}
	wr.Write(blob.constData(), blob.Size);
	wr << uint16_t(_byteswap_ushort(Ep.Port));
}

void PeerInfoBase::Read(const BinaryReader& rd) {
	Services = rd.ReadUInt64();
	byte buf[16];
	rd.Read(buf, sizeof buf);
	uint16_t port = _byteswap_ushort(rd.ReadUInt16());
	Ep = IPEndPoint(IPAddress(!memcmp(buf, "\0\0\0\0\0\0\0\0\0\0\xFF\xFF", 12) ? ConstBuf(buf+12, 4) : ConstBuf(buf, 16)), port);
}


void PeerInfo::Write(BinaryWriter& wr) const {
	wr << (uint32_t)to_time_t(Timestamp);
	base::Write(wr);
}

void PeerInfo::Read(const BinaryReader& rd) {
	Timestamp = DateTime::from_time_t(rd.ReadUInt32());
	base::Read(rd);
}

HashValue Hash(const CPersistent& pers) {
	return Hash(EXT_BIN(pers));
}

HashValue Hash(const Tx& tx) {
#ifdef X_DEBUG//!!!D
	if (tx.m_pimpl->m_hash && tx.m_pimpl->m_hash != Hash(EXT_BIN(tx))) {
		HashValue hv = Hash(EXT_BIN(tx));
		MemoryStream ms;
		BinaryWriter wr(ms);
		wr << tx;
		hv = hv;
	}
	ASSERT(tx.m_pimpl->m_nBytesOfHash != 32 || tx.m_pimpl->m_hash == Hash(EXT_BIN(tx)));
#endif
	if (tx.m_pimpl->m_nBytesOfHash != 32)		
		tx.SetHash(Eng().HashFromTx(tx));
	return tx.m_pimpl->m_hash;
}

void MerkleTx::Write(BinaryWriter& wr) const {
	base::Write(wr);
	wr << BlockHash << MerkleBranch;
}

void MerkleTx::Read(const BinaryReader& rd) {
	base::Read(rd);
	rd >> BlockHash >> MerkleBranch;
}

void AuxPow::Write(BinaryWriter& wr) const {
	base::Write(wr);
	wr << ChainMerkleBranch;
	ParentBlock.WriteHeader(wr);
}

void AuxPow::Read(const BinaryReader& rd) {
	base::Read(rd);
	rd >> ChainMerkleBranch;
	ParentBlock.ReadHeader(rd, true);
}

void AuxPow::Write(DbWriter& wr) const {
	base::Write(wr);
	wr << ChainMerkleBranch;
#if UCFG_COIN_COMPACT_AUX
	const BlockObj& bo = *ParentBlock.m_pimpl;
	wr << bo.Ver << bo.PrevBlockHash << (uint32_t)to_time_t(bo.Timestamp) << bo.DifficultyTargetBits << bo.Nonce;
#else
	ParentBlock.WriteHeader(wr);
#endif
}

void AuxPow::Read(const DbReader& rd) {
	base::Read(rd);
	rd >> ChainMerkleBranch;
#if UCFG_COIN_COMPACT_AUX
	const HashValue merkleRoot = MerkleBranch.Apply(Hash(Tx(_self)));
	ParentBlock.ReadHeader(rd, true, &merkleRoot);
#else
	ParentBlock.ReadHeader(rd, true);
#endif
}

int CalcMerkleIndex(int merkleSize, int chainId, uint32_t nonce) {
    return (((nonce * 1103515245 + 12345) + chainId) * 1103515245 + 12345) % merkleSize;
}

const byte s_mergedMiningHeader[4] = { 0xFA, 0xBE, 'm', 'm' } ;

void AuxPow::Check(const Block& blockAux) {
	if (blockAux.ChainId == ParentBlock.ChainId)
		Throw(CoinErr::AUXPOW_ParentHashOurChainId);
	if (ChainMerkleBranch.Vec.size() > 30)
		Throw(CoinErr::AUXPOW_ChainMerkleBranchTooLong);

	HashValue rootHash = ChainMerkleBranch.Apply(Hash(blockAux));
	std::reverse(rootHash.data(), rootHash.data()+32);						// correct endian

	if (MerkleBranch.Apply(Hash(Tx(_self))) != ParentBlock.get_MerkleRoot())
		Throw(CoinErr::AUXPOW_MerkleRootIncorrect);

	Blob script = TxIns().at(0).Script();
	const byte *pcHead = Search(ConstBuf(script), ConstBuf(s_mergedMiningHeader, sizeof s_mergedMiningHeader)),
		       *pc = Search(ConstBuf(script), rootHash.ToConstBuf());
	if (pc == script.end())
		Throw(CoinErr::AUXPOW_MissingMerkleRootInParentCoinbase);
	if (pcHead != script.end()) {
		if (script.end() != std::search(pcHead+1, script.end(), begin(s_mergedMiningHeader), end(s_mergedMiningHeader)))
			Throw(CoinErr::AUXPOW_MutipleMergedMiningHeades);
		if (pc != pcHead + sizeof(s_mergedMiningHeader))
			Throw(CoinErr::AUXPOW_MergedMinignHeaderiNotJustBeforeMerkleRoot);		
	} else if (pc-script.begin() > 20)
		Throw(CoinErr::AUXPOW_MerkleRootMustStartInFirstFewBytes);
	CMemReadStream stm(ConstBuf(pc + 32, script.end()-pc-32));
	int32_t size, nonce;
	BinaryReader(stm) >> size >> nonce;
	if (size != (1 << ChainMerkleBranch.Vec.size()))
		Throw(CoinErr::AUXPOW_MerkleBranchSizeIncorrect);
	if (ChainMerkleBranch.Index != CalcMerkleIndex(size, blockAux.ChainId, nonce))
		Throw(CoinErr::AUXPOW_WrongIndex);
}


PrivateKey::PrivateKey(RCString s) {
	try {
		Blob blob = ConvertFromBase58(s.Trim());
		if (blob.Size < 10)
			Throw(CoinErr::InvalidAddress);
		byte ver = blob.constData()[0];
//!!! common ver for all Nets		if (ver != Eng().ChainParams.AddressVersion+128)
		if (!(ver & 128))
			Throw(CoinErr::InvalidAddress);
		m_blob = Blob(blob.constData()+1, blob.Size-1);
	} catch (RCExc) {
		Throw(CoinErr::InvalidAddress);
	}
}

PrivateKey::PrivateKey(const ConstBuf& cbuf, bool bCompressed) {
	m_blob = cbuf;
	if (bCompressed) {
		m_blob.Size = m_blob.Size+1;
		m_blob.data()[m_blob.Size-1] = 1;
	}
}

String PrivateKey::ToString() const {
	byte ver = 128;		//!!!  for all nets, because Private Keys are common // Eng().ChainParams.AddressVersion+128;
	Blob blob = ConstBuf(&ver, 1)+m_blob;
	return ConvertToBase58(blob);
}

ChainCaches::ChainCaches()
	:	m_bestBlock(nullptr)
	,	HashToBlockCache(64)
	,	HeightToHashCache(1024)
	,	HashToTxCache(1024)					// Number of Txes in the block usually >256
	,	m_cachePkIdToPubKey(4096)			// should be more than usual value (Txes per Block)
	,	PubkeyCacheEnabled(true)
	,	OrphanBlocks(BLOCK_DOWNLOAD_WINDOW)	
{
}

} // Coin::

