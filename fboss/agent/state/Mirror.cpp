// Copyright 2004-present Facebook. All Rights Reserved.

#include "fboss/agent/state/Mirror.h"
#include <memory>
#include "common/network/if/gen-cpp2/Address_types.h"
#include "fboss/agent/AddressUtil.h"
#include "fboss/agent/gen-cpp2/switch_state_types.h"
#include "fboss/agent/state/NodeBase-defs.h"
#include "fboss/agent/state/Port.h"
#include "folly/IPAddress.h"
#include "folly/MacAddress.h"
#include "folly/dynamic.h"

namespace facebook::fboss {

namespace {
constexpr auto kSrcIp = "srcIp";
constexpr auto kDstIp = "dstIp";
constexpr auto kSrcMac = "srcMac";
constexpr auto kDstMac = "dstMac";
constexpr auto kName = "name";
constexpr auto kEgressPort = "egressPort";
constexpr auto kDestinationIp = "destinationIp";
constexpr auto kTunnel = "tunnel";
constexpr auto kConfigHasEgressPort = "configHasEgressPort";
constexpr auto kIsResolved = "isResolved";
constexpr auto kDscp = "dscp";
constexpr auto kUdpSrcPort = "udpSrcPort";
constexpr auto kUdpDstPort = "udpDstPort";
constexpr auto kTruncate = "truncate";
constexpr auto kTtl = "ttl";

std::string toThriftMacAddress(const folly::MacAddress& mac) {
  return mac.toString();
}

folly::MacAddress toFollyMacAddress(const std::string& mac) {
  return folly::MacAddress(mac);
}
} // namespace

folly::dynamic MirrorTunnel::toFollyDynamic() const {
  folly::dynamic tunnel = folly::dynamic::object;
  tunnel[kSrcIp] = srcIp.str();
  tunnel[kDstIp] = dstIp.str();
  tunnel[kSrcMac] = srcMac.toString();
  tunnel[kDstMac] = dstMac.toString();
  if (udpPorts.has_value()) {
    tunnel[kUdpSrcPort] = udpPorts.value().udpSrcPort;
    tunnel[kUdpDstPort] = udpPorts.value().udpDstPort;
  }
  tunnel[kTtl] = ttl;
  return tunnel;
}

MirrorTunnel MirrorTunnel::fromFollyDynamic(const folly::dynamic& json) {
  auto tunnel = MirrorTunnel(
      folly::IPAddress(json[kSrcIp].asString()),
      folly::IPAddress(json[kDstIp].asString()),
      folly::MacAddress(json[kSrcMac].asString()),
      folly::MacAddress(json[kDstMac].asString()));

  if (json.find(kUdpSrcPort) != json.items().end()) {
    tunnel.udpPorts =
        TunnelUdpPorts(json[kUdpSrcPort].asInt(), json[kUdpDstPort].asInt());
    tunnel.greProtocol = 0;
  }
  tunnel.ttl = json.getDefault(kTtl, MirrorTunnel::kTTL).asInt();
  return tunnel;
}

folly::dynamic MirrorFields::toFollyDynamicLegacy() const {
  folly::dynamic mirrorFields = folly::dynamic::object;
  mirrorFields[kName] = name;
  if (egressPort) {
    mirrorFields[kEgressPort] = folly::to<std::string>(egressPort.value());
  } else {
    mirrorFields[kEgressPort] = folly::dynamic::object;
  }
  if (destinationIp) {
    mirrorFields[kDestinationIp] = destinationIp.value().str();
  } else {
    mirrorFields[kDestinationIp] = folly::dynamic::object;
  }
  if (srcIp) {
    mirrorFields[kSrcIp] = srcIp.value().str();
  }
  if (resolvedTunnel) {
    mirrorFields[kTunnel] = resolvedTunnel.value().toFollyDynamic();
  } else {
    mirrorFields[kTunnel] = folly::dynamic::object;
  }
  mirrorFields[kConfigHasEgressPort] = configHasEgressPort;
  mirrorFields[kDscp] = dscp;
  mirrorFields[kTruncate] = truncate;
  if (udpPorts.has_value()) {
    mirrorFields[kUdpSrcPort] = udpPorts.value().udpSrcPort;
    mirrorFields[kUdpDstPort] = udpPorts.value().udpDstPort;
  }

  return mirrorFields;
}

Mirror::Mirror(
    std::string name,
    std::optional<PortID> egressPort,
    std::optional<folly::IPAddress> destinationIp,
    std::optional<folly::IPAddress> srcIp,
    std::optional<TunnelUdpPorts> udpPorts,
    uint8_t dscp,
    bool truncate)
    : ThriftyBaseT(
          name,
          egressPort,
          destinationIp,
          srcIp,
          udpPorts,
          dscp,
          truncate) {}

std::string Mirror::getID() const {
  return getFields()->name;
}

std::optional<PortID> Mirror::getEgressPort() const {
  return getFields()->egressPort;
}

std::optional<TunnelUdpPorts> Mirror::getTunnelUdpPorts() const {
  return getFields()->udpPorts;
}

std::optional<MirrorTunnel> Mirror::getMirrorTunnel() const {
  return getFields()->resolvedTunnel;
}

uint8_t Mirror::getDscp() const {
  return getFields()->dscp;
}

bool Mirror::getTruncate() const {
  return getFields()->truncate;
}

void Mirror::setTruncate(bool truncate) {
  writableFields()->truncate = truncate;
}

void Mirror::setEgressPort(PortID egressPort) {
  writableFields()->egressPort = egressPort;
}

void Mirror::setMirrorTunnel(const MirrorTunnel& tunnel) {
  writableFields()->resolvedTunnel = tunnel;
}

folly::dynamic Mirror::toFollyDynamicLegacy() const {
  auto mirror = getFields()->toFollyDynamicLegacy();
  mirror[kIsResolved] = isResolved();
  return mirror;
}

MirrorFields MirrorFields::fromFollyDynamicLegacy(const folly::dynamic& json) {
  auto name = json[kName].asString();
  auto configHasEgressPort = json[kConfigHasEgressPort].asBool();
  uint8_t dscp = cfg::switch_config_constants::DEFAULT_MIRROR_DSCP_;
  if (json.find(kDscp) != json.items().end()) {
    dscp = json[kDscp].asInt();
  }
  bool truncate = false;
  if (json.find(kTruncate) != json.items().end()) {
    truncate = json[kTruncate].asBool();
  }
  auto egressPort = std::optional<PortID>();
  auto destinationIp = std::optional<folly::IPAddress>();
  auto srcIp = std::optional<folly::IPAddress>();
  auto tunnel = std::optional<MirrorTunnel>();
  if (!json[kEgressPort].empty()) {
    egressPort = PortID(json[kEgressPort].asInt());
  }
  if (!json[kDestinationIp].empty()) {
    destinationIp = folly::IPAddress(json[kDestinationIp].asString());
  }
  if (json.find(kSrcIp) != json.items().end()) {
    srcIp = folly::IPAddress(json[kSrcIp].asString());
  }
  if (!json[kTunnel].empty()) {
    tunnel = MirrorTunnel::fromFollyDynamic(json[kTunnel]);
  }

  std::optional<TunnelUdpPorts> udpPorts = std::nullopt;
  if (tunnel.has_value()) {
    udpPorts = tunnel.value().udpPorts;
  } else if (
      (json.find(kUdpSrcPort) != json.items().end()) &&
      (json.find(kUdpDstPort) != json.items().end())) {
    // if the tunnel is not resolved and we warm-boot,
    // src/dst udp ports are needed, which are also stored directly
    // under the mirror config
    udpPorts =
        TunnelUdpPorts(json[kUdpSrcPort].asInt(), json[kUdpDstPort].asInt());
  }
  auto fields = MirrorFields(
      name, egressPort, destinationIp, srcIp, udpPorts, dscp, truncate);
  fields.configHasEgressPort = configHasEgressPort;
  if (tunnel) {
    fields.resolvedTunnel = tunnel.value();
  }
  return fields;
}

std::shared_ptr<Mirror> Mirror::fromFollyDynamicLegacy(
    const folly::dynamic& json) {
  auto fields = MirrorFields::fromFollyDynamicLegacy(json);
  return std::make_shared<Mirror>(fields);
}

bool Mirror::operator==(const Mirror& rhs) const {
  auto f1 = getFields();
  auto f2 = rhs.getFields();
  auto b = (*f1 == *f2);
  return b;
}

bool Mirror::isResolved() const {
  return getMirrorTunnel().has_value() || !getDestinationIp().has_value();
}

bool Mirror::operator!=(const Mirror& rhs) const {
  return !(*this == rhs);
}

bool Mirror::configHasEgressPort() const {
  return getFields()->configHasEgressPort;
}

std::optional<folly::IPAddress> Mirror::getDestinationIp() const {
  return getFields()->destinationIp;
}

std::optional<folly::IPAddress> Mirror::getSrcIp() const {
  return getFields()->srcIp;
}

Mirror::Type Mirror::type() const {
  if (!getFields()->destinationIp) {
    return Mirror::Type::SPAN;
  }
  if (!getFields()->udpPorts) {
    return Mirror::Type::ERSPAN;
  }
  return Mirror::Type::SFLOW;
}

state::MirrorFields MirrorFields::toThrift() const {
  state::MirrorFields thriftMirrorFields;
  thriftMirrorFields.name() = name;
  thriftMirrorFields.dscp() = dscp;
  thriftMirrorFields.configHasEgressPort() = configHasEgressPort;
  thriftMirrorFields.truncate() = truncate;
  thriftMirrorFields.isResolved() =
      resolvedTunnel.has_value() || !destinationIp.has_value();
  if (egressPort) {
    thriftMirrorFields.egressPort() = *egressPort;
  }
  if (destinationIp) {
    thriftMirrorFields.destinationIp() =
        facebook::network::toBinaryAddress(*destinationIp);
  }
  if (srcIp) {
    thriftMirrorFields.srcIp() = facebook::network::toBinaryAddress(*srcIp);
  }
  if (udpPorts) {
    thriftMirrorFields.udpSrcPort() = udpPorts->udpSrcPort;
    thriftMirrorFields.udpDstPort() = udpPorts->udpDstPort;
  }
  if (resolvedTunnel) {
    state::MirrorTunnel tunnel;
    tunnel.srcMac() = toThriftMacAddress(resolvedTunnel->srcMac);
    tunnel.dstMac() = toThriftMacAddress(resolvedTunnel->dstMac);
    tunnel.srcIp() = facebook::network::toBinaryAddress(resolvedTunnel->srcIp);
    tunnel.dstIp() = facebook::network::toBinaryAddress(resolvedTunnel->dstIp);
    if (resolvedTunnel->udpPorts) {
      tunnel.udpSrcPort() = resolvedTunnel->udpPorts->udpSrcPort;
      tunnel.udpDstPort() = resolvedTunnel->udpPorts->udpDstPort;
    }
    tunnel.ttl() = resolvedTunnel->ttl;
    thriftMirrorFields.tunnel() = tunnel;
  }
  return thriftMirrorFields;
}

MirrorFields MirrorFields::fromThrift(state::MirrorFields const& fields) {
  std::string name = *fields.name();
  std::optional<PortID> egressPort{};
  if (fields.egressPort()) {
    egressPort = *fields.egressPort();
  }
  std::optional<folly::IPAddress> destinationIp{};
  if (fields.destinationIp()) {
    destinationIp = facebook::network::toIPAddress(*fields.destinationIp());
  }
  std::optional<folly::IPAddress> srcIp{};
  if (fields.srcIp()) {
    srcIp = facebook::network::toIPAddress(*fields.srcIp());
  }
  std::optional<TunnelUdpPorts> tunnelUdpPorts{};
  if (fields.udpSrcPort() && fields.udpDstPort()) {
    tunnelUdpPorts = TunnelUdpPorts(*fields.udpSrcPort(), *fields.udpDstPort());
  }

  uint8_t dscp = *fields.dscp();
  bool truncate = *fields.truncate();

  auto mirrorFields = MirrorFields(
      name, egressPort, destinationIp, srcIp, tunnelUdpPorts, dscp, truncate);
  mirrorFields.configHasEgressPort = *fields.configHasEgressPort();
  if (fields.tunnel()) {
    std::optional<TunnelUdpPorts> udpPorts{};
    auto srcIp = facebook::network::toIPAddress(*fields.tunnel()->srcIp());
    auto dstIp = facebook::network::toIPAddress(*fields.tunnel()->dstIp());
    auto srcMac = toFollyMacAddress(*fields.tunnel()->srcMac());
    auto dstMac = toFollyMacAddress(*fields.tunnel()->dstMac());
    if (fields.tunnel()->udpSrcPort() && fields.tunnel()->udpDstPort()) {
      udpPorts = TunnelUdpPorts(
          *fields.tunnel()->udpSrcPort(), *fields.tunnel()->udpDstPort());
    }
    auto ttl = *fields.tunnel()->ttl();
    if (udpPorts) {
      mirrorFields.resolvedTunnel =
          MirrorTunnel(srcIp, dstIp, srcMac, dstMac, *udpPorts, ttl);
    } else {
      mirrorFields.resolvedTunnel =
          MirrorTunnel(srcIp, dstIp, srcMac, dstMac, ttl);
    }
  }
  return mirrorFields;
}

folly::dynamic MirrorFields::migrateToThrifty(folly::dynamic const& dyn) {
  folly::dynamic newDyn = dyn;
  if (newDyn.find(kSrcIp) != newDyn.items().end()) {
    ThriftyUtils::translateTo<network::thrift::BinaryAddress>(newDyn[kSrcIp]);
  }
  if (newDyn.find(kDestinationIp) != newDyn.items().end()) {
    if (!newDyn[kDestinationIp].empty()) {
      ThriftyUtils::translateTo<network::thrift::BinaryAddress>(
          newDyn[kDestinationIp]);
    } else {
      newDyn.erase(kDestinationIp);
    }
  }
  if (dyn.find(kEgressPort) != dyn.items().end()) {
    if (!dyn[kEgressPort].empty()) {
      newDyn[kEgressPort] = dyn[kEgressPort].asInt();
    } else {
      newDyn.erase(kEgressPort);
    }
  }
  if (newDyn.find(kTunnel) != newDyn.items().end()) {
    if (!newDyn[kTunnel].empty()) {
      ThriftyUtils::translateTo<network::thrift::BinaryAddress>(
          newDyn[kTunnel][kSrcIp]);
      ThriftyUtils::translateTo<network::thrift::BinaryAddress>(
          newDyn[kTunnel][kDstIp]);
    } else {
      newDyn.erase(kTunnel);
    }
  }
  return newDyn;
}

void MirrorFields::migrateFromThrifty(folly::dynamic& dyn) {
  bool isResolved = true; // span
  if (dyn.find(kSrcIp) != dyn.items().end()) {
    ThriftyUtils::translateTo<folly::IPAddress>(dyn[kSrcIp]);
  }
  if (dyn.find(kDestinationIp) != dyn.items().end()) {
    // erspan or sflow
    isResolved = false;
    ThriftyUtils::translateTo<folly::IPAddress>(dyn[kDestinationIp]);
  } else {
    dyn[kDestinationIp] = folly::dynamic::object;
  }
  if (dyn.find(kTunnel) != dyn.items().end()) {
    ThriftyUtils::translateTo<folly::IPAddress>(dyn[kTunnel][kSrcIp]);
    ThriftyUtils::translateTo<folly::IPAddress>(dyn[kTunnel][kDstIp]);
    // erspan or sflow is resolved
    isResolved = true;
  } else {
    dyn[kTunnel] = folly::dynamic::object;
  }
  if (dyn.find(kEgressPort) != dyn.items().end()) {
    dyn[kEgressPort] =
        folly::to<std::string>(static_cast<PortID>(dyn[kEgressPort].asInt()));
  } else {
    dyn[kEgressPort] = folly::dynamic::object;
  }
  dyn[kIsResolved] = isResolved;
}

template class ThriftyBaseT<state::MirrorFields, Mirror, MirrorFields>;

} // namespace facebook::fboss
