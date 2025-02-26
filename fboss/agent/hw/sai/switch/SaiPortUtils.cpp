/*
 *  Copyright (c) 2004-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/agent/hw/sai/switch/SaiPortUtils.h"

namespace facebook::fboss::utility {

sai_port_flow_control_mode_t getSaiPortPauseMode(cfg::PortPause pause) {
  if (*pause.tx() && *pause.rx()) {
    return SAI_PORT_FLOW_CONTROL_MODE_BOTH_ENABLE;
  } else if (*pause.tx()) {
    return SAI_PORT_FLOW_CONTROL_MODE_TX_ONLY;
  } else if (*pause.rx()) {
    return SAI_PORT_FLOW_CONTROL_MODE_RX_ONLY;
  } else {
    return SAI_PORT_FLOW_CONTROL_MODE_DISABLE;
  }
}

sai_port_internal_loopback_mode_t getSaiPortInternalLoopbackMode(
    cfg::PortLoopbackMode loopbackMode) {
  switch (loopbackMode) {
    case cfg::PortLoopbackMode::NONE:
      return SAI_PORT_INTERNAL_LOOPBACK_MODE_NONE;
    case cfg::PortLoopbackMode::PHY:
      return SAI_PORT_INTERNAL_LOOPBACK_MODE_PHY;
    case cfg::PortLoopbackMode::MAC:
      return SAI_PORT_INTERNAL_LOOPBACK_MODE_MAC;
    default:
      return SAI_PORT_INTERNAL_LOOPBACK_MODE_NONE;
  }
}

cfg::PortLoopbackMode getCfgPortInternalLoopbackMode(
    sai_port_internal_loopback_mode_t mode) {
  switch (mode) {
    case SAI_PORT_INTERNAL_LOOPBACK_MODE_NONE:
      return cfg::PortLoopbackMode::NONE;
    case SAI_PORT_INTERNAL_LOOPBACK_MODE_PHY:
      return cfg::PortLoopbackMode::PHY;
    case SAI_PORT_INTERNAL_LOOPBACK_MODE_MAC:
      return cfg::PortLoopbackMode::MAC;
    default:
      return cfg::PortLoopbackMode::NONE;
  }
}

sai_port_media_type_t getSaiPortMediaType(
    TransmitterTechnology transmitterTech,
    cfg::PortSpeed speed) {
  switch (transmitterTech) {
    case TransmitterTechnology::COPPER:
      return SAI_PORT_MEDIA_TYPE_COPPER;

    case TransmitterTechnology::OPTICAL:
      return SAI_PORT_MEDIA_TYPE_FIBER;

    case TransmitterTechnology::BACKPLANE:
      return SAI_PORT_MEDIA_TYPE_BACKPLANE;
    case TransmitterTechnology::UNKNOWN:
      switch (speed) {
        case cfg::PortSpeed::FORTYG:
        case cfg::PortSpeed::HUNDREDG:
          return SAI_PORT_MEDIA_TYPE_FIBER;

        default:
          return SAI_PORT_MEDIA_TYPE_COPPER;
      }
  }
  return SAI_PORT_MEDIA_TYPE_UNKNOWN;
}

sai_port_fec_mode_t getSaiPortFecMode(phy::FecMode fec) {
  sai_port_fec_mode_t mode = SAI_PORT_FEC_MODE_NONE;
  switch (fec) {
    case phy::FecMode::NONE:
      mode = SAI_PORT_FEC_MODE_NONE;
      break;

    case phy::FecMode::CL74:
      mode = SAI_PORT_FEC_MODE_FC;
      break;

    case phy::FecMode::CL91:
    case phy::FecMode::RS528:
    case phy::FecMode::RS544:
    case phy::FecMode::RS544_2N:
      mode = SAI_PORT_FEC_MODE_RS;
      break;
  }
  return mode;
}

phy::FecMode getFecModeFromSaiFecMode(
    sai_port_fec_mode_t fec,
    cfg::PortProfileID profileID) {
  phy::FecMode mode;
  switch (fec) {
    case SAI_PORT_FEC_MODE_NONE:
      mode = phy::FecMode::NONE;
      break;

    case SAI_PORT_FEC_MODE_RS:
      switch (profileID) {
        case cfg::PortProfileID::PROFILE_100G_4_NRZ_CL91:
        case cfg::PortProfileID::PROFILE_100G_4_NRZ_CL91_COPPER:
        case cfg::PortProfileID::PROFILE_100G_4_NRZ_CL91_OPTICAL:
        case cfg::PortProfileID::PROFILE_100G_4_NRZ_CL91_COPPER_RACK_YV3_T1:
          mode = phy::FecMode::CL91;
          break;
        case cfg::PortProfileID::PROFILE_100G_4_NRZ_RS528:
        case cfg::PortProfileID::PROFILE_25G_1_NRZ_RS528_COPPER:
        case cfg::PortProfileID::PROFILE_50G_2_NRZ_RS528_COPPER:
        case cfg::PortProfileID::PROFILE_100G_4_NRZ_RS528_COPPER:
        case cfg::PortProfileID::PROFILE_100G_4_NRZ_RS528_OPTICAL:
          mode = phy::FecMode::RS528;
          break;
        case cfg::PortProfileID::PROFILE_200G_4_PAM4_RS544X2N:
        case cfg::PortProfileID::PROFILE_400G_8_PAM4_RS544X2N:
        case cfg::PortProfileID::PROFILE_200G_4_PAM4_RS544X2N_COPPER:
        case cfg::PortProfileID::PROFILE_200G_4_PAM4_RS544X2N_OPTICAL:
        case cfg::PortProfileID::PROFILE_400G_8_PAM4_RS544X2N_OPTICAL:
          mode = phy::FecMode::RS544_2N;
          break;
        default:
          mode = phy::FecMode::NONE;
      }
      break;

    case SAI_PORT_FEC_MODE_FC:
      mode = phy::FecMode::CL74;
      break;
  }
  return mode;
}

sai_port_ptp_mode_t getSaiPortPtpMode(bool enable) {
  // NOTE: SAI_PORT_PTP_MODE_TWO_STEP_TIMESTAMP is not supported
  return enable ? SAI_PORT_PTP_MODE_SINGLE_STEP_TIMESTAMP
                : SAI_PORT_PTP_MODE_NONE;
}

} // namespace facebook::fboss::utility
