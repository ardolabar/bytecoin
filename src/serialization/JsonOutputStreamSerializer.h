// Copyright (c) 2012-2014 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include "serialization/ISerializer.h"
#include "serialization/JsonValue.h"

#include <iostream>

namespace cryptonote {

class JsonOutputStreamSerializer : public ISerializer {
public:
  JsonOutputStreamSerializer();
  virtual ~JsonOutputStreamSerializer();

  JsonValue getJsonValue() const;
  SerializerType type() const;

  virtual ISerializer& beginObject(const std::string& name) override;
  virtual ISerializer& endObject() override;

  virtual ISerializer& beginArray(std::size_t& size, const std::string& name) override;
  virtual ISerializer& endArray() override;

  virtual ISerializer& operator()(int32_t& value, const std::string& name) override;
  virtual ISerializer& operator()(uint32_t& value, const std::string& name) override;
  virtual ISerializer& operator()(int64_t& value, const std::string& name) override;
  virtual ISerializer& operator()(uint64_t& value, const std::string& name) override;
  virtual ISerializer& operator()(double& value, const std::string& name) override;
  virtual ISerializer& operator()(std::string& value, const std::string& name) override;
  virtual ISerializer& operator()(uint8_t& value, const std::string& name) override;
  virtual ISerializer& operator()(bool& value, const std::string& name) override;
  virtual ISerializer& operator()(char* value, std::size_t size, const std::string& name) override;

  virtual ISerializer& tag(const std::string& name) override;
  virtual ISerializer& untagged(uint8_t& value) override;
  virtual ISerializer& endTag() override;

  virtual bool hasObject(const std::string& name) override;

  friend std::ostream& operator<<(std::ostream& out, const JsonOutputStreamSerializer& enumerator);

private:
  JsonValue root;
  std::vector<JsonValue*> chain;
};

} // namespace cryptonote
