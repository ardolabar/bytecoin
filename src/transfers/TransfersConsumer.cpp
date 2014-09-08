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

#include "TransfersConsumer.h"
#include "CommonTypes.h"

#include "common/BlockingQueue.h"
#include "cryptonote_core/cryptonote_format_utils.h"

#include "INode.h"
#include <future>

#include <iostream>

namespace CryptoNote {

  TransfersConsumer::TransfersConsumer(const cryptonote::Currency& currency, INode& node, const AccountSubscription& sub) :
    m_node(node), m_keys(sub.keys), m_transfers(currency, sub.transactionSpendableAge) { }

  void TransfersConsumer::onBlockchainDetach(uint64_t height) {
    m_transfers.detach(height);
  }

  void TransfersConsumer::onNewBlocks(const CompleteBlock* blocks, uint64_t startHeight, size_t count) {
    
    auto newHeight = startHeight + count;

    struct Tx {
      BlockInfo blockInfo;
      const ITransactionReader* tx;
    };

    size_t workers = std::thread::hardware_concurrency();
    if (workers == 0)
      workers = 2;

    BlockingQueue<Tx> inputQueue(workers*2);

    auto pushingThread = std::async(std::launch::async, [&] {     
      uint64_t height = startHeight;
      BlockInfo blockInfo;
      
      while (count--) {
        const auto& block = blocks->block;
        blockInfo.height = height;
        blockInfo.timestamp = block.timestamp;

        for (const auto& tx : blocks->transactions) {
          Tx item = { blockInfo, tx.get() };
          inputQueue.push(item);
        }
        ++blocks;
      }
      inputQueue.close();
    });

    auto processingFunction = [&] {
      Tx item;
      std::vector<TransactionOutputInformationIn> transfers;

      while (inputQueue.pop(item)) {

        auto ec = processOutputs(*item.tx, transfers);
        uint64_t amountIn = 0;
        uint64_t amountOut = 0;

        if (!transfers.empty()) {
          amountIn = m_transfers.addTransactionOutputs(item.blockInfo, *item.tx, transfers);
          transfers.clear();
        }

        amountOut = m_transfers.addTransactionInputs(item.blockInfo, *item.tx);

        if (amountIn || amountOut) {
          m_observerManager.notify(
            &ITransfersObserver::onTransfer, this, item.tx->getTransactionHash(), amountIn, amountOut);
        }
      }
    };

    std::vector<std::future<void>> processingThreads;
    for (size_t i = 0; i < workers; ++i) {
      processingThreads.push_back(std::async(std::launch::async, processingFunction));
    }

    try {
      for (auto& f : processingThreads) {
        f.get();
      }
    } catch (const std::exception& e) {
      std::cerr << "Exception! " << e.what() << std::endl;
    }

    m_transfers.updateHeight(newHeight);
  }

  AccountAddress TransfersConsumer::getAddress() {
    return m_keys.address;
  }

  ITransfersContainer& TransfersConsumer::getContainer() {
    return m_transfers;
  }

  std::error_code TransfersConsumer::processOutputs(const ITransactionReader& tx, std::vector<TransactionOutputInformationIn>& transfers) {
    std::vector<uint64_t> accountOuts; // outputs to the account
    uint64_t amount = 0;

    tx.findOutputsToAccount(m_keys.address, m_keys.viewSecretKey, accountOuts, amount);
    if (accountOuts.empty())
      return std::error_code();

    std::vector<uint64_t> globalIdxs; // indexes for amounts

    auto txHash = tx.getTransactionHash();
    auto txPubKey = tx.getTransactionPublicKey();

    auto errorCode = getGlobalIndices(reinterpret_cast<const crypto::hash&>(txHash), globalIdxs);

    if (errorCode) {
      return errorCode;
    }

    for (auto idx : accountOuts) {
      auto outType = tx.getOutputType(size_t(idx));

      if (outType != TransactionTypes::OutputType::Key && outType != TransactionTypes::OutputType::Multisignature) {
        continue;
      }

      TransactionOutputInformationIn info;

      info.type = outType;
      info.transactionPublicKey = txPubKey;
      info.outputInTransaction = idx;
      info.globalOutputIndex = globalIdxs[idx];

      if (outType == TransactionTypes::OutputType::Key) {
        TransactionTypes::OutputKey out;
        tx.getOutput(idx, out);

        cryptonote::KeyPair in_ephemeral;
        cryptonote::generate_key_image_helper(
          reinterpret_cast<const cryptonote::account_keys&>(m_keys),
          reinterpret_cast<const crypto::public_key&>(txPubKey),
          idx,
          in_ephemeral,
          reinterpret_cast<crypto::key_image&>(info.keyImage));

        assert(out.key == reinterpret_cast<const PublicKey&>(in_ephemeral.pub));

        info.amount = out.amount;
        info.outputKey = out.key;

      } else if (outType == TransactionTypes::OutputType::Multisignature) {
        TransactionTypes::OutputMultisignature out;
        tx.getOutput(idx, out);

        info.amount = out.amount;
        info.requiredSignatures = out.requiredSignatures;
      }

      transfers.push_back(info);
    }

    return std::error_code();
  }

  std::error_code TransfersConsumer::getGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices) {
    std::promise<std::error_code> errCode;
    std::future<std::error_code> f = errCode.get_future();

    outsGlobalIndices.clear();
    m_node.getTransactionOutsGlobalIndices(transactionHash, outsGlobalIndices, [&errCode](std::error_code ec) { errCode.set_value(ec); });
 
    return f.get();
  }

}

