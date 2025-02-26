/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#pragma once

#include <CLI/CLI.hpp>
#include "fboss/cli/fboss2/options/OutputFormat.h"
#include "fboss/cli/fboss2/options/SSLPolicy.h"
#include "folly/String.h"

namespace facebook::fboss {

class CmdGlobalOptions {
 public:
  CmdGlobalOptions() = default;
  ~CmdGlobalOptions() = default;
  CmdGlobalOptions(const CmdGlobalOptions& other) = delete;
  CmdGlobalOptions& operator=(const CmdGlobalOptions& other) = delete;
  enum FilterOp { LT, GT, LTE, GTE, EQ, NEQ };

  using FilterTerm = std::tuple<std::string, FilterOp, std::string>;
  using IntersectionList = std::vector<FilterTerm>;
  using UnionList = std::vector<IntersectionList>;

  /* These CLi option error codes have been defined to
  avoid using the catchall error code (1).
  These will also aid in proper unit testing.
  */
  enum CliOptionResult {
    EOK = 0,
    KEY_ERROR = 1,
    VALUE_ERROR = 2,
    TYPE_ERROR = 3,
    OP_ERROR = 4,
    EXTRA_OPTIONS = 5,
    TERM_ERROR = 6
  };

  // Static function for getting the CmdGlobalOptions folly::Singleton
  static std::shared_ptr<CmdGlobalOptions> getInstance();

  void init(CLI::App& app);

  class BaseTypeVerifier {
   public:
    virtual ~BaseTypeVerifier() {}
    virtual CliOptionResult verify(std::string& value, std::ostream& out) = 0;
  };

  /* This is being done becase of the following reasons:
    1) switch statement (used in getFilterOp) in c++ doesn't work with strings.
    Hence, we need to find the hash of the string.
    2) The built-in hash function does not return a constexpr which is needed
    here. Hence, we defined a new hash function as below!
  */
  static constexpr unsigned int hash(const char* s, int off = 0) {
    return !s[off] ? 5381 : (hash(s, off + 1) * 33) ^ s[off];
  }

  FilterOp getFilterOp(std::string parsedOp) const {
    switch (hash(parsedOp.c_str())) {
      case hash("=="):
        return FilterOp::EQ;
      case hash("<"):
        return FilterOp::LT;
      case hash("<="):
        return FilterOp::LTE;
      case hash(">"):
        return FilterOp::GT;
      case hash(">="):
        return FilterOp::GTE;
      case hash("!="):
        return FilterOp::NEQ;
      default:
        throw std::invalid_argument("Invalid filter argument passed");
    }
  }

  template <typename ExpectedType>
  class TypeVerifier : public BaseTypeVerifier {
    std::vector<std::string> acceptedFilterValues;
    std::string key;

   public:
    TypeVerifier(
        const std::string& filterKey,
        const std::vector<std::string>& acceptedValues)
        : key(filterKey) {
      for (const auto& value : acceptedValues) {
        acceptedFilterValues.push_back(value);
      }
    }

    explicit TypeVerifier(const std::string& filterKey) : key(filterKey) {}

    CliOptionResult verify(std::string& value, std::ostream& out = std::cerr)
        override {
      auto converted = folly::tryTo<ExpectedType>(value);
      if (converted.hasError()) {
        out << "invalid filter value data type passsed for key " << key
            << std::endl;
        return CliOptionResult::TYPE_ERROR;
      }
      if (!acceptedFilterValues.empty()) {
        if (std::find(
                acceptedFilterValues.begin(),
                acceptedFilterValues.end(),
                value) == acceptedFilterValues.end()) {
          out << "invalid filter value for key " << key << std::endl
              << "accepted values are: { ";
          for (auto& acceptedVal : acceptedFilterValues) {
            out << acceptedVal << " ";
          }
          out << "}";
          return CliOptionResult::VALUE_ERROR;
        }
      }
      return CliOptionResult::EOK;
    }
  };

