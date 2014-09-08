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

#include "IBlockchainSynchronizer.h"
#include "ITransfersSynchronizer.h"
#include "TransfersContainer.h"

#include "crypto/crypto.h"

#include "IObservableImpl.h"

namespace CryptoNote {

  class INode;
 
  class TransfersConsumer : 
    public IBlockchainConsumer, 
    public IObservableImpl<ITransfersObserver, ITransfersSubscription> {

  public:

    TransfersConsumer(const cryptonote::Currency& currency, INode& node, const AccountSubscription& sub);

    // IBlockchainConsumer
    virtual void onBlockchainDetach(uint64_t height) override;
    virtual void onNewBlocks(const CompleteBlock* blocks, uint64_t startHeight, size_t count) override;

    // ITransfersSubscription
    virtual AccountAddress getAddress() override;
    virtual ITransfersContainer& getContainer() override;

  private:

    std::error_code processOutputs(const ITransactionReader& tx, std::vector<TransactionOutputInformationIn>& transfers);

    std::error_code getGlobalIndices(const crypto::hash& transactionHash, std::vector<uint64_t>& outsGlobalIndices);

    AccountKeys m_keys;
    TransfersContainer m_transfers;
    INode& m_node;
  };

}