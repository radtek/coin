/*######   Copyright (c) 2012-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#pragma once

#include <el/inet/p2p-net.h>
using namespace Ext::Inet;
using P2P::Link;
using P2P::Peer;

#include "coin-model.h"
#include "eng.h"

namespace Coin {
	

class CoinEng;

ENUM_CLASS(InventoryType) {
	MSG_TX = 1,
	MSG_BLOCK = 2,
	MSG_FILTERED_BLOCK = 3
} END_ENUM_CLASS(InventoryType);

class Inventory : public CPersistent, public CPrintable {
public:
	InventoryType Type;
	Coin::HashValue HashValue;

	Inventory() {}

	Inventory(InventoryType typ, const Coin::HashValue& hash)
		:	Type(typ)
		,	HashValue(hash)
	{}

	bool operator==(const Inventory& inv) const {
		return Type==inv.Type && HashValue==inv.HashValue;
	}

	void Write(BinaryWriter& wr) const override {
		wr << uint32_t(Type) << HashValue;
	}

	void Read(const BinaryReader& rd) override {
		uint32_t typ;
		rd >> typ >> HashValue;
		Type = InventoryType(typ);
	}
protected:
	void Print(ostream& os) const override;
};

} namespace std {
	template <> struct hash<Coin::Inventory> {
		size_t operator()(const Coin::Inventory& inv) const {
			return hash<Coin::HashValue>()(inv.HashValue) + int(inv.Type);
		}
	};
} namespace Coin {

class CoinLink : public P2P::Link {
	typedef P2P::Link base;
public:
	LruCache<Inventory> KnownInvertorySet;
	
	typedef unordered_set<Inventory> CInvertorySetToSend;
	CInvertorySetToSend InvertorySetToSend;

	HashValue HashContinue;
	HashValue HashBlockLastUnknown;
	HashValue HashBlockLastCommon, HashBlockBestKnown;

	Block m_curMerkleBlock;
	vector<HashValue> m_curMatchedHashes;

	BlocksInFlightList BlocksInFlight;

	mutex MtxFilter;
	ptr<CoinFilter> Filter;
	int32_t LastReceivedBlock;
	CBool RelayTxes;
	CBool IsClient;
	CBool IsPreferredDownload;
	CBool IsSyncStarted;

	CoinLink(P2P::NetManager& netManager, thread_group& tr);
	void Push(const Inventory& inv);
	void Send(ptr<P2P::Message> msg) override;
	void UpdateBlockAvailability(const HashValue& hashBlock);
	void RequestHeaders();
	void RequestBlocks();
protected:
	void OnPeriodic() override;
};


class VersionMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	uint64_t Nonce, Services;
	uint32_t ProtocolVer;
	String UserAgent;
	int32_t LastReceivedBlock;
	mutable DateTime RemoteTimestamp;
	PeerInfoBase RemotePeerInfo, LocalPeerInfo;
	bool RelayTxes;

	VersionMessage();
protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link) override;
	void Print(ostream& os) const override;
};

class VerackMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	VerackMessage()
		:	base("verack")
	{
	}
protected:
	void Process(P2P::Link& link) override;
};

class GetAddrMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	GetAddrMessage()
		:	base("getaddr")
	{}
protected:
	void Process(P2P::Link& link) override;
};

class AddrMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	AddrMessage()
		:	base("addr")
	{}

	vector<PeerInfo> PeerInfos;
protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Print(ostream& os) const override;
	void Process(P2P::Link& link) override;
};

class InvGetDataMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	vector<Inventory> Invs;
protected:
	InvGetDataMessage(const char *cmd)
		:	base(cmd)
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link) override;
	void Print(ostream& os) const override;
};

class InvMessage : public InvGetDataMessage {
	typedef InvGetDataMessage base;
public:
	InvMessage()
		:	base("inv")
	{}
protected:
	void Process(P2P::Link& link) override;
};

class GetDataMessage : public InvGetDataMessage {
	typedef InvGetDataMessage base;
public:
	GetDataMessage()
		:	base("getdata")
	{}

	void Process(P2P::Link& link) override;
};

class NotFoundMessage : public InvGetDataMessage {
	typedef InvGetDataMessage base;
public:
	NotFoundMessage()
		:	base("notfound")
	{}

	void Process(P2P::Link& link) override;
};

class HeadersMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	vector<BlockHeader> Headers;

	HeadersMessage()
		:	base("headers")
	{}
protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Print(ostream& os) const override;
	void Process(P2P::Link& link) override;
};

class GetHeadersGetBlocksMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	uint32_t Ver;
	LocatorHashes Locators;
	HashValue HashStop;

	GetHeadersGetBlocksMessage(const char *cmd)
		:	base(cmd)
		,	Ver(Eng().ChainParams.ProtocolVersion) 
	{}

	void Set(const HashValue& hashLast, const HashValue& hashStop);
protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Print(ostream& os) const override;
};

class GetHeadersMessage : public GetHeadersGetBlocksMessage {
	typedef GetHeadersGetBlocksMessage base;
public:
	GetHeadersMessage()
		:	base("getheaders")
	{}

	GetHeadersMessage(const HashValue& hashLast, const HashValue& hashStop = HashValue::Null());
protected:
	void Process(P2P::Link& link) override;
};

class GetBlocksMessage : public GetHeadersGetBlocksMessage {
	typedef GetHeadersGetBlocksMessage base;
public:
	GetBlocksMessage()
		:	base("getblocks")
	{}	

	GetBlocksMessage(const HashValue& hashLast, const HashValue& hashStop);
protected:
	void Process(P2P::Link& link) override;
};

class TxMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Coin::Tx Tx;

	TxMessage(const Coin::Tx& tx = Coin::Tx())
		:	base("tx")
		,	Tx(tx)
	{}

	void Process(P2P::Link& link) override;
protected:
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Print(ostream& os) const override;
};

class BlockMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Coin::Block Block;

	BlockMessage(const Coin::Block& block = nullptr, const char *cmd = "block")
		:	base(cmd)
		,	Block(block)
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
protected:
	virtual void ProcessContent(P2P::Link& link);
	void Process(P2P::Link& link) override;
	void Print(ostream& os) const override;
};

class MerkleBlockMessage : public BlockMessage {
	typedef BlockMessage base;
public:
	CoinPartialMerkleTree PartialMT;

	MerkleBlockMessage()
		:	base(Coin::Block(), "merkleblock")
	{}

	vector<Tx> Init(const Coin::Block& block, CoinFilter& filter);
	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
protected:
	void ProcessContent(P2P::Link& link) override;
	void Process(P2P::Link& link) override;
};

class CheckOrderMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	CheckOrderMessage()
		:	base("checkorder")
	{}
};

class SubmitOrderMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	HashValue TxHash;

	SubmitOrderMessage()
		:	base("submitorder")
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
};

class ReplyMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	uint32_t Code;

	ReplyMessage()
		:	base("reply")
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
};

class PingPongMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	uint64_t Nonce;

	PingPongMessage(const char *cmd)
		:	base(cmd)
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
};

class PingMessage : public PingPongMessage {
	typedef PingPongMessage base;
public:
	PingMessage()
		:	base("ping")
	{
		Ext::Random().NextBytes(Buf(&Nonce, sizeof Nonce));
	}

	void Read(const BinaryReader& rd) override;
	void Process(P2P::Link& link) override;
};

class PongMessage : public PingPongMessage {
	typedef PingPongMessage base;
public:
	PongMessage()
		:	base("pong")
	{
		Nonce = 0;
	}
};

class MemPoolMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	MemPoolMessage()
		:	base("mempool")
	{}

	void Process(P2P::Link& link) override;
};

class AlertMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Blob Payload;
	Blob Signature;

	ptr<Coin::Alert> Alert;

	AlertMessage()
		:	base("alert")
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
protected:
	void Process(P2P::Link& link) override;
};

class FilterLoadMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	ptr<CoinFilter> Filter;

	FilterLoadMessage(CoinFilter *filter = 0)
		:	base("filterload")
		,	Filter(filter)
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
protected:
	void Process(P2P::Link& link) override;
};

class FilterAddMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	Blob Data;

	FilterAddMessage()
		:	base("filteradd")
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
protected:
	void Process(P2P::Link& link) override;
};

class FilterClearMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	FilterClearMessage()
		:	base("filterclear")
	{}
	
protected:
	void Process(P2P::Link& link) override {
		CoinLink& clink = (CoinLink&)link;
		EXT_LOCKED(clink.MtxFilter, clink.Filter = nullptr);
		clink.RelayTxes = true;
	}
};

ENUM_CLASS(RejectReason) {
	Malformed 	= 1,
	Invalid 	= 0x10,
	Obsolete	= 0x11,
	Duplicate	= 0x12,
	NonStandard	= 0x40,
	Dust		= 0x41,
	InsufficientFee	= 0x42,
	CheckPoint		= 0x43
} END_ENUM_CLASS(RejectReason);

class RejectMessage : public CoinMessage {
	typedef CoinMessage base;
public:
	String Command, Reason;
	HashValue Hash;
	RejectReason Code;

	RejectMessage(RejectReason code = RejectReason::Malformed, RCString command = String(), RCString reason = String())
		:	base("reject")
		,	Code(code)
		,	Command(command)
		,	Reason(reason)
	{}

	void Write(BinaryWriter& wr) const override;
	void Read(const BinaryReader& rd) override;
	void Print(ostream& os) const override;
protected:
	void Process(P2P::Link& link) override {
		TRC(2, ToString());
	}
};


} // Coin::
