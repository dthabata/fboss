/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/PlatformPort.h"

#include "fboss/agent/FbossError.h"
#include "fboss/agent/Platform.h"
#include "fboss/lib/config/PlatformConfigUtils.h"

#include <folly/logging/xlog.h>
#include <re2/re2.h>
#include <thrift/lib/cpp/util/EnumUtils.h>

namespace facebook::fboss {

PlatformPort::PlatformPort(PortID id, Platform* platform)
    : id_(id), platform_(platform) {
  const auto& tcvrList = getTransceiverLanes();
  // If the platform port comes with transceiver lanes
  if (!tcvrList.empty()) {
    // All the transceiver lanes should use the same transceiver id
    auto chipCfg = getPlatform()->getDataPlanePhyChip(*tcvrList[0].chip());
    if (!chipCfg) {
      throw FbossError(
          "Port ",
          getPortID(),
          " is using platform unsupported chip ",
          *tcvrList[0].chip());
    }
    transceiverID_.emplace(TransceiverID(*chipCfg->physicalID()));
  }
}

std::ostream& operator<<(std::ostream& os, PortLedExternalState lfs) {
  switch (lfs) {
    case PortLedExternalState::NONE:
      os << "None";
      break;
    case PortLedExternalState::CABLING_ERROR:
      os << "Cabling Error";
      break;
    case PortLedExternalState::EXTERNAL_FORCE_ON:
      os << "Turned ON externally by a Thrift call";
      break;
    case PortLedExternalState::EXTERNAL_FORCE_OFF:
      os << "Turned OFF externally by a Thrift call";
      break;
  }
  return os;
}

const cfg::PlatformPortEntry& PlatformPort::getPlatformPortEntry() const {
  const auto& platformPorts = platform_->getPlatformPorts();
  if (auto itPlatformPort = platformPorts.find(id_);
      itPlatformPort != platformPorts.end()) {
    return itPlatformPort->second;
  }
  throw FbossError("Can't find PlatformPortEntry for port=", id_);
}

std::optional<std::vector<phy::PinConfig>>
PlatformPort::getTransceiverPinConfigs(cfg::PortProfileID profileID) const {
  return platform_->getPlatformMapping()->getPortTransceiverPinConfigs(
      PlatformPortProfileConfigMatcher(profileID, id_));
}

phy::PortPinConfig PlatformPort::getPortXphyPinConfig(
    cfg::PortProfileID profileID) const {
  if (platform_->needTransceiverInfo()) {
    folly::EventBase evb;
    auto transceiverInfo = getTransceiverInfo(&evb);
    if (transceiverInfo) {
      return platform_->getPlatformMapping()->getPortXphyPinConfig(
          PlatformPortProfileConfigMatcher(
              profileID,
              id_,
              buildPlatformPortConfigOverrideFactor(*transceiverInfo)));
    }
  }
  return platform_->getPlatformMapping()->getPortXphyPinConfig(
      PlatformPortProfileConfigMatcher(profileID, id_));
}

phy::PortPinConfig PlatformPort::getPortPinConfigs(
    cfg::PortProfileID profileID) const {
  // Now iphy pin configs can read it from sw port, will eventually deprecate
  // this function in the future
  auto pinConfig = getPortXphyPinConfig(profileID);
  if (auto transceiverPins = getTransceiverPinConfigs(profileID)) {
    pinConfig.transceiver() = *transceiverPins;
  }
  return pinConfig;
}

std::map<std::string, phy::DataPlanePhyChip>
PlatformPort::getPortDataplaneChips(cfg::PortProfileID profileID) const {
  auto pins = getPortPinConfigs(profileID);
  auto allChips = platform_->getPlatformMapping()->getChips();
  std::map<std::string, phy::DataPlanePhyChip> chips;

  auto addChips = [&allChips, &chips](const auto& pins) {
    for (auto& pin : pins) {
      auto chip = *pin.id()->chip();
      chips[chip] = allChips[chip];
    }
  };

  addChips(*pins.iphy());
  if (auto xphySys = pins.xphySys()) {
    addChips(*xphySys);
  }
  if (auto xphyLine = pins.xphyLine()) {
    addChips(*xphyLine);
  }
  if (auto transceiver = pins.transceiver()) {
    addChips(*transceiver);
  }

  return chips;
}

cfg::PortProfileID PlatformPort::getProfileIDBySpeed(
    cfg::PortSpeed speed) const {
  auto profile = getProfileIDBySpeedIf(speed);
  if (!profile.has_value()) {
    throw FbossError(
        "Platform port ",
        getPortID(),
        " has no profile for speed ",
        apache::thrift::util::enumNameSafe(speed));
  }
  return profile.value();
}

std::optional<cfg::PortProfileID> PlatformPort::getProfileIDBySpeedIf(
    cfg::PortSpeed speed) const {
  if (speed == cfg::PortSpeed::DEFAULT) {
    return cfg::PortProfileID::PROFILE_DEFAULT;
  }

  const auto& platformPortEntry = getPlatformPortEntry();
  for (auto profile : *platformPortEntry.supportedProfiles()) {
    auto profileID = profile.first;
    if (auto profileCfg = platform_->getPortProfileConfig(
            PlatformPortProfileConfigMatcher(profileID, getPortID()))) {
      if (*profileCfg->speed() == speed) {
        return profileID;
      }
    } else {
      throw FbossError(
          "Platform port ",
          getPortID(),
          " has invalid profile ",
          apache::thrift::util::enumNameSafe(profileID));
    }
  }
  XLOG(WARN) << "Can't find supported profile for port=" << getPortID()
             << ", speed=" << apache::thrift::util::enumNameSafe(speed);
  return std::nullopt;
}

const phy::PortProfileConfig PlatformPort::getPortProfileConfig(
    cfg::PortProfileID profileID) const {
  auto profile = getPortProfileConfigIf(profileID);
  if (!profile.has_value()) {
    throw FbossError(
        "No port profile with id ",
        apache::thrift::util::enumNameSafe(profileID),
        " found in PlatformConfig for port ",
        id_);
  }
  return profile.value();
}

const std::optional<phy::PortProfileConfig>
PlatformPort::getPortProfileConfigIf(cfg::PortProfileID profileID) const {
  if (platform_->needTransceiverInfo()) {
    folly::EventBase evb;
    std::optional<TransceiverInfo> transceiverInfo = getTransceiverInfo(&evb);
    if (transceiverInfo.has_value()) {
      return platform_->getPortProfileConfig(PlatformPortProfileConfigMatcher(
          profileID,
          id_,
          buildPlatformPortConfigOverrideFactor(*transceiverInfo)));
    }
  }
  return platform_->getPortProfileConfig(
      PlatformPortProfileConfigMatcher(profileID, id_));
}

const phy::PortProfileConfig PlatformPort::getPortProfileConfigFromCache(
    cfg::PortProfileID profileID) {
  {
    auto cachedProfileConfig = cachedProfileConfig_.rlock();
    if (cachedProfileConfig->has_value() &&
        cachedProfileConfig->value().first == profileID) {
      return cachedProfileConfig->value().second;
    }
  }
  XLOG(DBG4) << "Cached profile config not found for port " << id_
             << " query with profile ID "
             << apache::thrift::util::enumNameSafe(profileID);
  phy::PortProfileConfig profile;
  {
    auto cachedProfileConfig = cachedProfileConfig_.wlock();
    profile = getPortProfileConfig(profileID);
    *cachedProfileConfig = std::make_pair(profileID, profile);
  }
  return profile;
}

// This should only be called by platforms that actually have
// an external phy
std::optional<int32_t> PlatformPort::getExternalPhyID() {
  const auto& platformPortEntry = getPlatformPortEntry();
  const auto& chips = getPlatform()->getDataPlanePhyChips();
  if (chips.empty()) {
    throw FbossError("Not platform data plane phy chips");
  }

  const auto& xphy = utility::getDataPlanePhyChips(
      platformPortEntry, chips, phy::DataPlanePhyChipType::XPHY);
  if (xphy.empty()) {
    return std::nullopt;
  } else {
    // One port should only has one xphy id
    CHECK_EQ(xphy.size(), 1);
    return *xphy.begin()->second.physicalID();
  }
}

std::optional<TransceiverInfo> PlatformPort::getTransceiverInfo(
    folly::EventBase* evb) const {
  auto transID = getTransceiverID();
  try {
    return std::optional(getFutureTransceiverInfo().getVia(evb));
  } catch (const std::exception& e) {
    XLOG(DBG3) << "Error retrieving TransceiverInfo for transceiver "
               << *transID << " Exception: " << folly::exceptionStr(e);
    return std::nullopt;
  }
}

void PlatformPort::clearCachedProfileConfig() {
  auto cachedProfileConfig = cachedProfileConfig_.wlock();
  *cachedProfileConfig = std::nullopt;
}

std::vector<phy::PinID> PlatformPort::getTransceiverLanes(
    std::optional<cfg::PortProfileID> profileID) const {
  return utility::getTransceiverLanes(
      getPlatformPortEntry(), getPlatform()->getDataPlanePhyChips(), profileID);
}

namespace {
constexpr auto kFbossPortNameRegex = "eth(\\d+)/(\\d+)/1";
}

MultiPimPlatformPort::MultiPimPlatformPort(
    PortID id,
    const cfg::PlatformPortEntry& entry) {
  // With the new platform config design, we store port name in the platform
  // config with the format ethX/Y/1, X is the pim number and Y is the
  // transceiver number in general.
  // We can also add pim_id as a new field in the cfg::PlatformPortEntry
  auto portName = *entry.mapping_ref()->name_ref();
  int pimID = 0;
  int transceiverID = 0;
  re2::RE2 portNameRe(kFbossPortNameRegex);
  if (!re2::RE2::FullMatch(portName, portNameRe, &pimID, &transceiverID)) {
    throw FbossError("Invalid port name:", portName, " for port id:", id);
  }
  CHECK_GT(pimID, 0);
  pimID_ = PimID(pimID);
  CHECK_GE(transceiverID, 1);
  transceiverIndexInPim_ = transceiverID - 1;
}

} // namespace facebook::fboss
