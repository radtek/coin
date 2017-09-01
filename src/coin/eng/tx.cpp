/*######   Copyright (c) 2011-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

using namespace std::placeholders;

#include <el/crypto/hash.h>
#include <el/crypto/ext-openssl.h>

#include <el/num/mod.h>
using namespace Ext::Num;

#define OPENSSL_NO_SCTP
#include <openssl/err.h>
#include <openssl/ec.h>

#include "coin-protocol.h"
#include "script.h"
#include "crypter.h"
#include "eng.h"
#include "coin-model.h"

namespace Coin {

const Blob& TxIn::Script() const {
	if (!m_script) {
		CoinEng& eng = Eng();
		Tx tx = Tx::FromDb(PrevOutPoint.TxHash);
		const TxOut& txOut = tx.TxOuts()[PrevOutPoint.Index];
		Blob pk = RecoverPubKeyType & 0x40
			? Sec256Dsa::RecoverPubKey(SignatureHash(txOut.get_PkScript(), *m_pTxo, N, 1), ConstBuf(m_sig.constData()+1, m_sig.Size-1), RecoverPubKeyType&3, RecoverPubKeyType&4)
			: eng.GetPkById(txOut.m_idPk);
		byte lenPk = byte(pk.Size);
		m_script.AssignIfNull(m_sig + ConstBuf(&lenPk, 1) + pk);
	}
	return m_script;
}

void TxIn::Write(BinaryWriter& wr) const {
	wr << PrevOutPoint;
	CoinSerialized::WriteBlob(wr, Script());
	wr << Sequence;
}

void TxIn::Read(const BinaryReader& rd) {
	rd >> PrevOutPoint;
	m_script = CoinSerialized::ReadBlob(rd);
	rd >> Sequence;
}

void TxOut::Write(BinaryWriter& wr) const {
	CoinSerialized::WriteBlob(wr << Value, get_PkScript());
}

void TxOut::Read(const BinaryReader& rd) {
	m_pkScript = CoinSerialized::ReadBlob(rd >> Value);
}

const TxOut& CTxMap::GetOutputFor(const OutPoint& prev) const {
	return at(prev.TxHash).TxOuts().at(prev.Index);
}

bool CoinsView::HasInput(const OutPoint& op) const {
	bool bHas;
	if (Lookup(m_outPoints, op, bHas))
		return bHas;
	CoinEng& eng = Eng();
	if (!eng.Db->FindTx(op.TxHash, 0))											//!!! Double find. Should be optimized. But we need this check during Reorganize()
		Throw(CoinErr::TxNotFound);
	vector<bool> vSpend = eng.Db->GetCoinsByTxHash(op.TxHash);
	return m_outPoints[op] = op.Index < vSpend.size() && vSpend[op.Index];
}

void CoinsView::SpendInput(const OutPoint& op) {
	m_outPoints[op] = false;
}


//!!! static regex s_rePkScriptDestination("(\\x76\\xA9\\x14(.{20,20})\\x88\\xAC|(\\x21|\\x41)(.+)\\xAC)");		// binary, VC don't allows regex for binary data

byte TryParseDestination(const ConstBuf& pkScript, HashValue160& hash160, Blob& pubkey) {
	const byte *p = pkScript.P;
	if (pkScript.Size < 4 || p[pkScript.Size-1] != OP_CHECKSIG)
		return 0;
	if (pkScript.Size == 25) {
		if (p[0]==OP_DUP && p[1]==OP_HASH160 && p[2]==20 && p[pkScript.Size-2]==OP_EQUALVERIFY) {
			hash160 = HashValue160(ConstBuf(p+3, 20));
			return 20;
		}
	} else if (p[0]==33 || p[0]==65) {
		byte len = p[0];
		if (pkScript.Size == 1+len+1) {
			hash160 = Hash160(pubkey = Blob(p+1, len));
			return len;
		}
	}
	return 0;
}

TxObj::TxObj()
	:	Height(-1)
	,	LockBlock(0)
	,	m_nBytesOfHash(0)
	,	m_bLoadedIns(false)
{
}

void Tx::WriteTxIns(DbWriter& wr) const {
	CoinEng& eng = Eng();
	int nIn = 0;
	EXT_FOR (const TxIn& txIn, TxIns()) {
		byte typ = uint32_t(-1)==txIn.Sequence ? 0x80 : 0;
		if (txIn.PrevOutPoint.IsNull())
			wr.Write7BitEncoded(0);
		else {
			ASSERT(txIn.PrevOutPoint.Index >= 0);

			pair<int, int> pp = wr.PTxHashesOutNums->StartingTxOutIdx(txIn.PrevOutPoint.TxHash);
			if (pp.second >= 0) {
				wr.Write7BitEncoded(1);			// current block
			} else {
				pp = eng.Db->FindPrevTxCoords(wr, Height, txIn.PrevOutPoint.TxHash);
			}
			wr.Write7BitEncoded(pp.second + txIn.PrevOutPoint.Index);

			ConstBuf pk = FindStandardHashAllSigPubKey(txIn.Script());
			if (txIn.RecoverPubKeyType) {
				typ |= txIn.RecoverPubKeyType;
				(wr << typ).BaseStream.WriteBuf(Sec256Signature(ConstBuf(txIn.Script().constData()+1, pk.P-txIn.Script().constData()-2)).ToCompact());
				goto LAB_TXIN_WROTE;
			}
#if 0 //!!!R
			if (pk.P && !pk.Size) {

					/*!!!
					ConstBuf sig(txIn.Script().constData()+1, pk.P-txIn.Script().constData()-2);
					Blob blobPk(pk.P+1, pk.Size-1);		//!!!O
					HashValue hashSig = SignatureHash(Tx::FromDb(txIn.PrevOutPoint.TxHash).TxOuts()[txIn.PrevOutPoint.Index].PkScript, *m_pimpl, nIn, 1);
					for (byte recid=0; recid<3; ++recid) {
						if (Sec256Dsa::RecoverPubKey(hashSig, sig, recid, pk.Size<35) == blobPk) {
							typ |= 8;
							subType = byte((pk.Size<35 ? 4 : 0) | recid);
							goto LAB_RECOVER_OK;
						}
					} 
					{
						CConnectJob::CMap::iterator it = wr.ConnectJob->Map.find(Hash160(ConstBuf(pk.P+1, pk.Size-1)));
						if (it == wr.ConnectJob->Map.end() || !it->second.PubKey)
							break;
						const TxOut& txOut = wr.ConnectJob->TxMap.GetOutputFor(txIn.PrevOutPoint);
						if (txOut.m_idPk.IsNull())
							break;
					}
					typ |= 0x40;
					*/
				ConstBuf sig(txIn.Script().constData()+5, pk.P-txIn.Script().constData()-6);
				ASSERT(sig.Size>=63 && sig.Size<71);
				int len1 = txIn.Script().constData()[4];
				typ |= 0x10;							// 0.58 bug workaround: typ should not be zero
				typ |= (len1 & 1) << 5;
				typ += byte(sig.Size-63);			
				(wr << typ).Write(sig.P, len1);
				wr.Write(sig.P+len1+2, sig.Size-len1-2);		// skip "0x02, len2" fields
				goto LAB_TXIN_WROTE;
			}