  CliOptionResult isValid(
      const std::unordered_map<
          std::string_view,
          std::shared_ptr<BaseTypeVerifier>>& validFilters,
      UnionList& filters,
      std::ostream& out = std::cerr) const {
    for (const auto& intersectList : filters) {
      for (const auto& filtTerm : intersectList) {
        auto key = std::get<0>(filtTerm);
        auto val = std::get<2>(filtTerm);
        // invalid filter key
        auto it = validFilters.find(key);
        if (it == validFilters.end()) {
          out << "Invalid filter key passsed " << key << std::endl;
          out << "Filterable fields: { ";
          for (const auto& field : validFilters) {
            out << field.first << " ";
          }
          out << "}" << std::endl;
          return CliOptionResult::KEY_ERROR;
        }
        auto typeVerifyEC = (it->second)->verify(val, out);
        if (typeVerifyEC != CliOptionResult::EOK) {
          return typeVerifyEC;
        }
        // Note that the operator validation is done while parsing
        // hence, we don't perform operator validation here.
      }
    }
    return CliOptionResult::EOK;
  }

  CliOptionResult validateNonFilterOptions() {
    bool hostsSet = !getHosts().empty();
    bool smcSet = !getSmc().empty();
    bool fileSet = !getFile().empty();

    if ((hostsSet && (smcSet || fileSet)) ||
        (smcSet && (hostsSet || fileSet)) ||
        (fileSet && (hostsSet || smcSet))) {
      std::cerr << "only one of host(s), smc or file can be set\n";
      return CliOptionResult::EXTRA_OPTIONS;
    }
    return CliOptionResult::EOK;
  }

  std::vector<std::string> getHosts() const {
    return hosts_;
  }

  std::string getSmc() const {
    return smc_;
  }

  std::string getFile() const {
    return file_;
  }

  std::string getLogLevel() const {
    return logLevel_;
  }

  const SSLPolicy& getSslPolicy() const {
    return sslPolicy_;
  }

  const OutputFormat& getFmt() const {
    return fmt_;
  }

  std::string getLogUsage() const {
    return logUsage_;
  }

  int getAgentThriftPort() const {
    return agentThriftPort_;
  }

  int getQsfpThriftPort() const {
    return qsfpThriftPort_;
  }

  int getBgpThriftPort() const {
    return bgpThriftPort_;
  }

  int getFsdbThriftPort() const {
    return fsdbThriftPort_;
  }

  int getOpenrThriftPort() const {
    return openrThriftPort_;
  }

  int getMkaThriftPort() const {
    return mkaThriftPort_;
  }
  int getCoopThriftPort() const {
    return coopThriftPort_;
  }

  int getRackmonThriftPort() const {
    return rackmonThriftPort_;
  }

  int getSensorServiceThriftPort() const {
    return sensorServiceThriftPort_;
  }

  int getDataCorralServiceThriftPort() const {
    return dataCorralServiceThriftPort_;
  }

  int getBmcHttpPort() const {
    return bmcHttpPort_;
  }

  int getVipInjectorPort() const {
    return vipInjectorThriftPort_;
  }

  std::string getColor() const {
    return color_;
  }

  // Setters for testing purposes
  void setSslPolicy(SSLPolicy& sslPolicy) {
    sslPolicy_ = sslPolicy;
  }

  void setAgentThriftPort(int port) {
    agentThriftPort_ = port;
  }

  void setQsfpThriftPort(int port) {
    qsfpThriftPort_ = port;
  }

  void setBgpThriftPort(int port) {
    bgpThriftPort_ = port;
  }

  void setOpenrThriftPort(int port) {
    openrThriftPort_ = port;
  }

  void setVipThriftPort(int port) {
    vipInjectorThriftPort_ = port;
  }

  void setFsdbThriftPort(int port) {
    fsdbThriftPort_ = port;
  }

  void setFilterInput(std::string& filter) {
    filter_ = filter;
  }

  UnionList getFilters(CliOptionResult& filterParsingEC) const;

 private:
  void initAdditional(CLI::App& app);

  std::vector<std::string> hosts_;
  std::string smc_;
  std::string file_;
  std::string logLevel_{"DBG0"};
  SSLPolicy sslPolicy_{"plaintext"};
  OutputFormat fmt_;
  std::string logUsage_{"scuba"};
  int fsdbThriftPort_{5908};
  int agentThriftPort_{5909};
  int qsfpThriftPort_{5910};
  int bgpThriftPort_{6909};
  int openrThriftPort_{2018};
  int coopThriftPort_{6969};
  int mkaThriftPort_{5920};
  int bmcHttpPort_{8443};
  int rackmonThriftPort_{5973};
  int sensorServiceThriftPort_{5970};
  int dataCorralServiceThriftPort_{5971};
  int vipInjectorThriftPort_{3333};
  std::string color_{"yes"};
  std::string filter_;
};

} // namespace facebook::fboss
