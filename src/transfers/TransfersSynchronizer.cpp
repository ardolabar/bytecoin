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

#include "TransfersSynchronizer.h"
#include "TransfersConsumer.h"

#include "serialization/BinaryInputStreamSerializer.h"
#include "serialization/BinaryOutputStreamSerializer.h"

namespace std {

  inline bool operator < (const CryptoNote::AccountAddress& a, const CryptoNote::AccountAddress& b) {
    return std::make_pair(a.spendPublicKey, a.spendPublicKey) < std::make_pair(b.spendPublicKey, b.spendPublicKey);
  }
}

namespace CryptoNote {

  void serialize(AccountAddress& acc, const std::string& name, cryptonote::ISerializer& s) {
    s.beginObject(name);
    s(acc.spendPublicKey, "spendKey");
    s(acc.viewPublicKey, "viewKey");
    s.endObject();
  }

  const uint32_t TRANSFERS_STORAGE_ARCHIVE_VERSION = 0;

  TransfersSyncronizer::TransfersSyncronizer(const cryptonote::Currency& currency, IBlockchainSynchronizer& sync, INode& node) :
    m_currency(currency), m_sync(sync), m_node(node) {
  }

  TransfersSyncronizer::~TransfersSyncronizer() {
  }

  ITransfersSubscription& TransfersSyncronizer::addSubscription(const AccountSubscription& acc) {

    auto it = m_subscriptions.find(acc.keys.address);
    if (it != m_subscriptions.end())
      return *it->second;

    std::unique_ptr<TransfersConsumer> consumer(
      new TransfersConsumer(m_currency, m_node, acc));
    
    auto consumerPtr = consumer.get();
    auto res = m_subscriptions.insert(std::make_pair(acc.keys.address, std::move(consumer)));
    assert(res.second);

    m_sync.addConsumer(consumerPtr);
    return *consumerPtr;
  }

  bool TransfersSyncronizer::removeSubscription(const AccountAddress& acc) {
    auto it = m_subscriptions.find(acc);
    if (it == m_subscriptions.end())
      return false;
    m_sync.removeConsumer(it->second.get());
    m_subscriptions.erase(it);
    return true;
  }

  void TransfersSyncronizer::getSubscriptions(std::vector<AccountAddress>& subscriptions) {
    for (const auto& kv : m_subscriptions) {
      subscriptions.push_back(kv.first);
    }
  }

  ITransfersSubscription* TransfersSyncronizer::getSubscription(const AccountAddress& acc) {
    auto it = m_subscriptions.find(acc);
    return (it == m_subscriptions.end()) ? 0 : it->second.get();
  }

  void TransfersSyncronizer::save(std::ostream& os) {
    m_sync.save(os);

    cryptonote::BinaryOutputStreamSerializer s(os);
    s(const_cast<uint32_t&>(TRANSFERS_STORAGE_ARCHIVE_VERSION), "version");

    size_t subscriptionCount = m_subscriptions.size();

    s.beginArray(subscriptionCount, "subscriptions");

    for (const auto& sub : m_subscriptions) {
      s.beginObject("");
      s(const_cast<AccountAddress&>(sub.first), "account");

      std::stringstream stateStream;
      // synchronization state
      m_sync.getConsumerState(sub.second.get())->save(stateStream);
      // transfers container
      sub.second->getContainer().save(stateStream);
      
      // store data block
      std::string blob = stateStream.str();
      s(blob, "state");
      s.endObject();
    }
  }

  void TransfersSyncronizer::load(std::istream& is) {
    m_sync.load(is);

    cryptonote::BinaryInputStreamSerializer s(is);
    uint32_t version = 0;

    s(version, "version");

    if (version > TRANSFERS_STORAGE_ARCHIVE_VERSION) {
      throw std::runtime_error("");
    }
    
    size_t subscriptionCount = 0;
    s.beginArray(subscriptionCount, "subscriptions");

    while (subscriptionCount--) {
      s.beginObject("");
      AccountAddress addr;
      s(addr, "account");

      std::string blob;
      s(blob, "state");

      auto subIter = m_subscriptions.find(addr);
      if (subIter != m_subscriptions.end()) {
        std::stringstream stateStream(blob);
        auto consumerState = m_sync.getConsumerState(subIter->second.get());
        assert(consumerState);
        // load consumer state
        consumerState->load(stateStream);
        // load container
        subIter->second->getContainer().load(stateStream);
      }
      s.endObject();
    }

    s.endArray();
  }
}