#endif
		}
		wr << byte(typ);		//  | SCRIPT_PKSCRIPT_GEN
		CoinSerialized::WriteBlob(wr, txIn.Script());
LAB_TXIN_WROTE:
		if (txIn.Sequence != uint32_t(-1))
			wr.Write7BitEncoded(uint32_t(txIn.Sequence));
		++nIn;
	}
}

void TxObj::ReadTxIns(const DbReader& rd) const {
	CoinEng& eng = Eng();
	for (int nIn=0; !rd.BaseStream.Eof(); ++nIn) {
		int64_t blockordBack = rd.Read7BitEncoded(),
			 blockHeight = -1;
		pair<int, OutPoint> pp(-1, OutPoint());
		TxIn txIn;
		if (0 == blockordBack)
			txIn.PrevOutPoint = OutPoint();
		else {
			int idx = (int)rd.Read7BitEncoded();
			if (rd.ReadTxHashes) {																	//!!!? always true
				blockHeight = Height-(blockordBack-1);
				txIn.PrevOutPoint = (pp = rd.Eng->Db->GetTxHashesOutNums((int)blockHeight).OutPointByIdx(idx)).second;
			}
		}
		byte typ = rd.ReadByte();
		switch (typ & 0x7F) {
		case 0:
			txIn.m_script = CoinSerialized::ReadBlob(rd);
#ifdef X_DEBUG//!!!D
			if (txIn.m_script.Size > 80) {
				ConstBuf pk = FindStandardHashAllSigPubKey(txIn.Script());
				if (pk.P) {
					ConstBuf sig(txIn.Script().constData()+1, pk.P-txIn.Script().constData()-2);		
					Sec256Signature sigObj(sig), sigObj2;
					sigObj2.AssignCompact(sigObj.ToCompact());
					Blob ser = sigObj2.Serialize();

					if (!(ser == sig)) {
						ser = sigObj2.Serialize();
						ser = ser;
					}

				}
			}
#endif
			break;
		case 0x7F:
			if (eng.Mode==EngMode::Lite)
				break;
//!!!?			Throw(E_FAIL);
		default:
			if (typ & 8) {
				array<byte, 64> ar;
				rd.BaseStream.ReadBuffer(ar.data(), ar.size());
				Sec256Signature sigObj;
				sigObj.AssignCompact(ar);
				Blob ser = sigObj.Serialize();
				Blob sig(0, ser.Size + 2);
				byte *data = sig.data();
				data[0] = byte(ser.Size + 1);
				memcpy(data+1, ser.constData(), ser.Size);
				data[sig.Size-1] = 1;
				if (typ & 0x40) {
					txIn.m_pTxo = this;
					txIn.N = nIn;
					txIn.RecoverPubKeyType = byte(typ & 0x4F);
					txIn.m_sig = sig;
				} else
					txIn.m_script = sig;
			} else {
				int len = (typ & 0x07)+63;
				ASSERT(len>63 || (typ & 0x10));   // WriteTxIns bug workaround
				Blob sig(0, len+6);
				byte *data = sig.data();
			
				data[0] = byte(len+5);
				data[1] = 0x30;
				data[2] = byte(len+2);
				data[3] = 2;
				data[4] = 0x20 | ((typ & 0x20)>>5);
				rd.Read(data+5, data[4]);
				data[5+data[4]] = 2;
				byte len2 = byte(len-data[4]-2);
				data[5+data[4]+1] = len2;
				rd.Read(data+5+data[4]+2, len2);
				data[5+len] = 1;
				if (typ & 0x40) {
					txIn.m_sig = sig;
				} else
					txIn.m_script = sig;
			}
		}
		txIn.Sequence = (typ & 0x80) ? uint32_t(-1) : uint32_t(rd.Read7BitEncoded());
		m_txIns.push_back(txIn);
	}
}

