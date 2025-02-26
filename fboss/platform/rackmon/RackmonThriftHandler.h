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

#include "fboss/platform/rackmon/Rackmon.h"
#include "fboss/platform/rackmon/if/gen-cpp2/RackmonCtrl.h"

#include <memory>
#include <string>
#include <vector>

namespace rackmonsvc {

class ThriftHandler : virtual public RackmonCtrlSvIf {
  rackmon::Rackmon rackmond_{};

  ModbusDeviceInfo transformModbusDeviceInfo(
      const rackmon::ModbusDeviceInfo& source);
  ModbusRegisterStore transformRegisterStoreValue(
      const rackmon::RegisterStoreValue& source);
  RackmonMonitorData transformModbusDeviceValueData(
      const rackmon::ModbusDeviceValueData& source);
  ModbusRegisterValue transformRegisterValue(
      const rackmon::RegisterValue& value);

 public:
  ThriftHandler();

  void listModbusDevices(
      std::vector<rackmonsvc::ModbusDeviceInfo>& devices) override;

  void getMonitorData(
      std::vector<rackmonsvc::RackmonMonitorData>& data) override;

  void readHoldingRegisters(
      rackmonsvc::ReadWordRegistersResponse& response,
      std::unique_ptr<rackmonsvc::ReadWordRegistersRequest> request) override;

  rackmonsvc::RackmonStatusCode writeSingleRegister(
      std::unique_ptr<rackmonsvc::WriteSingleRegisterRequest> request) override;

  rackmonsvc::RackmonStatusCode presetMultipleRegisters(
      std::unique_ptr<rackmonsvc::PresetMultipleRegistersRequest> request)
      override;

  void readFileRecord(
      rackmonsvc::ReadFileRecordResponse& response,
      std::unique_ptr<rackmonsvc::ReadFileRecordRequest> request) override;

  rackmonsvc::RackmonStatusCode controlRackmond(
      rackmonsvc::RackmonControlRequest request) override;

  void getPowerLossSiren(rackmonsvc::PowerLossSiren& /* plsStatus */) override {
    // TODO
  }
};
} // namespace rackmonsvc
