// Copyright (c) 2012-2014, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>
#include "ITransaction.h"
#include "IObservable.h"

namespace CryptoNote {

  struct TransactionInformation {
    // transaction info
    Hash transactionHash;
    PublicKey publicKey;
    uint64_t blockHeight;
    uint64_t timestamp;
    uint64_t unlockTime;
    Hash paymentId;
  };

  struct TransactionOutputInformation {
    // output info
    uint64_t amount;
    uint64_t globalOutputIndex;
    size_t outputInTransaction;
    PublicKey outputKey;
    // transaction info
    PublicKey transactionPublicKey;
  };

  class ITransfersContainer {
  public:
    
    virtual size_t transfersCount() = 0;
    virtual size_t transactionsCount() = 0;

    virtual uint64_t balance() = 0;
    virtual uint64_t unlockedBalance() = 0;
    
    virtual void getUnlockedOutputs(std::vector<TransactionOutputInformation>& transfers) = 0;
    virtual bool getTransactionInformation(const Hash& transactionHash, TransactionInformation& info) = 0;
  };

}