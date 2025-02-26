/*
 *  Copyright (c) 2004-present, Meta Platforms, Inc. and affiliates.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include "fboss/platform/sensor_service/SensorServiceImpl.h"
#include <folly/FileUtil.h>
#include <folly/dynamic.h>
#include <folly/json.h>
#include <folly/logging/xlog.h>
#include <thrift/lib/cpp2/protocol/Serializer.h>
#include <filesystem>
#include "fboss/platform/helpers/Utils.h"
#include "fboss/platform/sensor_service/GetSensorConfig.h"

namespace {

// The following are keys in sensor conf file
const std::string kSourceLmsensor = "lmsensor";
const std::string kSourceSysfs = "sysfs";
const std::string kSourceMock = "mock";
const std::string kSensorFieldName = "name";

const std::string kMockLmsensorJasonData =
    "/etc/sensor_service/sensors_output.json";
const std::string kLmsensorCommand = "sensors -j";
} // namespace
namespace facebook::fboss::platform::sensor_service {
using namespace facebook::fboss::platform::helpers;

void SensorServiceImpl::init() {
  std::string sensorConfJson;
  // Check if conf file name is set, if not, set the default name
  if (confFileName_.empty()) {
    sensorConfJson = getPlatformConfig();
  } else if (!folly::readFile(confFileName_.c_str(), sensorConfJson)) {
    throw std::runtime_error(
        "Can not find sensor config file: " + confFileName_);
  }

  // Clear everything before init
  sensorNameMap_.clear();
  sensorTable_.sensorMapList()->clear();

  // folly::dynamic sensorConf;

  apache::thrift::SimpleJSONSerializer::deserialize<SensorConfig>(
      sensorConfJson, sensorTable_);

  XLOG(INFO) << apache::thrift::SimpleJSONSerializer::serialize<std::string>(
      sensorTable_);

  if (sensorTable_.source() == kSourceMock) {
    sensorSource_ = SensorSource::MOCK;
  } else if (sensorTable_.source() == kSourceLmsensor) {
    sensorSource_ = SensorSource::LMSENSOR;
  } else if (sensorTable_.source() == kSourceSysfs) {
    sensorSource_ = SensorSource::SYSFS;
  } else {
    throw std::runtime_error(folly::to<std::string>(
        "Invalid source in ", confFileName_, " : ", *sensorTable_.source()));
  }

  liveDataTable_.withWLock([&](auto& table) {
    for (auto& sensor : *sensorTable_.sensorMapList()) {
      for (auto& sensorIter : sensor.second) {
        // Check if file exists, if not, check if the path is regex pattern
        std::string path = *sensorIter.second.path();
        if (std::filesystem::exists(std::filesystem::path(path))) {
          table[sensorIter.first].path = path;
          sensorNameMap_[path] = sensorIter.first;
        } else {
          std::string realPath = findFileFromRegex(path);
          if (!realPath.empty()) {
            table[sensorIter.first].path = realPath;
            sensorNameMap_[realPath] = sensorIter.first;
          }
        }

        table[sensorIter.first].fru = sensor.first;
        if (sensorIter.second.compute().has_value()) {
          table[sensorIter.first].compute = *sensorIter.second.compute();
        }
        table[sensorIter.first].thresholds = *sensorIter.second.thresholdMap();

        XLOG(INFO) << sensorIter.first
                   << "; path = " << table[sensorIter.first].path
                   << "; compute = " << table[sensorIter.first].compute
                   << "; fru = " << table[sensorIter.first].fru;
      }
    }
  });

  XLOG(INFO) << "========================================================";
}

std::optional<SensorData> SensorServiceImpl::getSensorData(
    const std::string& sensorName) {
  SensorData d;
  d.name() = "";

  liveDataTable_.withRLock([&](auto& table) {
    auto it = table.find(sensorName);

    if (it != table.end()) {
      d.name() = it->first;
      d.value() = it->second.value;
      d.timeStamp() = it->second.timeStamp;
    }
  });

  return *d.name() == "" ? std::nullopt : std::optional<SensorData>{d};
}

std::vector<SensorData> SensorServiceImpl::getSensorsData(
    const std::vector<std::string>& sensorNames) {
  std::vector<SensorData> sensorDataVec;

  liveDataTable_.withRLock([&](auto& table) {
    for (auto& pair : table) {
      if (std::find(sensorNames.begin(), sensorNames.end(), pair.first) !=
          sensorNames.end()) {
        SensorData d;
        d.name() = pair.first;
        d.value() = pair.second.value;
        d.timeStamp() = pair.second.timeStamp;
        sensorDataVec.push_back(d);
      }
    }
  });
  return sensorDataVec;
}

std::vector<SensorData> SensorServiceImpl::getAllSensorData() {
  std::vector<SensorData> sensorDataVec;

  liveDataTable_.withRLock([&](auto& table) {
    for (auto& pair : table) {
      SensorData d;
      d.name() = pair.first;
      d.value() = pair.second.value;
      d.timeStamp() = pair.second.timeStamp;
      sensorDataVec.push_back(d);
    }
  });
  return sensorDataVec;
}

void SensorServiceImpl::fetchSensorData() {
  if (sensorSource_ == SensorSource::LMSENSOR) {
    int retVal = 0;
    std::string ret = execCommandUnchecked(kLmsensorCommand, retVal);

    if (retVal != 0) {
      throw std::runtime_error("Run " + kLmsensorCommand + " failed!");
    }

    parseSensorJsonData(ret);

  } else if (sensorSource_ == SensorSource::SYSFS) {
    // ToDo
    // Get sensor value via read from path (key of sensorTable_)
    getSensorDataFromPath();
  } else if (sensorSource_ == SensorSource::MOCK) {
    std::string sensorDataJson;
    if (folly::readFile(kMockLmsensorJasonData.c_str(), sensorDataJson)) {
      parseSensorJsonData(sensorDataJson);
    } else {
      throw std::runtime_error(
          "Can not find sensor data json file: " + kMockLmsensorJasonData);
    }
  } else {
    throw std::runtime_error(
        "Unknow Sensor Source selected : " +
        folly::to<std::string>(static_cast<int>(sensorSource_)));
  }
}

void SensorServiceImpl::getSensorDataFromPath() {
  auto dataTable = liveDataTable_.wlock();

  auto now = helpers::nowInSecs();
  for (const auto& [name, livedata] : *dataTable) {
    std::string sensorInput;

    if (folly::readFile(livedata.path.c_str(), sensorInput)) {
      (*dataTable)[name].value = folly::to<float>(sensorInput);
      (*dataTable)[name].timeStamp = now;
      if (livedata.compute != "") {
        (*dataTable)[name].value =
            computeExpression(livedata.compute, (*dataTable)[name].value);
      }
      XLOG(INFO) << name << "(" << livedata.path << ")"
                 << " : " << (*dataTable)[name].value;
    } else {
      XLOG(INFO) << "Can not read data for " << name << " from "
                 << livedata.path;
    }
  }
}

void SensorServiceImpl::parseSensorJsonData(const std::string& strJson) {
  folly::dynamic sensorJson = folly::parseJson(strJson);

  auto dataTable = liveDataTable_.wlock();

  auto now = helpers::nowInSecs();
  for (auto& firstPair : sensorJson.items()) {
    // Key is pair.first, value is pair.second
    if (firstPair.second.isObject()) {
      for (auto& secondPair : firstPair.second.items()) {
        std::string sensorPath = folly::to<std::string>(
            firstPair.first.asString(), ":", secondPair.first.asString());
        // Only check sensor data that the name is in the configuration file
        if (secondPair.second.isObject() &&
            (sensorNameMap_.count(sensorPath) != 0)) {
          // Get value only for now
          for (auto& thirdPair : secondPair.second.items()) {
            if (thirdPair.first.asString().find("_input") !=
                std::string::npos) {
              (*dataTable)[sensorNameMap_[sensorPath]].value =
                  folly::to<float>(thirdPair.second.asString());
              (*dataTable)[sensorNameMap_[sensorPath]].timeStamp = now;

              XLOG(INFO) << sensorNameMap_[sensorPath] << " : "
                         << (*dataTable)[sensorNameMap_[sensorPath]].value
                         << " >>>> "
                         << (*dataTable)[sensorNameMap_[sensorPath]].timeStamp;
            }
          }
        }
      }
    }
  }
}

} // namespace facebook::fboss::platform::sensor_service
