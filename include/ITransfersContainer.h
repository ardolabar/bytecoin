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
#include "IStreamSerializable.h"

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
    TransactionTypes::OutputType type;
    uint64_t amount;
    uint64_t globalOutputIndex;
    size_t outputInTransaction;

    // transaction info
    PublicKey transactionPublicKey;

    union {
      PublicKey outputKey;         // Type: Key 
      uint32_t requiredSignatures; // Type: Multisignature
    };
  };

  class ITransfersContainer: public IStreamSerializable {
  public:

    enum Flags : uint32_t {
      // state
      IncludeStateUnlocked = 0x01,
      IncludeStateLocked = 0x02,
      IncludeStateSoftLocked = 0x04,
      // output type
      IncludeTypeKey             = 0x100,
      IncludeTypeMultisignature  = 0x200,
      // combinations
      IncludeStateAll = 0xff,
      IncludeTypeAll = 0xff00,

      IncludeAllUnlocked = IncludeTypeAll | IncludeStateUnlocked,
      IncludeAll = IncludeTypeAll | IncludeStateAll,

      IncludeDefault = IncludeAllUnlocked
    };
   
    virtual size_t transfersCount() = 0;
    virtual size_t transactionsCount() = 0;
    virtual uint64_t balance(uint32_t flags = IncludeDefault) = 0;
    virtual void getOutputs(std::vector<TransactionOutputInformation>& transfers, uint32_t flags = IncludeDefault) = 0;
    virtual bool getTransactionInformation(const Hash& transactionHash, TransactionInformation& info) = 0;
    virtual bool getTransactionOutputs(const Hash& transactionHash, std::vector<TransactionOutputInformation>& transfers, uint32_t flags = IncludeDefault) = 0;
  };

}
