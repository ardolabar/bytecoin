// Copyright (c) 2012-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "ISerializer.h"

#include <string>
#include <vector>
#include <unordered_map>

namespace cryptonote {

void serializeVarint(uint64_t& value, const std::string& name, cryptonote::ISerializer& serializer);
void serializeVarint(uint32_t& value, const std::string& name, cryptonote::ISerializer& serializer);

template<typename T>
void serialize(std::vector<T>& value, const std::string& name, cryptonote::ISerializer& serializer) {
  std::size_t size = value.size();
  serializer.beginArray(size, name);
  value.resize(size);

  for (size_t i = 0; i < size; ++i) {
    serializer(value[i], "");
  }

  serializer.endArray();
}

template<typename K, typename V, typename Hash>
void serialize(std::unordered_map<K, V, Hash>& value, const std::string& name, cryptonote::ISerializer& serializer) {
  std::size_t size;
  size = value.size();

  serializer.beginArray(size, name);

  if (serializer.type() == cryptonote::ISerializer::INPUT) {
    value.reserve(size);

    for (size_t i = 0; i < size; ++i) {
      K key;
      V v;
      serializer.beginObject("");
      serializer(key, "");
      serializer(v, "");
      serializer.endObject();

      value[key] = v;
    }
  } else {
    for (auto kv: value) {
      K key;
      key = kv.first;
      serializer.beginObject("");
      serializer(key, "");
      serializer(kv.second, "");
      serializer.endObject();
    }
  }

  serializer.endArray();
}

}
