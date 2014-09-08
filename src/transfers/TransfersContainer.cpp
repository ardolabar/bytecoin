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


#include "TransfersContainer.h"
#include "cryptonote_core/cryptonote_format_utils.h"

#include "serialization/BinaryInputStreamSerializer.h"
#include "serialization/BinaryOutputStreamSerializer.h"

namespace CryptoNote {

  void serialize(TransactionInformation& ti, const std::string& name, cryptonote::ISerializer& s) {
    s(ti.transactionHash, "");
    s(ti.publicKey, "");
    s(ti.blockHeight, "");
    s(ti.timestamp, "");
    s(ti.unlockTime, "");
    s(ti.paymentId, "");
  }

  const uint32_t TRANSFERS_CONTAINER_STORAGE_VERSION = 0;

  TransfersContainer::TransfersContainer(const cryptonote::Currency& currency, size_t transactionSpendableAge) :
    m_currentHeight(0),
    m_currency(currency),
    m_transactionSpendableAge(transactionSpendableAge),
    m_keyImagesIndex(m_transfers.get<1>()),
    m_transactionIndex(m_transfers.get<2>()) {
  }

  uint64_t TransfersContainer::addTransactionOutputs(
    const BlockInfo& block,
    const ITransactionReader& tx,
    const std::vector<TransactionOutputInformationIn>& transfers) {

    std::unique_lock<std::mutex> lk(m_mutex);
    uint64_t amount = 0;

    auto txHash = tx.getTransactionHash();

    for (const auto& transfer : transfers) {
      TransactionOutputInformationEx info;
      static_cast<TransactionOutputInformationIn&>(info) = transfer;
      info.blockHeight = block.height;
      info.unlockTime = tx.getUnlockTime();
      info.transactionHash = txHash;

      m_transfers.push_back(info);

      amount += info.amount;
    }

    addTransaction(block, tx);
    return amount;
  }

  const TransactionInformation& TransfersContainer::addTransaction(const BlockInfo& block, const ITransactionReader& tx) {
    auto txHash = tx.getTransactionHash();
    auto it = m_transactions.find(txHash);

    if (it != m_transactions.end()) {
      return it->second;
    }

    TransactionInformation txInfo;
    txInfo.blockHeight = block.height;
    txInfo.timestamp = block.timestamp;
    txInfo.transactionHash = txHash;
    txInfo.unlockTime = tx.getUnlockTime();
    txInfo.publicKey = tx.getTransactionPublicKey();

    if (!tx.getPaymentId(txInfo.paymentId))
      txInfo.paymentId.fill(0);

    auto res = m_transactions.insert(std::make_pair(txInfo.transactionHash, txInfo));
    return res.first->second;
  }

  uint64_t TransfersContainer::addTransactionInputs(const BlockInfo& block, const ITransactionReader& tx) {
    std::unique_lock<std::mutex> lk(m_mutex);
    uint64_t amount = 0;

    for (size_t i = 0; i < tx.getInputCount(); ++i) {
      auto inputType = tx.getInputType(i);

      if (inputType == TransactionTypes::InputType::Key) {
        TransactionTypes::InputKey input;
        tx.getInput(i, input);
        if (markKeyImageSpent(block, tx, i, input.keyImage)) {
          amount += input.amount;
        }
      } else if (inputType == TransactionTypes::InputType::Multisignature) {
        TransactionTypes::InputMultisignature input;
        tx.getInput(i, input);
        if (markMultisignatureSpent(block, tx, i, input)) {
          amount += input.amount;
        }
      }
    }

    if (amount > 0) {
      addTransaction(block, tx);
    }

    return amount;
  }

  bool TransfersContainer::markKeyImageSpent(const BlockInfo& block, const ITransactionReader& tx, size_t inputIndex, const KeyImage& img) {
    auto it = m_keyImagesIndex.find(img);
    if (it == m_keyImagesIndex.end())
      return false;
    copyToSpent(block, tx, inputIndex, *it);
    // erase from available outputs
    m_keyImagesIndex.erase(it);
    return true;
  }

  bool TransfersContainer::markMultisignatureSpent(const BlockInfo& block, const ITransactionReader& tx, size_t inputIndex, const TransactionTypes::InputMultisignature& input) {
    // do linear search
    for (auto it = m_transfers.begin(); it != m_transfers.end(); ++it) {
      if (it->type == TransactionTypes::OutputType::Multisignature) {
        if (it->amount == input.amount && it->globalOutputIndex == input.outputIndex) {
          copyToSpent(block, tx, inputIndex, *it);
          m_transfers.erase(it);
          return true;
        }
      }
    }
    return false;
  }

  void TransfersContainer::copyToSpent(const BlockInfo& block, const ITransactionReader& tx, size_t inputIndex, const TransactionOutputInformationEx& output) {
    SpentTransactionOutput spentOutput;
    static_cast<TransactionOutputInformationEx&>(spentOutput) = output;
    spentOutput.spendingBlock = block;
    spentOutput.spendingTransactionHash = tx.getTransactionHash();
    spentOutput.inputInTransaction = inputIndex;
    m_spentTransfers.push_back(spentOutput);
  }