static mutex s_mtxTx;

const vector<TxIn>& TxObj::TxIns() const {
	if (!m_bLoadedIns) {
		EXT_LOCK (s_mtxTx) {
			if (!m_bLoadedIns) {
				ASSERT(m_nBytesOfHash && m_txIns.empty());
				Eng().Db->ReadTxIns(m_hash, _self);
				m_bLoadedIns = true;
			}
		}
	}
	return m_txIns;
}

void TxObj::Write(BinaryWriter& wr) const {
	wr << Ver;
	WritePrefix(wr);
	CoinSerialized::Write(wr, TxIns());
	CoinSerialized::Write(wr, TxOuts);
	wr << LockBlock;
	WriteSuffix(wr);
}

void TxObj::Read(const BinaryReader& rd) {
	ASSERT(!m_bLoadedIns);
	DBG_LOCAL_IGNORE_CONDITION(CoinErr::Misbehaving);

	Ver = rd.ReadUInt32();
	ReadPrefix(rd);
	CoinSerialized::Read(rd, m_txIns);
	m_bLoadedIns = true;
	CoinSerialized::Read(rd, TxOuts);
	if (m_txIns.empty() || TxOuts.empty())
		throw PeerMisbehavingException(10);
	LockBlock = rd.ReadUInt32();
	if (LockBlock >= 500000000)
		LockTimestamp = DateTime::from_time_t(LockBlock);
	ReadSuffix(rd);
}

const DateTime LOCKTIME_THRESHOLD(1985, 11, 5, 0, 53, 20);

bool TxObj::IsFinal(int height, const DateTime dt) const {
    if (LockTimestamp.Ticks == 0)
        return true;
/*!!!        if (height == 0)
        nBlockHeight = nBestHeight;
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime(); */
    if (LockTimestamp < (LockTimestamp<LOCKTIME_THRESHOLD ? DateTime::from_time_t(height) : dt))
        return true;
	return AllOf(TxIns(), bind(&TxIn::IsFinal, _1));
/*!!!	EXT_FOR (const TxIn& txIn, TxIns()) {
        if (!txIn.IsFinal())
            return false;
	}
    return true;*/
}

bool Tx::AllowFree(double priority) {
	CoinEng& eng = Eng();
	return eng.AllowFreeTxes && (priority > eng.ChainParams.CoinValue * 144 / 250);
}

bool Tx::TryFromDb(const HashValue& hash, Tx *ptx) {
	CoinEng& eng = Eng();

	EXT_LOCK(eng.Caches.Mtx) {
		ChainCaches::CHashToTxCache::iterator it = eng.Caches.HashToTxCache.find(hash);
		if (it != eng.Caches.HashToTxCache.end()) {
			if (ptx)
				*ptx = it->second.first;
			return true;
		}
	}
	if (!eng.Db->FindTx(hash, ptx))
		return false;
	if (ptx) {
		// ASSERT(ReducedHashValue(Hash(*ptx)) == ReducedHashValue(hash));

		EXT_LOCKED(eng.Caches.Mtx, eng.Caches.HashToTxCache.insert(make_pair(hash, *ptx)));
	}
	return true;
}

Tx Tx::FromDb(const HashValue& hash) {
	Coin::Tx r;
	if (!TryFromDb(hash, &r)) {
		TRC(2, "NotFound Tx: " << hash);
		throw TxNotFoundException(hash);
	}
	return r;
}

void Tx::EnsureCreate() {
	if (!m_pimpl)
		m_pimpl = Eng().CreateTxObj();
}

void Tx::Write(BinaryWriter& wr) const {
	m_pimpl->Write(wr);
}

void Tx::Read(const BinaryReader& rd) {
	EnsureCreate();
	m_pimpl->Read(rd);
}

