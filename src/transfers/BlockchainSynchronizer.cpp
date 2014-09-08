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

#include "BlockchainSynchronizer.h"
#include "cryptonote_core/TransactionApi.h"
#include "cryptonote_core/cryptonote_format_utils.h"
#include <functional>

namespace {
  inline std::vector<uint8_t> stringToVector(const std::string& s) {
    std::vector<uint8_t> vec(
      reinterpret_cast<const uint8_t*>(s.data()),
      reinterpret_cast<const uint8_t*>(s.data()) + s.size());
    return vec;
  }
}

namespace CryptoNote {

  BlockchainSynchronizer::BlockchainSynchronizer(INode& node, const crypto::hash& genesisBlockHash) :
    m_node(node), m_genesisBlockHash(genesisBlockHash), m_syncInProgress(false) {
  }

  void BlockchainSynchronizer::addConsumer(IBlockchainConsumer* consumer) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_consumers.insert(std::make_pair(consumer, std::make_shared<SynchronizationState>(m_genesisBlockHash)));
  }

  bool BlockchainSynchronizer::removeConsumer(IBlockchainConsumer* consumer) {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_consumers.erase(consumer) > 0;
  }

  IStreamSerializable* BlockchainSynchronizer::getConsumerState(IBlockchainConsumer* consumer) {
    std::lock_guard<std::mutex> lk(m_mutex);
    
    auto it = m_consumers.find(consumer);
    if (it == m_consumers.end())
      return nullptr;

    return it->second.get();
  }

  void BlockchainSynchronizer::start() {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_node.addObserver(this);
    startSync();
  }

  void BlockchainSynchronizer::stop() {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (m_syncInProgress) {
      m_syncFinish.wait(lk);
    }

    m_node.removeObserver(this);
  }

  void BlockchainSynchronizer::save(std::ostream& os) {
    os.write(m_genesisBlockHash.data, sizeof(m_genesisBlockHash.data));
  }

  void BlockchainSynchronizer::load(std::istream& in) {
    crypto::hash genesisBlockHash;
    in.read(genesisBlockHash.data, sizeof(genesisBlockHash.data));
    if (genesisBlockHash != m_genesisBlockHash) {
      throw std::runtime_error("Genesis block hash does not match stored state");
    }
  }

  void BlockchainSynchronizer::startSync() {
    if (m_syncInProgress)
      return;
    m_syncInProgress = true;
    requestNextBlocks(std::make_shared<GetBlocksResponse>());
  }

  void BlockchainSynchronizer::requestNextBlocks(std::shared_ptr<GetBlocksResponse> response) {
    auto history = getCommonHistory();
    if (!history.empty()) {
      m_node.getNewBlocks(std::move(history), response->newBlocks, response->startHeight,
        std::bind(&BlockchainSynchronizer::onGetBlocksCompleted, this, response, std::placeholders::_1));
    } else {
      m_syncInProgress = false;
    }
  }

  std::list<crypto::hash> BlockchainSynchronizer::getCommonHistory() {
    SynchronizationState::ShortHistory commonHistory;

    auto shortest = std::min_element(m_consumers.begin(), m_consumers.end(), 
      [](const ConsumersMap::value_type& v1, const ConsumersMap::value_type& v2) { 
        return v1.second->getHeight() < v2.second->getHeight();
      });

    if (shortest != m_consumers.end()) {
      commonHistory = shortest->second->getShortHistory();
    }

    return commonHistory;
  }

  void BlockchainSynchronizer::lastKnownBlockHeightUpdated(uint64_t height) {
    std::lock_guard<std::mutex> lk(m_mutex);
    startSync();
  }

  void BlockchainSynchronizer::onGetBlocksCompleted(std::shared_ptr<GetBlocksResponse> response, std::error_code ec) {
    if (ec) {
      m_observerManager.notify(
        &IBlockchainSynchronizerObserver::synchronizationProgressUpdated, 
        response->startHeight, // FIXME
        m_node.getLastLocalBlockHeight(), 
        ec);
      return;
    }

    // process in separate thread, unlock callback thread
    m_blockProcessing = std::async(std::launch::async, [this, response] { processBlocks(response); });
  }

  void BlockchainSynchronizer::processBlocks(std::shared_ptr<GetBlocksResponse> response) {
    
    auto newHeight = response->startHeight + response->newBlocks.size();
    BlockchainInterval interval;
    interval.startHeight = response->startHeight;

    std::vector<CompleteBlock> blocks;

    // parse blocks
    for (const auto& block : response->newBlocks) {
      CompleteBlock completeBlock;
      if (!cryptonote::parse_and_validate_block_from_blob(block.block, completeBlock.block))
        return; // TODO: handle error
      cryptonote::get_block_hash(completeBlock.block, completeBlock.blockHash);
      interval.blocks.push_back(completeBlock.blockHash);

      completeBlock.transactions.push_back(createTransaction(completeBlock.block.minerTx));

      for (const auto& txblob : block.txs) {
        completeBlock.transactions.push_back(createTransaction(stringToVector(txblob)));
      }

      blocks.push_back(std::move(completeBlock));
    }

    response->newBlocks.clear();

    bool blocksAdded = updateConsumers(interval, blocks);

    m_observerManager.notify(
      &IBlockchainSynchronizerObserver::synchronizationProgressUpdated,
      newHeight,
      m_node.getLastLocalBlockHeight(),
      std::error_code());

    std::unique_lock<std::mutex> lk(m_mutex);

    if (blocksAdded || m_node.getLastLocalBlockHeight() > newHeight) {
      // continue while we still have smth to process
      requestNextBlocks(response);
    } else {
      // Synchronization completed
      m_syncInProgress = false;
      m_syncFinish.notify_all();
    }
  }

  bool BlockchainSynchronizer::updateConsumers(const BlockchainInterval& interval, const std::vector<CompleteBlock>& blocks) {
    std::unique_lock<std::mutex> lk(m_mutex);
    ConsumersMap consumers(m_consumers); // copy to local var
    lk.unlock();

    bool blocksAdded = false;

    for (auto& kv : consumers) {
      auto result = kv.second->checkInterval(interval);
      if (result.detachRequired) {
        kv.first->onBlockchainDetach(result.detachHeight);
        kv.second->detach(result.detachHeight);
      }
      if (result.hasNewBlocks) {
        // update consumer
        kv.first->onNewBlocks(
          blocks.data() + result.newBlockHeight - interval.startHeight,
          result.newBlockHeight,
          blocks.size() - (result.newBlockHeight - interval.startHeight));
        // update state
        kv.second->addBlocks(
          interval.blocks.data() + result.newBlockHeight - interval.startHeight,
          result.newBlockHeight,
          interval.blocks.size() - (result.newBlockHeight - interval.startHeight));
        blocksAdded = true;
      }
    }
    return blocksAdded;
  }
}