  void TransfersContainer::detach(uint64_t height) {
    std::lock_guard<std::mutex> lk(m_mutex);

    size_t transfersDetached = 0;
    size_t transactionsDetached = 0;

    for (auto it = m_transfers.begin(); it != m_transfers.end();) {
      if (it->blockHeight >= height) {
        it = m_transfers.erase(it);
        ++transfersDetached;
      } else {
        ++it;
      }
    }

    for (auto it = m_transactions.begin(); it != m_transactions.end();) {
      if (it->second.blockHeight >= height) {
        it = m_transactions.erase(it);
        ++transactionsDetached;
      } else {
        ++it;
      }
    }

    // TODO: notification on detach
    m_currentHeight = height;
  }

  void TransfersContainer::updateHeight(uint64_t height) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_currentHeight = height;
  }

  size_t TransfersContainer::transfersCount() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_transfers.size() + m_spentTransfers.size();
  }

  size_t TransfersContainer::transactionsCount() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_transactions.size();
  }

  uint64_t TransfersContainer::balance(uint32_t flags) {
    std::lock_guard<std::mutex> lk(m_mutex);
    uint64_t amount = 0;
    for (const auto& t : m_transfers) {
      if (isIncluded(t, flags)) {
        amount += t.amount;
      }
    }
    return amount;
  }

  void TransfersContainer::getOutputs(std::vector<TransactionOutputInformation>& transfers, uint32_t flags) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (const auto& t : m_transfers) {
      if (isIncluded(t, flags)) {
        transfers.push_back(t);
      }
    }
  }

  bool TransfersContainer::getTransactionInformation(const Hash& transactionHash, TransactionInformation& info) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_transactions.find(transactionHash);
    if (it == m_transactions.end())
      return false;
    info = it->second;
    return true;
  }

  bool TransfersContainer::getTransactionOutputs(
    const Hash& transactionHash, std::vector<TransactionOutputInformation>& transfers, uint32_t flags) {
    std::lock_guard<std::mutex> lk(m_mutex);

    auto range = m_transactionIndex.equal_range(transactionHash);

    if (range.first == range.second) {
      return false; // empty range
    }

    for (auto i = range.first; i != range.second; ++i) {
      const auto& t = *i;
      if (isIncluded(t, flags)) {
        transfers.push_back(t);
      }
    }

    return true;
  }

  void TransfersContainer::save(std::ostream& os) {
    cryptonote::BinaryOutputStreamSerializer s(os);

    s(const_cast<uint32_t&>(TRANSFERS_CONTAINER_STORAGE_VERSION), "version");

    s(m_currentHeight, "height");
    s(m_transactions, "transactions");
    cryptonote::writeSequence<TransactionOutputInformationEx>(m_transfers.begin(), m_transfers.end(), "transfers", s);
    s(m_spentTransfers, "spentTransfers");
  }

  void TransfersContainer::load(std::istream& in) {
    cryptonote::BinaryInputStreamSerializer s(in);

    uint32_t version = 0;
    s(version, "version");

    if (version > TRANSFERS_CONTAINER_STORAGE_VERSION) {
      throw std::runtime_error("Unsupported transfers storage version");
    }

    s(m_currentHeight, "height");
    s(m_transactions, "transactions");
    cryptonote::readSequence<TransactionOutputInformationEx>(std::back_inserter(m_transfers), "transfers", s);
    s(m_spentTransfers, "spentTransfers");
  }

  bool TransfersContainer::isSpendTimeUnlocked(uint64_t unlockTime) const {
    if (unlockTime < m_currency.maxBlockHeight()) {
      // interpret as block index
      return m_currentHeight - 1 + m_currency.lockedTxAllowedDeltaBlocks() >= unlockTime;
    } else {
      //interpret as time
      uint64_t current_time = static_cast<uint64_t>(time(NULL));
      return current_time + m_currency.lockedTxAllowedDeltaSeconds() >= unlockTime;
    }

    return false;
  }

  bool TransfersContainer::isTransferUnlocked(const TransactionOutputInformationEx& info) const {
    return
      isSpendTimeUnlocked(info.unlockTime) &&
      m_currentHeight > info.blockHeight + m_transactionSpendableAge;
  }

  bool TransfersContainer::isIncluded(const TransactionOutputInformationEx& info, uint32_t flags) const {
    bool unlocked = isSpendTimeUnlocked(info.unlockTime);
    bool softLocked = unlocked && !(m_currentHeight > info.blockHeight + m_transactionSpendableAge);

    return
      // filter by type
      (
        (flags & IncludeTypeKey && info.type == TransactionTypes::OutputType::Key) ||
        (flags & IncludeTypeMultisignature && info.type == TransactionTypes::OutputType::Multisignature)
      ) 
      &&
      // filter by state
      (
        (flags & IncludeStateLocked && !unlocked) ||
        (flags & IncludeStateUnlocked && unlocked) ||
        (flags & IncludeStateSoftLocked && softLocked)
      );
  }

}