bool Tx::IsNewerThan(const Tx& txOld) const {
	if (TxIns().size() != txOld.TxIns().size())
		return false;
	bool r = false;
	uint32_t lowest = UINT_MAX;
	for (int i=0; i<TxIns().size(); ++i) {
		const TxIn &txIn = TxIns()[i],
			&oldIn = txOld.TxIns()[i];
		if (txIn.PrevOutPoint != oldIn.PrevOutPoint)
			return false;
		if (txIn.Sequence != oldIn.Sequence) {
			if (txIn.Sequence <= lowest)
				r = false;
			if (oldIn.Sequence <= lowest)
				r = true;
			lowest = std::min(lowest, std::min(txIn.Sequence, oldIn.Sequence));
		}
	}
	return r;
}

void Tx::SpendInputs() const {
	if (!IsCoinBase()) {
		CoinEng& eng = Eng();

		EXT_FOR (const TxIn& txIn, TxIns()) {
			eng.Db->UpdateCoins(txIn.PrevOutPoint, true);
		}
	}
}

Blob ToCompressedKey(const ConstBuf& cbuf) {
	ASSERT(cbuf.Size==33 || cbuf.Size==65);

	if (cbuf.Size == 33)
		return cbuf;
	Blob r = CCrypter::PublicKeyBlobToCompressedBlob(cbuf);
	r.data()[0] |= 0x80;
	if (cbuf.P[0] & 2)
		r.data()[0] |= 4; // POINT_CONVERSION_HYBRID
#ifdef X_DEBUG//!!!D
	Blob test(r);
	test.data()[0] &= 0x7F;
	ASSERT(ConstBuf(test) == ConstBuf(cbuf.P, 33));
#endif
#ifdef X_DEBUG//!!!D
	ASSERT(ToUncompressedKey(r) == Blob(cbuf));
#endif
	return r;
}


/*!!!!ERROR
Blob ToCompressedKeyWithoutCheck(const ConstBuf& cbuf) {							// Just truncates last 32 bytes. Use only after checking hashes!
	ASSERT(cbuf.Size==33 || cbuf.Size==65);

	if (cbuf.Size == 33)
		return cbuf;
	Blob r(cbuf.P, 33);
	r.data()[0] |= 0x80;
	return r;
}
*/

static const BigInteger s_ec_p("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F", 16);

Blob ToUncompressedKey(const ConstBuf& cbuf) {
	if (cbuf.Size != 33 || !(cbuf.P[0] & 0x80))
		return cbuf;
	BigInteger x = OpensslBn::FromBinary(ConstBuf(cbuf.P+1, 32)).ToBigInteger();
	BigInteger xr = (x*x*x+7) % s_ec_p;
	BigInteger y = sqrt_mod(xr, s_ec_p);
	if (y.TestBit(0) != bool(cbuf.P[0] & 1))
		y = s_ec_p-y;
	Blob r(0, 65);
	r.data()[0] = 4;									//	POINT_CONVERSION_UNCOMPRESSED
	if (cbuf.P[0] & 4) {
		r.data()[0] |= 2 | (cbuf.P[0] & 1);				//  POINT_CONVERSION_HYBRID
	}
	memcpy(r.data()+1, cbuf.P+1, 32);
	OpensslBn(y).ToBinary(r.data()+33, 32);
	return r;
}

PubKeyHash160 DbPubKeyToHashValue160(const ConstBuf& mb) {
	return mb.Size == 20
		? PubKeyHash160(nullptr, HashValue160(mb))
		: PubKeyHash160(ToUncompressedKey(mb));
}

HashValue160 CoinEng::GetHash160ById(int64_t id) {
	EXT_LOCK (Caches.Mtx) {
		ChainCaches::CCachePkIdToPubKey::iterator it = Caches.m_cachePkIdToPubKey.find(id);	
		if (it != Caches.m_cachePkIdToPubKey.end())
			return it->second.first.Hash160;
	}
	Blob pk = Db->FindPubkey(id);
	if (!!pk) {
		PubKeyHash160 pkh = DbPubKeyToHashValue160(pk);
#ifdef X_DEBUG//!!!D
		CIdPk idpk(pkh.Hash160);
		ASSERT(idpk == id);
#endif
		EXT_LOCK (Caches.Mtx) {
			if (Caches.PubkeyCacheEnabled)
				Caches.m_cachePkIdToPubKey.insert(make_pair(id, pkh));
		}
		return pkh.Hash160;
	} else
		Throw(ExtErr::DB_NoRecord);
}

Blob CoinEng::GetPkById(int64_t id) {
	EXT_LOCK (Caches.Mtx) {
		ChainCaches::CCachePkIdToPubKey::iterator it = Caches.m_cachePkIdToPubKey.find(id);	
		if (it != Caches.m_cachePkIdToPubKey.end())
			return it->second.first.PubKey;
	}
	Blob pk = Db->FindPubkey(id);
	if (!!pk) {
		if (pk.Size == 20)
			Throw(CoinErr::InconsistentDatabase);
		PubKeyHash160 pkh = PubKeyHash160(ToUncompressedKey(pk));
		EXT_LOCK (Caches.Mtx) {
			if (Caches.PubkeyCacheEnabled)
				Caches.m_cachePkIdToPubKey.insert(make_pair(id, pkh));
		}
		return pkh.PubKey;
	} else
		Throw(ExtErr::DB_NoRecord);
}

