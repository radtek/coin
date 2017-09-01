/*######   Copyright (c) 2015 Ufasoft  http://yupitecoin.com  mailto:support@yupitecoin.com,  Sergey Pavlov  mailto:dev@yupitecoin.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>

#include "miner.h"

namespace Coin {

class GroestlHasher : public Hasher {
public:
	GroestlHasher()
		:	Hasher("groestl", HashAlgo::Groestl)
	{}

	HashValue CalcHash(const ConstBuf& cbuf) override {
		return GroestlHash(cbuf);
	}

} g_groestlHasher;



} // Coin::
