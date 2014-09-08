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

#include "INode.h"
#include "SynchronizationState.h"
#include "IBlockchainSynchronizer.h"
#include "IObservableImpl.h"
#include "IStreamSerializable.h"

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <future>

namespace CryptoNote {

  class BlockchainSynchronizer: 
    public IObservableImpl<IBlockchainSynchronizerObserver, IBlockchainSynchronizer>,
    public INodeObserver {
  public:

    BlockchainSynchronizer(INode& node, const crypto::hash& genesisBlockHash);

    // IBlockchainSynchronizer
    virtual void addConsumer(IBlockchainConsumer* consumer) override;
    virtual bool removeConsumer(IBlockchainConsumer* consumer) override;
    virtual IStreamSerializable* getConsumerState(IBlockchainConsumer* consumer) override;

    virtual void start() override;
    virtual void stop() override;

    // IStreamSerializable
    virtual void save(std::ostream& os) override;
    virtual void load(std::istream& in) override;

    struct GetBlocksResponse {
      uint64_t startHeight;
      std::list<cryptonote::block_complete_entry> newBlocks;
    };

  private:

    // INodeObserver
    virtual void lastKnownBlockHeightUpdated(uint64_t height);

    void startSync();
    void requestNextBlocks(std::shared_ptr<GetBlocksResponse> response);
    void onGetBlocksCompleted(std::shared_ptr<GetBlocksResponse> response, std::error_code ec);
    void processBlocks(std::shared_ptr<GetBlocksResponse> response);
    bool updateConsumers(const BlockchainInterval& interval, const std::vector<CompleteBlock>& blocks);

    std::list<crypto::hash> getCommonHistory();

    typedef std::map<IBlockchainConsumer*, std::shared_ptr<SynchronizationState>> ConsumersMap;

    ConsumersMap m_consumers;
    INode& m_node;
    const crypto::hash m_genesisBlockHash;

    std::future<void> m_blockProcessing;
    bool m_syncInProgress;
    std::condition_variable m_syncFinish;
    std::mutex m_mutex;
  };


}