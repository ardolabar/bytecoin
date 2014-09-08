// Copyright (c) 2012-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "serialization/JsonOutputStreamSerializer.h"

#include <cassert>
#include <stdexcept>

namespace cryptonote {

JsonOutputStreamSerializer::JsonOutputStreamSerializer() : root(JsonValue::OBJECT) {
}

JsonOutputStreamSerializer::~JsonOutputStreamSerializer() {
}

std::ostream& operator<<(std::ostream& out, const JsonOutputStreamSerializer& enumerator) {
  out << enumerator.root;
  return out;
}

JsonValue JsonOutputStreamSerializer::getJsonValue() const {
  return root;
}

ISerializer::SerializerType JsonOutputStreamSerializer::type() const {
  return ISerializer::OUTPUT;
}

ISerializer& JsonOutputStreamSerializer::beginObject(const std::string& name) {
  if (chain.size() == 0) {
    chain.push_back(&root);
    return *this;
  }

  JsonValue* parent = chain.back();
  JsonValue obj(JsonValue::OBJECT);

  if (parent->isObject()) {
    JsonValue& res = parent->insert(name, obj);
    chain.push_back(&res);
  } else {
    JsonValue& res = parent->pushBack(obj);
    chain.push_back(&res);
  }

  return *this;
}

ISerializer& JsonOutputStreamSerializer::endObject() {
  chain.pop_back();
  return *this;
}

ISerializer& JsonOutputStreamSerializer::beginArray(std::size_t& size, const std::string& name) {
  JsonValue val(JsonValue::ARRAY);
  JsonValue& res = chain.back()->insert(name, val);
  chain.push_back(&res);
  return *this;
}

ISerializer& JsonOutputStreamSerializer::endArray() {
  chain.pop_back();
  return *this;
}

ISerializer& JsonOutputStreamSerializer::operator()(uint64_t& value, const std::string& name) {
  int64_t v = static_cast<int64_t>(value);
  return operator()(v, name);
}

ISerializer& JsonOutputStreamSerializer::operator()(uint32_t& value, const std::string& name) {
  uint64_t v = static_cast<uint64_t>(value);
  return operator()(v, name);
}

ISerializer& JsonOutputStreamSerializer::operator()(int32_t& value, const std::string& name) {
  int64_t v = static_cast<int64_t>(value);
  return operator()(v, name);
}

ISerializer& JsonOutputStreamSerializer::operator()(int64_t& value, const std::string& name) {
  JsonValue* val = chain.back();
  JsonValue v;
  v = static_cast<int64_t>(value);
  if (val->isArray()) {
    val->pushBack(v);
  } else {
    val->insert(name, v);
  }
  return *this;
}

ISerializer& JsonOutputStreamSerializer::operator()(double& value, const std::string& name) {
  JsonValue* val = chain.back();
  JsonValue v;
  v = static_cast<double>(value);

  if (val->isArray()) {
    val->pushBack(v);
  } else {
    val->insert(name, v);
  }
  return *this;
}

ISerializer& JsonOutputStreamSerializer::operator()(std::string& value, const std::string& name) {
  JsonValue* val = chain.back();
  JsonValue v;
  v = value;

  if (val->isArray()) {
    val->pushBack(v);
  } else {
    val->insert(name, v);
  }
  return *this;
}

ISerializer& JsonOutputStreamSerializer::operator()(uint8_t& value, const std::string& name) {
  uint64_t v = static_cast<uint64_t>(value);
  return operator()(v, name);
}

ISerializer& JsonOutputStreamSerializer::operator()(bool& value, const std::string& name) {
  JsonValue* val = chain.back();
  JsonValue v;
  v = static_cast<bool>(value);
  if (val->isArray()) {
    val->pushBack(v);
  } else {
    val->insert(name, v);
  }

  return *this;
}

ISerializer& JsonOutputStreamSerializer::operator()(char* value, std::size_t size, const std::string& name) {
  assert(false);
  throw std::runtime_error("JsonOutputStreamSerializer doesn't support \"char *\" type of serialization");

  return *this;
}

ISerializer& JsonOutputStreamSerializer::tag(const std::string& name) {
  assert(false);
  throw std::runtime_error("JsonOutputStreamSerializer doesn't support this type of serialization");

  return *this;
}

ISerializer& JsonOutputStreamSerializer::untagged(uint8_t& value) {
  assert(false);
  throw std::runtime_error("JsonOutputStreamSerializer doesn't support this type of serialization");

  return *this;
}

ISerializer& JsonOutputStreamSerializer::endTag() {
  assert(false);
  throw std::runtime_error("JsonOutputStreamSerializer doesn't support this type of serialization");

  return *this;
}

bool JsonOutputStreamSerializer::hasObject(const std::string& name) {
  assert(false);
  throw std::runtime_error("JsonOutputStreamSerializer doesn't support this type of serialization");

  return false;
}

}


