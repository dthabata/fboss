/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/hw/sai/switch/SaiQosMapManager.h"

#include "fboss/agent/FbossError.h"
#include "fboss/agent/hw/sai/api/QosMapApi.h"
#include "fboss/agent/hw/sai/store/SaiStore.h"
#include "fboss/agent/hw/sai/switch/SaiManagerTable.h"
#include "fboss/agent/hw/switch_asics/HwAsic.h"
#include "fboss/agent/platforms/sai/SaiPlatform.h"
#include "fboss/agent/state/SwitchState.h"

namespace facebook::fboss {

SaiQosMapManager::SaiQosMapManager(
    SaiStore* saiStore,
    SaiManagerTable* managerTable,
    const SaiPlatform* platform)
    : saiStore_(saiStore), managerTable_(managerTable), platform_(platform) {}

std::shared_ptr<SaiQosMap> SaiQosMapManager::setDscpToTcQosMap(
    const DscpMap& newDscpMap) {
  std::vector<sai_qos_map_t> mapToValueList;
  const auto& entries = newDscpMap.from();
  mapToValueList.reserve(entries.size());
  for (const auto& entry : entries) {
    sai_qos_map_t mapping{};
    mapping.key.dscp = entry.attr();
    mapping.value.tc = entry.trafficClass();
    mapToValueList.push_back(mapping);
  }
  // set the dscp -> tc mapping in SAI
  SaiQosMapTraits::Attributes::Type typeAttribute{SAI_QOS_MAP_TYPE_DSCP_TO_TC};
  SaiQosMapTraits::Attributes::MapToValueList mapToValueListAttribute{
      mapToValueList};
  auto& store = saiStore_->get<SaiQosMapTraits>();
  SaiQosMapTraits::AdapterHostKey k{typeAttribute};
  SaiQosMapTraits::CreateAttributes c{typeAttribute, mapToValueListAttribute};
  return store.setObject(k, c);
}

std::shared_ptr<SaiQosMap> SaiQosMapManager::setExpToTcQosMap(
    const ExpMap& newExpMap) {
  if (!platform_->getAsic()->isSupported(HwAsic::Feature::SAI_MPLS_QOS)) {
    return nullptr;
  }
  std::vector<sai_qos_map_t> mapToValueList;
  const auto& entries = newExpMap.from();
  mapToValueList.reserve(entries.size());
  for (const auto& entry : entries) {
    sai_qos_map_t mapping{};
    mapping.key.mpls_exp = entry.attr();
    mapping.value.tc = entry.trafficClass();
    mapToValueList.push_back(mapping);
  }
  // set the exp -> tc mapping in SAI
  SaiQosMapTraits::Attributes::Type typeAttribute{
      SAI_QOS_MAP_TYPE_MPLS_EXP_TO_TC};
  SaiQosMapTraits::Attributes::MapToValueList mapToValueListAttribute{
      mapToValueList};
  auto& store = saiStore_->get<SaiQosMapTraits>();
  SaiQosMapTraits::AdapterHostKey k{typeAttribute};
  SaiQosMapTraits::CreateAttributes c{typeAttribute, mapToValueListAttribute};
  return store.setObject(k, c);
}

std::shared_ptr<SaiQosMap> SaiQosMapManager::setTcToExpQosMap(
    const ExpMap& newExpMap) {
  if (!platform_->getAsic()->isSupported(HwAsic::Feature::SAI_MPLS_QOS)) {
    return nullptr;
  }
  std::vector<sai_qos_map_t> mapToValueList;
  const auto& entries = newExpMap.to();
  mapToValueList.reserve(entries.size());
  for (const auto& entry : entries) {
    sai_qos_map_t mapping{};
    mapping.key.tc = entry.trafficClass();
    mapping.key.color = SAI_PACKET_COLOR_GREEN;
    mapping.value.mpls_exp = entry.attr();
    mapToValueList.push_back(mapping);
  }
  // set the tc -> exp mapping in SAI
  SaiQosMapTraits::Attributes::Type typeAttribute{
      SAI_QOS_MAP_TYPE_TC_AND_COLOR_TO_MPLS_EXP};
  SaiQosMapTraits::Attributes::MapToValueList mapToValueListAttribute{
      mapToValueList};
  auto& store = saiStore_->get<SaiQosMapTraits>();
  SaiQosMapTraits::AdapterHostKey k{typeAttribute};
  SaiQosMapTraits::CreateAttributes c{typeAttribute, mapToValueListAttribute};
  return store.setObject(k, c);
}

std::shared_ptr<SaiQosMap> SaiQosMapManager::setTcToQueueQosMap(
    const QosPolicy::TrafficClassToQueueId& newTcToQueueIdMap) {
  std::vector<sai_qos_map_t> mapToValueList;
  mapToValueList.reserve(newTcToQueueIdMap.size());
  for (const auto& tc2q : newTcToQueueIdMap) {
    auto [tc, q] = tc2q;
    sai_qos_map_t mapping{};
    mapping.key.tc = tc;
    mapping.value.queue_index = q;
    mapToValueList.push_back(mapping);
  }
  // set the tc->queue mapping in SAI
  SaiQosMapTraits::Attributes::Type typeAttribute{SAI_QOS_MAP_TYPE_TC_TO_QUEUE};
  SaiQosMapTraits::Attributes::MapToValueList mapToValueListAttribute{
      mapToValueList};
  auto& store = saiStore_->get<SaiQosMapTraits>();
  SaiQosMapTraits::AdapterHostKey k{typeAttribute};
  SaiQosMapTraits::CreateAttributes c{typeAttribute, mapToValueListAttribute};
  return store.setObject(k, c);
}

void SaiQosMapManager::setQosMaps(
    const std::shared_ptr<QosPolicy>& newQosPolicy) {
  XLOG(INFO) << "Setting global QoS map: " << newQosPolicy->getName();
  handle_ = std::make_unique<SaiQosMapHandle>();
  handle_->dscpToTcMap = setDscpToTcQosMap(newQosPolicy->getDscpMap());
  handle_->tcToQueueMap =
      setTcToQueueQosMap(newQosPolicy->getTrafficClassToQueueId());
  handle_->expToTcMap = setExpToTcQosMap(newQosPolicy->getExpMap());
  handle_->tcToExpMap = setTcToExpQosMap(newQosPolicy->getExpMap());
}

void SaiQosMapManager::addQosMap(
    const std::shared_ptr<QosPolicy>& newQosPolicy) {
  if (handle_) {
    throw FbossError("Failed to add QoS map: already programmed");
  }
  return setQosMaps(newQosPolicy);
}

void SaiQosMapManager::removeQosMap() {
  if (!handle_) {
    throw FbossError("Failed to remove QoS map: none programmed");
  }
  handle_.reset();
}

void SaiQosMapManager::changeQosMap(
    const std::shared_ptr<QosPolicy>& oldQosPolicy,
    const std::shared_ptr<QosPolicy>& newQosPolicy) {
  if (!handle_) {
    throw FbossError("Failed to change QoS map: none programmed");
  }
  return setQosMaps(newQosPolicy);
}

const SaiQosMapHandle* SaiQosMapManager::getQosMap() const {
  return getQosMapImpl();
}
SaiQosMapHandle* SaiQosMapManager::getQosMap() {
  return getQosMapImpl();
}
SaiQosMapHandle* SaiQosMapManager::getQosMapImpl() const {
  if (!handle_) {
    return nullptr;
  }
  return handle_.get();
}

} // namespace facebook::fboss