bool CoinEng::GetPkId(const HashValue160& hash160, CIdPk& id) {
	id = CIdPk(hash160);
	EXT_LOCK (Caches.Mtx) {
		ChainCaches::CCachePkIdToPubKey::iterator it = Caches.m_cachePkIdToPubKey.find(id);	
		if (it != Caches.m_cachePkIdToPubKey.end())
			return hash160 == it->second.first.Hash160;
	}

	PubKeyHash160 pkh;
	Blob pk = Db->FindPubkey((int64_t)id);
	if (!!pk) {
		if ((pkh = DbPubKeyToHashValue160(pk)).Hash160 != hash160)
			return false;
	} else {
		Throw(ExtErr::CodeNotReachable); //!!!
		Db->InsertPubkey((int64_t)id, hash160);
		pkh = PubKeyHash160(nullptr, hash160);
	}
	EXT_LOCK (Caches.Mtx) {
		if (Caches.PubkeyCacheEnabled)
			Caches.m_cachePkIdToPubKey.insert(make_pair(id, pkh));
	}
	return true;
}

bool CoinEng::GetPkId(const ConstBuf& cbuf, CIdPk& id) {
	ASSERT(cbuf.Size != 20);

	if (!IsPubKey(cbuf))
		return false;

	HashValue160 hash160 = Hash160(cbuf);
	id = CIdPk(hash160);
	Blob compressed;
	try {
		DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, EC_R_POINT_IS_NOT_ON_CURVE));
		DBG_LOCAL_IGNORE(MAKE_HRESULT(SEVERITY_ERROR, FACILITY_OPENSSL, ERR_R_EC_LIB));

		compressed = ToCompressedKey(cbuf);
	} catch (RCExc) {
		return false;
	}
	Blob pk = Db->FindPubkey((int64_t)id);
	if (!!pk) {
		ConstBuf mb = pk;
		if (mb.Size != 20) {
			return mb == compressed;
		}
		if (hash160 != HashValue160(mb))
			return false;
		Throw(ExtErr::CodeNotReachable); //!!!
		Db->UpdatePubkey((int64_t)id, compressed);
		EXT_LOCK (Caches.Mtx) {
			Caches.m_cachePkIdToPubKey[id] = PubKeyHash160(cbuf, hash160);
		}
		return true;
	}
	Throw(ExtErr::CodeNotReachable); //!!!
	Db->InsertPubkey((int64_t)id, compressed);
	return true;
}

void TxOut::CheckForDust() const {
	if (Value < 3*(Eng().ChainParams.MinTxFee * (EXT_BIN(_self).Size + 148) / 1000))
		Throw(CoinErr::TxAmountTooSmall);
}

const Blob& TxOut::get_PkScript() const {
	if (!m_pkScript) {
		CoinEng& eng = Eng();
		switch (m_typ) {
		case 20:
			{
				byte ar[25] = { OP_DUP, OP_HASH160,
								20,
								0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
								OP_EQUALVERIFY, OP_CHECKSIG };
				HashValue160 hash160 = eng.GetHash160ById(m_idPk);
				memcpy(ar+3, hash160.data(), 20);
				m_pkScript.AssignIfNull(Blob(ar, sizeof ar));
			}
			break;
		case 33:
		case 65:
			{
				Blob pk = eng.GetPkById(m_idPk);
				ASSERT(pk.Size == 33 || pk.Size == 65);
				Blob pkScript(0, pk.Size+2);
				pkScript.data()[0] = byte(pk.Size);
				memcpy(pkScript.data()+1, pk.constData(), pk.Size);
				pkScript.data()[pk.Size+1] = OP_CHECKSIG;
				m_pkScript.AssignIfNull(pkScript);
			}
			break;
		default:
			Throw(ExtErr::CodeNotReachable);
		}
	}
	return m_pkScript;
}

bool IsCanonicalSignature(const ConstBuf& sig) {
	if (sig.Size<9 || sig.Size>73 || sig.P[0]!=0x30)
		return false;
	//!!!TODO

	return true;
}

ConstBuf FindStandardHashAllSigPubKey(const ConstBuf& cbuf) {
	ConstBuf r(0, 0);
	if (cbuf.Size > 64) {
		int len = cbuf.P[0];
		if (len>64 && len<64+32 && cbuf.P[1]==0x30 && cbuf.P[2]==len-3 && cbuf.P[3]==2) {
			switch (int len1 = cbuf.P[4]) {
			case 0x1F:
			case 0x20:
			case 0x21:
				if (cbuf.P[5+len1] == 2 && cbuf.P[5+len1+1]+len1+4 == len-3 && cbuf.P[len]==1) {
					if (cbuf.Size == len+1)
						r = ConstBuf(cbuf.P+len+1, 0);
					else if (cbuf.Size > len+2) {
						int len2 = cbuf.P[1+len];
						if ((len2==33 || len2==65) && cbuf.Size == len+len2+2)
							r = ConstBuf(cbuf.P+len+1, len2+1);
					}
				}
			}			
		}
	}
	return r;
}

