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
#include <unordered_map>
#include <mutex>


#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/random_access_index.hpp>

#include "crypto/crypto.h"
#include "cryptonote_core/cryptonote_basic.h"
#include "cryptonote_core/Currency.h"
#include "serialization/ISerializer.h"

#include "ITransaction.h"
#include "ITransfersContainer.h"
#include "SerializationHelpers.h"

namespace CryptoNote {

  struct TransactionOutputInformationIn : public TransactionOutputInformation {
    KeyImage keyImage;
  };

  struct TransactionOutputInformationEx : public TransactionOutputInformationIn {
    uint64_t unlockTime;
    uint64_t blockHeight;
    Hash transactionHash;

    const KeyImage& getKeyImage() const { return keyImage; }
    const Hash& getTransactionHash() const { return transactionHash; }

    void serialize(cryptonote::ISerializer& s, const std::string& name) {
      s(reinterpret_cast<uint8_t&>(type), "type");
      s(amount, "");
      s(globalOutputIndex, "");
      s(outputInTransaction, "");
      s(transactionPublicKey, "");
      s(keyImage, "");
      s(unlockTime, "");
      s(blockHeight, "");
      s(transactionHash, "");

      if (type == TransactionTypes::OutputType::Key)
        s(outputKey, "");
      else if (type == TransactionTypes::OutputType::Multisignature)
        s(requiredSignatures, "");
    }

  };

  struct BlockInfo {
    uint64_t height;
    uint64_t timestamp;

    void serialize(cryptonote::ISerializer& s, const std::string& name) {
      s(height, "height");
      s(timestamp, "timestamp");
    }
  };

  struct SpentTransactionOutput : TransactionOutputInformationEx {
    BlockInfo spendingBlock;
    Hash spendingTransactionHash;
    size_t inputInTransaction;

    void serialize(cryptonote::ISerializer& s, const std::string& name) {
      TransactionOutputInformationEx::serialize(s, name);
      s(spendingBlock, "spendingBlock");
      s(spendingTransactionHash, "spendingTransactionHash");
      s(inputInTransaction, "inputInTransaction");
    }
  };

  class TransfersContainer : public ITransfersContainer {

  public:

    TransfersContainer(const cryptonote::Currency& currency, size_t transactionSpendableAge);

    uint64_t addTransactionOutputs(const BlockInfo& block, const ITransactionReader& tx, const std::vector<TransactionOutputInformationIn>& transfers);
    uint64_t addTransactionInputs(const BlockInfo& block, const ITransactionReader& tx);

    void detach(uint64_t height);
    void updateHeight(uint64_t height);

    // ITransfersContainer
    virtual size_t transfersCount() override;
    virtual size_t transactionsCount() override;
    virtual uint64_t balance(uint32_t flags) override;
    virtual void getOutputs(std::vector<TransactionOutputInformation>& transfers, uint32_t flags) override;
    virtual bool getTransactionInformation(const Hash& transactionHash, TransactionInformation& info) override;
    virtual bool getTransactionOutputs(const Hash& transactionHash, std::vector<TransactionOutputInformation>& transfers, uint32_t flags) override;

    // IStreamSerializable
    virtual void save(std::ostream& os) override;
    virtual void load(std::istream& in) override;

  private:

    const TransactionInformation& addTransaction(const BlockInfo& block, const ITransactionReader& tx);
    bool isSpendTimeUnlocked(uint64_t unlockTime) const;
    bool isTransferUnlocked(const TransactionOutputInformationEx& info) const;
    bool isIncluded(const TransactionOutputInformationEx& info, uint32_t flags) const;

    bool markKeyImageSpent(const BlockInfo& block, const ITransactionReader& tx, size_t inputIndex, const KeyImage& img);
    bool markMultisignatureSpent(const BlockInfo& block, const ITransactionReader& tx, size_t inputIndex, const TransactionTypes::InputMultisignature& input);
    void copyToSpent(const BlockInfo& block, const ITransactionReader& tx, size_t inputIndex, const TransactionOutputInformationEx& output);

    std::unordered_map<Hash, TransactionInformation, boost::hash<Hash>> m_transactions;   

    typedef boost::multi_index_container <
      TransactionOutputInformationEx,
      boost::multi_index::indexed_by<
        boost::multi_index::random_access<>,
        boost::multi_index::hashed_non_unique<boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx, const KeyImage&, &TransactionOutputInformationEx::getKeyImage>>,
        boost::multi_index::hashed_non_unique<boost::multi_index::const_mem_fun<
          TransactionOutputInformationEx, const Hash&, &TransactionOutputInformationEx::getTransactionHash>>
      >
    > TransfersMultiIndex;
    
    TransfersMultiIndex m_transfers;
    TransfersMultiIndex::nth_index<1>::type& m_keyImagesIndex;
    TransfersMultiIndex::nth_index<2>::type& m_transactionIndex;

    std::vector<SpentTransactionOutput> m_spentTransfers;

    uint64_t m_currentHeight; // current height is needed to check if a tranfer is unlocked
    size_t m_transactionSpendableAge;
    const cryptonote::Currency& m_currency;
    std::mutex m_mutex;
  };

}