/*######   Copyright (c) 2013-2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include <el/bignum.h>
#include <el/crypto/hash.h>
using namespace Crypto;

#include "util.h"

namespace Coin {


static const char* s_pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

String ConvertToBase58ShaSquare(const ConstBuf& cbuf) {
	SHA256 sha;
	HashValue hash = HashValue(sha.ComputeHash(sha.ComputeHash(cbuf)));
	Blob v = cbuf + Blob(hash.data(), 4);
	vector<char> r;

	vector<byte> tmp(v.Size+1, 0);
	std::reverse_copy(v.begin(), v.end(), tmp.begin());
	for (BigInteger n(&tmp[0], tmp.size()); Sign(n);) {
		pair<BigInteger, BigInteger> pp = div(n, 58);
		n = pp.first;
		r.insert(r.begin(), s_pszBase58[explicit_cast<int>(pp.second)]);
	}

	for (int i=0; i<v.Size && !v.constData()[i]; ++i)
		r.insert(r.begin(), s_pszBase58[0]);
	return String(&r[0], r.size());
}

String ConvertToBase58(const ConstBuf& cbuf) {
	HashValue hash = HasherEng::GetCurrent()->HashForAddress(cbuf);
	Blob v = cbuf + Blob(hash.data(), 4);
	vector<char> r;

	vector<byte> tmp(v.Size+1, 0);
	std::reverse_copy(v.begin(), v.end(), tmp.begin());
	for (BigInteger n(&tmp[0], tmp.size()); Sign(n);) {
		pair<BigInteger, BigInteger> pp = div(n, 58);
		n = pp.first;
		r.insert(r.begin(), s_pszBase58[explicit_cast<int>(pp.second)]);
	}

	for (int i=0; i<v.Size && !v.constData()[i]; ++i)
		r.insert(r.begin(), s_pszBase58[0]);
	return String(&r[0], r.size());
}

Blob ConvertFromBase58ShaSquare(RCString s) {
	BigInteger bi = 0;
	for (const char *p=s; *p; ++p) {
		if (const char *q = strchr(s_pszBase58, *p)) {
			bi = bi*58 + BigInteger(q-s_pszBase58);
		} else
			Throw(E_INVALIDARG);
	}
	vector<byte> v((bi.Length+7)/8);
	bi.ToBytes(&v[0], v.size());
	if (v.size()>=2 && v.end()[-1]==0 && v.end()[-1]>=0x80)
		v.resize(v.size()-1);
	vector<byte> r;
	for (const char *p=s; *p==s_pszBase58[0]; ++p)
		r.push_back(0);
	r.resize(r.size()+v.size());
	std::reverse_copy(v.begin(), v.end(), r.end()-v.size());
	if (r.size() < 4)
		Throw(E_FAIL);
	SHA256 sha;
	HashValue hash = HashValue(sha.ComputeHash(sha.ComputeHash(ConstBuf(&r[0], r.size()-4))));
	if (memcmp(hash.data(), &r.end()[-4], 4))
		Throw(HRESULT_FROM_WIN32(ERROR_CRC));
	return Blob(&r[0], r.size()-4);
}

Blob ConvertFromBase58(RCString s, bool bCheckHash) {
	BigInteger bi = 0;
	for (const char *p=s; *p; ++p) {
		if (const char *q = strchr(s_pszBase58, *p)) {
			bi = bi*58 + BigInteger(q-s_pszBase58);
		} else
			Throw(errc::invalid_argument);
	}
	vector<byte> v((bi.Length+7)/8);
	bi.ToBytes(&v[0], v.size());
	if (v.size()>=2 && v.end()[-1]==0 && v.end()[-1]>=0x80)
		v.resize(v.size()-1);
	vector<byte> r;
	for (const char *p=s; *p==s_pszBase58[0]; ++p)
		r.push_back(0);
	r.resize(r.size()+v.size());
	std::reverse_copy(v.begin(), v.end(), r.end()-v.size());
	if (r.size() < 4)
		Throw(E_FAIL);
	if (bCheckHash) {
		HashValue hash = HasherEng::GetCurrent()->HashForAddress(ConstBuf(&r[0], r.size()-4));
		if (memcmp(hash.data(), &r.end()[-4], 4))
			Throw(HRESULT_FROM_WIN32(ERROR_CRC));
	}
	return Blob(&r[0], r.size()-4);
}


} // Coin::