DbWriter& operator<<(DbWriter& wr, const Tx& tx) {
	CoinEng& eng = Eng();
	
	if (!wr.BlockchainDb) {
		wr.Write7BitEncoded(tx.m_pimpl->Ver);
		tx.m_pimpl->WritePrefix(wr);
		wr << tx.TxIns() << tx.TxOuts();
		wr.Write7BitEncoded(tx.LockBlock);
		tx.m_pimpl->WriteSuffix(wr);
	} else {
		uint64_t v = uint64_t(tx.m_pimpl->Ver) << 2;
		if (tx.IsCoinBase())
			v |= 1;
		if (tx.LockBlock)
			v |= 2;
		wr.Write7BitEncoded(v);
		tx.m_pimpl->WritePrefix(wr);
		if (tx.LockBlock)
			wr.Write7BitEncoded(tx.LockBlock);
		tx.m_pimpl->WriteSuffix(wr);

		for (int i=0; i<tx.TxOuts().size(); ++i) {
			const TxOut& txOut = tx.TxOuts()[i];
#if UCFG_COIN_PUBKEYID_36BITS
			uint64_t valtyp = uint64_t(txOut.Value) << 6;
#else
			uint64_t valtyp = uint64_t(txOut.Value) << 5;
#endif
			HashValue160 hash160;
			Blob pk;
			CIdPk idPk;
			switch (byte typ = txOut.TryParseDestination(hash160, pk)) {
			case 20:
			case 33:
			case 65:
				if (eng.Mode==EngMode::Lite) {
					wr.Write7BitEncoded(valtyp | 3);
					break;
				} else {
					CConnectJob::CMap::iterator it = wr.ConnectJob->Map.find(hash160);
					if (it != wr.ConnectJob->Map.end() && !!it->second.PubKey) {
						idPk = CIdPk(hash160);
#if UCFG_COIN_PUBKEYID_36BITS
						wr.Write7BitEncoded(valtyp | (20==typ ? 1 : 2) | ((int64_t(idPk) & 0xF00000000)>>30));
#else
						wr.Write7BitEncoded(valtyp | (20==typ ? 1 : 2) | ((int64_t(idPk) & 0x700000000)>>30));
#endif
						wr << uint32_t(int64_t(txOut.m_idPk = idPk));
						break;
					}
				}
			default:
				wr.Write7BitEncoded(valtyp);						// SCRIPT_PKSCRIPT_GEN
				CoinSerialized::WriteBlob(wr, txOut.get_PkScript());
			}
		}
	}	
	return wr;
}

const DbReader& operator>>(const DbReader& rd, Tx& tx) {
	CoinEng& eng = Eng();

	uint64_t v;
//!!!?	if (tx.Ver != 1)
//		Throw(ExtErr::New_Protocol_Version);

	tx.EnsureCreate();

	if (!rd.BlockchainDb) {
		v = rd.Read7BitEncoded();
		tx.m_pimpl->Ver = (uint32_t)v;
		tx.m_pimpl->ReadPrefix(rd);
		rd >> tx.m_pimpl->m_txIns >> tx.m_pimpl->TxOuts;
		tx.m_pimpl->m_bLoadedIns = true;
		tx.m_pimpl->LockBlock = (uint32_t)rd.Read7BitEncoded();
		tx.m_pimpl->ReadSuffix(rd);
	} else {
		v = rd.Read7BitEncoded();
		tx.m_pimpl->Ver = (uint32_t)(v >> 2);
		tx.m_pimpl->ReadPrefix(rd);
		tx.m_pimpl->m_bIsCoinBase = v & 1;
		if (v & 2)
			tx.LockBlock = (uint32_t)rd.Read7BitEncoded();
		tx.m_pimpl->ReadSuffix(rd);

		while (!rd.BaseStream.Eof()) {
			uint64_t valtyp = rd.Read7BitEncoded();
#if UCFG_COIN_PUBKEYID_36BITS
			uint64_t value = valtyp>>6;
#else
			uint64_t value = valtyp>>5;
#endif
			tx.TxOuts().push_back(TxOut(value, Blob(nullptr)));
			TxOut& txOut = tx.TxOuts().back();
			static const byte s_types[] = { 0, 20, 33, 0x7F };
			switch (byte typ = s_types[valtyp & 3]) {
			case 0:
				txOut.m_pkScript = CoinSerialized::ReadBlob(rd);
				break;
			case 20:
			case 33:
			case 65:
				txOut.m_typ = typ;
				txOut.m_pkScript = nullptr;
#if UCFG_COIN_PUBKEYID_36BITS
				txOut.m_idPk = CIdPk(int64_t(rd.ReadUInt32()) | ((valtyp & 0x3C) << 30));
#else
				txOut.m_idPk = CIdPk(int64_t(rd.ReadUInt32()) | ((valtyp & 0x1C) << 30));
#endif
				break;
			default:
				Throw(E_FAIL);
			}		
		}
	}	
	if (tx.LockBlock >= 500000000)
		tx.m_pimpl->LockTimestamp = DateTime::from_time_t(tx.LockBlock);
	return rd;
}

