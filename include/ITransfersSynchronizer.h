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
#include "ITransfersContainer.h"
#include "IStreamSerializable.h"

namespace CryptoNote {

  struct AccountSubscription {
    AccountKeys keys;
    uint64_t accountCreationTime;
    size_t transactionSpendableAge;
  };

  class ITransfersSubscription;

  class ITransfersObserver {
  public:
    virtual void onTransfer(ITransfersSubscription* object, 
      const Hash& transactionHash, uint64_t amountIn, uint64_t amountOut) {}
  };

  class ITransfersSubscription : public IObservable<ITransfersObserver> {
  public:
    virtual AccountAddress getAddress() = 0;
    virtual ITransfersContainer& getContainer() = 0;
  };

  class ITransfersSynchronizer: public IStreamSerializable {
  public:
    virtual ~ITransfersSynchronizer() {}

    virtual ITransfersSubscription& addSubscription(const AccountSubscription& acc) = 0;
    virtual bool removeSubscription(const AccountAddress& acc) = 0;
    virtual void getSubscriptions(std::vector<AccountAddress>& subscriptions) = 0;
    // returns nullptr if not address is not found
    virtual ITransfersSubscription* getSubscription(const AccountAddress& acc) = 0;
  };

}
