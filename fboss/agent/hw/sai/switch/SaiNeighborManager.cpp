/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/hw/sai/switch/SaiNeighborManager.h"
#include "fboss/agent/hw/sai/api/NeighborApi.h"
#include "fboss/agent/hw/sai/store/SaiStore.h"
#include "fboss/agent/hw/sai/switch/SaiBridgeManager.h"
#include "fboss/agent/hw/sai/switch/SaiLagManager.h"
#include "fboss/agent/hw/sai/switch/SaiManagerTable.h"
#include "fboss/agent/hw/sai/switch/SaiNextHopGroupManager.h"
#include "fboss/agent/hw/sai/switch/SaiPortManager.h"
#include "fboss/agent/hw/sai/switch/SaiRouterInterfaceManager.h"
#include "fboss/agent/hw/sai/switch/SaiSwitchManager.h"
#include "fboss/agent/state/ArpEntry.h"
#include "fboss/agent/state/DeltaFunctions.h"
#include "fboss/agent/state/NdpEntry.h"
#include "folly/IPAddress.h"

namespace facebook::fboss {

SaiNeighborManager::SaiNeighborManager(
    SaiStore* saiStore,
    SaiManagerTable* managerTable,
    const SaiPlatform* platform)
    : saiStore_(saiStore), managerTable_(managerTable), platform_(platform) {}

// Helper function to create a SAI NeighborEntry from an FBOSS SwitchState
// NeighborEntry (e.g., NeighborEntry<IPAddressV6, NDPTable>)
template <typename NeighborEntryT>
SaiNeighborTraits::NeighborEntry SaiNeighborManager::saiEntryFromSwEntry(
    const std::shared_ptr<NeighborEntryT>& swEntry) {
  folly::IPAddress ip(swEntry->getIP());
  SaiRouterInterfaceHandle* routerInterfaceHandle =
      managerTable_->routerInterfaceManager().getRouterInterfaceHandle(
          swEntry->getIntfID());
  if (!routerInterfaceHandle) {
    throw FbossError(
        "Failed to create sai_neighbor_entry from NeighborEntry. "
        "No SaiRouterInterface for InterfaceID: ",
        swEntry->getIntfID());
  }
  auto switchId = managerTable_->switchManager().getSwitchSaiId();
  return SaiNeighborTraits::NeighborEntry(
      switchId, routerInterfaceHandle->routerInterface->adapterKey(), ip);
}

template <typename NeighborEntryT>
void SaiNeighborManager::changeNeighbor(
    const std::shared_ptr<NeighborEntryT>& oldSwEntry,
    const std::shared_ptr<NeighborEntryT>& newSwEntry) {
  if (oldSwEntry->isPending() && newSwEntry->isPending()) {
    // We don't maintain pending entries so nothing to do here
  }
  if (oldSwEntry->isPending() && !newSwEntry->isPending()) {
    addNeighbor(newSwEntry);
  }
  if (!oldSwEntry->isPending() && newSwEntry->isPending()) {
    removeNeighbor(oldSwEntry);
  }
  if (!oldSwEntry->isPending() && !newSwEntry->isPending()) {
    if (*oldSwEntry != *newSwEntry) {
      removeNeighbor(oldSwEntry);
      addNeighbor(newSwEntry);
    } else {
      /* attempt to resolve next hops if not already resolved, if already
       * resolved, it would be no-op */
      auto iter = managedNeighbors_.find(saiEntryFromSwEntry(newSwEntry));
      CHECK(iter != managedNeighbors_.end());
      iter->second->notifySubscribers();
    }
  }

  XLOG(DBG2) << "Change Neighbor:: old Neighbor: " << oldSwEntry->str()
             << " new Neighbor: " << newSwEntry->str();
}

template <typename NeighborEntryT>
void SaiNeighborManager::addNeighbor(
    const std::shared_ptr<NeighborEntryT>& swEntry) {
  if (swEntry->isPending()) {
    XLOG(INFO) << "skip adding unresolved neighbor " << swEntry->getIP();
    return;
  }
  XLOG(INFO) << "addNeighbor " << swEntry->getIP();
  auto subscriberKey = saiEntryFromSwEntry(swEntry);
  if (managedNeighbors_.find(subscriberKey) != managedNeighbors_.end()) {
    throw FbossError(
        "Attempted to add duplicate neighbor: ", swEntry->getIP().str());
  }

  SaiPortDescriptor saiPortDesc = swEntry->getPort().isPhysicalPort()
      ? SaiPortDescriptor(swEntry->getPort().phyPortID())
      : SaiPortDescriptor(swEntry->getPort().aggPortID());

  std::optional<sai_uint32_t> metadata;
  if (swEntry->getClassID()) {
    metadata = static_cast<sai_uint32_t>(swEntry->getClassID().value());
  }

  std::optional<sai_uint32_t> encapIndex;
  if (swEntry->getEncapIndex()) {
    encapIndex = static_cast<sai_uint32_t>(swEntry->getEncapIndex().value());
  }
  auto saiRouterIntf =
      managerTable_->routerInterfaceManager().getRouterInterfaceHandle(
          swEntry->getIntfID());

  auto subscriber = std::make_shared<ManagedNeighbor>(
      this,
      std::make_tuple(
          saiPortDesc, saiRouterIntf->routerInterface->adapterKey()),
      std::make_tuple(
          swEntry->getIntfID(), swEntry->getIP(), swEntry->getMac()),
      metadata,
      encapIndex,
      swEntry->getIsLocal());

  SaiObjectEventPublisher::getInstance()->get<SaiFdbTraits>().subscribe(
      subscriber);
  managedNeighbors_.emplace(subscriberKey, std::move(subscriber));
  XLOG(DBG2) << "Add Neighbor: create ManagedNeighbor" << swEntry->str();
}

template <typename NeighborEntryT>
void SaiNeighborManager::removeNeighbor(
    const std::shared_ptr<NeighborEntryT>& swEntry) {
  if (swEntry->isPending()) {
    XLOG(INFO) << "skip removing unresolved neighbor " << swEntry->getIP();
    return;
  }
  XLOG(INFO) << "removeNeighbor " << swEntry->getIP();
  auto subscriberKey = saiEntryFromSwEntry(swEntry);
  if (managedNeighbors_.find(subscriberKey) == managedNeighbors_.end()) {
    throw FbossError(
        "Attempted to remove non-existent neighbor: ", swEntry->getIP());
  }
  managedNeighbors_.erase(subscriberKey);
  XLOG(DBG2) << "Remove Neighbor: " << swEntry->str();
}

void SaiNeighborManager::clear() {
  managedNeighbors_.clear();
}

std::shared_ptr<SaiNeighbor> SaiNeighborManager::createSaiObject(
    const SaiNeighborTraits::AdapterHostKey& key,
    const SaiNeighborTraits::CreateAttributes& attributes) {
  auto& store = saiStore_->get<SaiNeighborTraits>();
  return store.setObject(key, attributes);
}

const SaiNeighborHandle* SaiNeighborManager::getNeighborHandle(
    const SaiNeighborTraits::NeighborEntry& saiEntry) const {
  return getNeighborHandleImpl(saiEntry);
}
SaiNeighborHandle* SaiNeighborManager::getNeighborHandle(
    const SaiNeighborTraits::NeighborEntry& saiEntry) {
  return getNeighborHandleImpl(saiEntry);
}
SaiNeighborHandle* SaiNeighborManager::getNeighborHandleImpl(
    const SaiNeighborTraits::NeighborEntry& saiEntry) const {
  auto itr = managedNeighbors_.find(saiEntry);
  if (itr == managedNeighbors_.end()) {
    return nullptr;
  }
  auto subscriber = itr->second.get();
  if (!subscriber) {
    XLOG(FATAL) << "Invalid null neighbor for ip: " << saiEntry.ip().str();
  }
  return subscriber->getHandle();
}

bool SaiNeighborManager::isLinkUp(SaiPortDescriptor port) {
  if (port.isPhysicalPort()) {
    auto portHandle =
        managerTable_->portManager().getPortHandle(port.phyPortID());
    auto portOperStatus = SaiApiTable::getInstance()->portApi().getAttribute(
        portHandle->port->adapterKey(),
        SaiPortTraits::Attributes::OperStatus{});
    return (portOperStatus == SAI_PORT_OPER_STATUS_UP);
  }
  return managerTable_->lagManager().isMinimumLinkMet(port.aggPortID());
}

std::string SaiNeighborManager::listManagedObjects() const {
  std::string output{};
  for (auto entry : managedNeighbors_) {
    output += entry.second->toString();
    output += "\n";
  }
  return output;
}

void ManagedNeighbor::createObject(PublisherObjects objects) {
  auto fdbEntry = std::get<FdbWeakptr>(objects).lock();
  const auto& ip = std::get<folly::IPAddress>(intfIDAndIpAndMac_);
  auto adapterHostKey = SaiNeighborTraits::NeighborEntry(
      fdbEntry->adapterHostKey().switchId(), getRouterInterfaceSaiId(), ip);

  std::optional<bool> isLocal;
  if (encapIndex_) {
    // Encap index programmed via the sw layer corresponds to a non
    // local neighbor entry. That's when we want to set isLocal attribute.
    // Ideally we would like to set isLocal to true (default sai spec value)
    // always. But some sai adaptors are not happy with that on non VOQ
    // systems
    isLocal = isLocal_;
  }
  auto createAttributes = SaiNeighborTraits::CreateAttributes{
      fdbEntry->adapterHostKey().mac(), metadata_, encapIndex_, isLocal};
  auto object = manager_->createSaiObject(adapterHostKey, createAttributes);
  this->setObject(object);
  handle_->neighbor = getSaiObject();
  handle_->fdbEntry = fdbEntry.get();

  XLOG(DBG2) << "ManagedNeigbhor::createObject: " << toString();
}

void ManagedNeighbor::removeObject(size_t, PublisherObjects) {
  XLOG(DBG2) << "ManagedNeigbhor::removeObject: " << toString();

  this->resetObject();
  handle_->neighbor = nullptr;
  handle_->fdbEntry = nullptr;
}

void ManagedNeighbor::notifySubscribers() const {
  auto neighbor = this->getObject();
  if (!neighbor) {
    return;
  }
  neighbor->notifyAfterCreate(neighbor);
}

std::string ManagedNeighbor::toString() const {
  auto metadataStr =
      metadata_.has_value() ? std::to_string(metadata_.value()) : "none";
  auto encapStr =
      encapIndex_.has_value() ? std::to_string(encapIndex_.value()) : "none";
  auto neighborStr = handle_->neighbor
      ? handle_->neighbor->adapterKey().toString()
      : "NeighborEntry: none";
  auto fdbEntryStr = handle_->fdbEntry
      ? handle_->fdbEntry->adapterKey().toString()
      : "FdbEntry: none";

  const auto& ip = std::get<folly::IPAddress>(intfIDAndIpAndMac_);
  auto saiPortDesc = getSaiPortDesc();
  return folly::to<std::string>(
      getObject() ? "active " : "inactive ",
      "managed neighbor: ",
      "ip: ",
      ip.str(),
      saiPortDesc.str(),
      " metadata: ",
      metadataStr,
      " encapIndex: ",
      encapStr,
      " isLocal: ",
      (isLocal_ ? "Y" : "N"),
      " ",
      neighborStr,
      " ",
      fdbEntryStr);
}

void ManagedNeighbor::handleLinkDown() {
  auto* object = getSaiObject();
  if (!object) {
    XLOG(DBG2)
        << "neighbor is already unresolved, skip notifying link down to subscribed next hops";
    return;
  }
  XLOGF(
      DBG2,
      "neighbor {} notifying link down to subscribed next hops",
      object->adapterHostKey());
  SaiObjectEventPublisher::getInstance()
      ->get<SaiNeighborTraits>()
      .notifyLinkDown(object->adapterHostKey());
}

template SaiNeighborTraits::NeighborEntry
SaiNeighborManager::saiEntryFromSwEntry<NdpEntry>(
    const std::shared_ptr<NdpEntry>& swEntry);
template SaiNeighborTraits::NeighborEntry
SaiNeighborManager::saiEntryFromSwEntry<ArpEntry>(
    const std::shared_ptr<ArpEntry>& swEntry);

template void SaiNeighborManager::changeNeighbor<NdpEntry>(
    const std::shared_ptr<NdpEntry>& oldSwEntry,
    const std::shared_ptr<NdpEntry>& newSwEntry);
template void SaiNeighborManager::changeNeighbor<ArpEntry>(
    const std::shared_ptr<ArpEntry>& oldSwEntry,
    const std::shared_ptr<ArpEntry>& newSwEntry);

template void SaiNeighborManager::addNeighbor<NdpEntry>(
    const std::shared_ptr<NdpEntry>& swEntry);
template void SaiNeighborManager::addNeighbor<ArpEntry>(
    const std::shared_ptr<ArpEntry>& swEntry);

template void SaiNeighborManager::removeNeighbor<NdpEntry>(
    const std::shared_ptr<NdpEntry>& swEntry);
template void SaiNeighborManager::removeNeighbor<ArpEntry>(
    const std::shared_ptr<ArpEntry>& swEntry);

} // namespace facebook::fboss