void Tx::Check() const {
	CoinEng& eng = Eng();

	if (TxIns().empty())
		Throw(CoinErr::BadTxnsVinEmpty);
	if (TxOuts().empty())
		Throw(CoinErr::BadTxnsVoutEmpty);

	bool bIsCoinBase = IsCoinBase();
	int64_t nOut = 0;
	EXT_FOR(const TxOut& txOut, TxOuts()) {
        if (!txOut.IsEmpty()) {
			if (txOut.Value < 0)
				Throw(CoinErr::BadTxnsVoutNegative);
			if (!bIsCoinBase && txOut.Value < eng.ChainParams.MinTxOutAmount)
				Throw(CoinErr::TxOutBelowMinimum);
		}
		eng.CheckMoneyRange(nOut += eng.CheckMoneyRange(txOut.Value));
	}
	unordered_set<OutPoint> outPoints;
	EXT_FOR(const TxIn& txIn, TxIns()) {									// Check for duplicate inputs
		if (!outPoints.insert(txIn.PrevOutPoint).second)
			Throw(CoinErr::DupTxInputs);
		if (!bIsCoinBase && txIn.PrevOutPoint.IsNull())
			Throw(CoinErr::BadTxnsPrevoutNull);
	}
	if (bIsCoinBase && !between(int(TxIns()[0].Script().Size), 2, 100))
		Throw(CoinErr::BadCbLength);

	eng.OnCheck(_self);
}

void CoinEng::UpdateMinFeeForTxOuts(int64_t& minFee, const int64_t& baseFee, const Tx& tx) {
    if (minFee < baseFee) {                   // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.001		//!!! Bitcoin requires min 0.01
		const int64_t cent = ChainParams.CoinValue / 1000;
		EXT_FOR (const TxOut txOut, tx.TxOuts()) {
            if (txOut.Value < cent) {
                minFee = baseFee;
				break;
			}
		}
	}
}

uint32_t Tx::GetSerializeSize() const {
	return EXT_BIN(_self).Size;
}

int64_t Tx::GetMinFee(uint32_t blockSize, bool bAllowFree, MinFeeMode mode, uint32_t nBytes) const {
	CoinEng& eng = Eng();
    int64_t nBaseFee = mode==MinFeeMode::Relay ? eng.GetMinRelayTxFee() : eng.ChainParams.MinTxFee;

	if (uint32_t(-1) == nBytes)
		nBytes = GetSerializeSize();
    uint32_t nNewBlockSize = blockSize + nBytes;
    int64_t nMinFee = (1 + (int64_t)nBytes / 1000) * nBaseFee;

    if (bAllowFree) {
        if (blockSize == 1) {          
            if (nBytes < MAX_FREE_TRANSACTION_CREATE_SIZE)
                nMinFee = 0;					// (about 4500bc if made of 50bc inputs)
        } else if (nNewBlockSize < 27000)		// Free transaction area
			nMinFee = 0;
    }
    
	eng.UpdateMinFeeForTxOuts(nMinFee, nBaseFee, _self);
    
    if (blockSize != 1 && nNewBlockSize >= MAX_BLOCK_SIZE_GEN/2) {					// Raise the price as the block approaches full
        if (nNewBlockSize >= MAX_BLOCK_SIZE_GEN)
            return eng.ChainParams.MaxMoney;
        nMinFee *= MAX_BLOCK_SIZE_GEN / (MAX_BLOCK_SIZE_GEN - nNewBlockSize);
    }

	try {
		eng.CheckMoneyRange(nMinFee);
	} catch (RCExc) {
		return eng.ChainParams.MaxMoney;
	}
    return nMinFee;
}

int64_t Tx::get_ValueOut() const {
	CoinEng& eng = Eng();

	int64_t r = 0;
	EXT_FOR (const TxOut& txOut, TxOuts()) {
		eng.CheckMoneyRange(r += eng.CheckMoneyRange(txOut.Value));
	}
	return r;
}

int64_t Tx::get_Fee() const {
	if (IsCoinBase())
		return 0;
	int64_t sum = 0;
	EXT_FOR (const TxIn& txIn, TxIns()) {
		sum += Tx::FromDb(txIn.PrevOutPoint.TxHash).TxOuts().at(txIn.PrevOutPoint.Index).Value;
	}
	return sum - ValueOut;
}

int Tx::get_DepthInMainChain() const {
	CoinEng& eng = Eng();
	Block bestBlock = eng.BestBlock();
	return Height >= 0 && bestBlock ? bestBlock.Height-Height+1 : 0;
}

int Tx::GetP2SHSigOpCount(const CTxMap& txMap) const {
	if (IsCoinBase())
		return 0;
	int r = 0;
	EXT_FOR (const TxIn& txIn, TxIns()) {
		const TxOut& txOut = txMap.GetOutputFor(txIn.PrevOutPoint);
		if (IsPayToScriptHash(txOut.get_PkScript()))
			r += CalcSigOpCount(txOut.get_PkScript(), txIn.Script());
	}
	return r;
}

void Tx::CheckInOutValue(int64_t nValueIn, int64_t& nFees, int64_t minFee, const Target& target) const {
	int64_t valOut = ValueOut;
	if (IsCoinStake())
		m_pimpl->CheckCoinStakeReward(valOut - nValueIn, target);
	else {
		if (nValueIn < valOut)
			Throw(CoinErr::ValueInLessThanValueOut);
		int64_t nTxFee = nValueIn-valOut;
		if (nTxFee < minFee)
			Throw(CoinErr::TxFeeIsLow);
		nFees = Eng().CheckMoneyRange(nFees + nTxFee);
	}
}

void Tx::ConnectInputs(CoinsView& view, int32_t height, int& nBlockSigOps, int64_t& nFees, bool bBlock, bool bMiner, int64_t minFee, const Target& target) const {
	CoinEng& eng = Eng();

	HashValue hashTx = Hash(_self);
	int nSigOp = nBlockSigOps + SigOpCount;
	if (!IsCoinBase()) {
		vector<Tx> vTxPrev;
		int64_t nValueIn = 0;
		if (eng.Mode!=EngMode::Lite) {
//			TRC(3, TxIns.size() << " TxIns");

			for (int i=0; i<TxIns().size(); ++i) {
				const TxIn& txIn = TxIns()[i];
				OutPoint op = txIn.PrevOutPoint;
		
				Tx txPrev;
				if (bBlock || bMiner) {
					CTxMap::iterator it = view.TxMap.find(op.TxHash);
					txPrev = it!=view.TxMap.end() ? it->second : FromDb(op.TxHash);
				} else {
					EXT_LOCK (eng.TxPool.Mtx) {
						CoinEng::CTxPool::CHashToTx::iterator it = eng.TxPool.m_hashToTx.find(op.TxHash);
						if (it != eng.TxPool.m_hashToTx.end())
							txPrev = it->second;
					}
					if (!txPrev)
						txPrev = FromDb(op.TxHash);
				}
				if (op.Index >= txPrev.TxOuts().size())
					Throw(E_FAIL);
				const TxOut& txOut = txPrev.TxOuts()[op.Index];
				if (t_bPayToScriptHash) {
					if ((nSigOp += CalcSigOpCount(txOut.PkScript, txIn.Script())) > MAX_BLOCK_SIGOPS)
						Throw(CoinErr::TxTooManySigOps);
				}

				if (txPrev.IsCoinBase())
					eng.CheckCoinbasedTxPrev(height, txPrev);

				if (!view.HasInput(op))
					Throw(CoinErr::InputsAlreadySpent);
				view.SpendInput(op);

				if (!bBlock || eng.BestBlockHeight() > eng.ChainParams.LastCheckpointHeight-INITIAL_BLOCK_THRESHOLD)	// Skip ECDSA signature verification when connecting blocks (fBlock=true) during initial download
					VerifySignature(txPrev, _self, i);

				eng.CheckMoneyRange(nValueIn += txOut.Value);
				vTxPrev.push_back(txPrev);
			}
		}

		if (bBlock)
			eng.OnConnectInputs(_self, vTxPrev, bBlock, bMiner);

		if (eng.Mode!=EngMode::Lite)
			CheckInOutValue(nValueIn, nFees, minFee, target);
	}
	if (nSigOp > MAX_BLOCK_SIGOPS)
		Throw(CoinErr::TxTooManySigOps);
	nBlockSigOps = nSigOp;
	view.TxMap[hashTx] = _self;
}

int Tx::get_SigOpCount() const {
	int r = 0;
	EXT_FOR (const TxIn& txIn, TxIns()) {
		if (txIn.PrevOutPoint.Index >= 0)
			r += CalcSigOpCount1(txIn.Script());
	}
	EXT_FOR (const TxOut& txOut, TxOuts()) {
		try {																		//!!! should be more careful checking
			DBG_LOCAL_IGNORE_CONDITION(ExtErr::EndOfStream);
			
			r += CalcSigOpCount1(txOut.get_PkScript());
		} catch (RCExc) {			
		}
	}
	return r;
}

DbWriter& operator<<(DbWriter& wr, const CTxes& txes) {
	if (!wr.BlockchainDb)
		Throw(E_NOTIMPL);	
	EXT_FOR (const Tx& tx, txes) {
		wr << tx;
	}
	return wr;
}

const DbReader& operator>>(const DbReader& rd, CTxes& txes) {
	if (!rd.BlockchainDb)
		Throw(E_NOTIMPL);
	ASSERT(txes.empty());
	while (!rd.BaseStream.Eof()) {
		Tx tx;
		rd >> tx;
//!!!R		tx.m_pimpl->N = txes.size();
		tx.Height = rd.PCurBlock->Height;
		txes.push_back(tx);
	}
	return rd;
}


} // Coin::

