// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <folly/IPAddressV4.h>

#include "fboss/agent/AddressUtil.h"
#include "fboss/agent/if/gen-cpp2/ctrl_types.h"
#include "fboss/cli/fboss2/utils/CmdClientUtils.h"

#include "fboss/cli/fboss2/commands/show/port/CmdShowPort.h"
#include "fboss/cli/fboss2/commands/show/port/gen-cpp2/model_types.h"
#include "fboss/cli/fboss2/test/CmdHandlerTestBase.h"
#include "nettools/common/TestUtils.h"

using namespace ::testing;

namespace facebook::fboss {

/*
 * Set up port test data
 */
std::map<int32_t, PortInfoThrift> createPortEntries() {
  std::map<int32_t, PortInfoThrift> portMap;

  PortInfoThrift portEntry1;
  portEntry1.portId() = 1;
  portEntry1.name() = "eth1/5/1";
  portEntry1.adminState() = PortAdminState::ENABLED;
  portEntry1.operState() = PortOperState::DOWN;
  portEntry1.speedMbps() = 100000;
  portEntry1.profileID() = "PROFILE_100G_4_NRZ_CL91_COPPER";
  TransceiverIdxThrift tcvr1;
  tcvr1.transceiverId() = 0;
  portEntry1.transceiverIdx() = tcvr1;

  PortInfoThrift portEntry2;
  portEntry2.portId() = 2;
  portEntry2.name() = "eth1/5/2";
  portEntry2.adminState() = PortAdminState::DISABLED;
  portEntry2.operState() = PortOperState::DOWN;
  portEntry2.speedMbps() = 25000;
  portEntry2.profileID() = "PROFILE_25G_1_NRZ_CL74_COPPER";
  TransceiverIdxThrift tcvr2;
  tcvr2.transceiverId() = 1;
  portEntry2.transceiverIdx() = tcvr2;

  PortInfoThrift portEntry3;
  portEntry3.portId() = 3;
  portEntry3.name() = "eth1/5/3";
  portEntry3.adminState() = PortAdminState::ENABLED;
  portEntry3.operState() = PortOperState::UP;
  portEntry3.speedMbps() = 100000;
  portEntry3.profileID() = "PROFILE_100G_4_NRZ_CL91_COPPER";
  TransceiverIdxThrift tcvr3;
  tcvr3.transceiverId() = 2;
  portEntry3.transceiverIdx() = tcvr3;

  PortInfoThrift portEntry4;
  portEntry4.portId() = 8;
  portEntry4.name() = "fab402/9/1";
  portEntry4.adminState() = PortAdminState::ENABLED;
  portEntry4.operState() = PortOperState::UP;
  portEntry4.speedMbps() = 100000;
  portEntry4.profileID() = "PROFILE_100G_4_NRZ_NOFEC_COPPER";
  TransceiverIdxThrift tcvr4;
  tcvr4.transceiverId() = 3;
  portEntry4.transceiverIdx() = tcvr4;

  PortInfoThrift portEntry5;
  portEntry5.portId() = 7;
  portEntry5.name() = "eth1/10/2";
  portEntry5.adminState() = PortAdminState::ENABLED;
  portEntry5.operState() = PortOperState::UP;
  portEntry5.speedMbps() = 100000;
  portEntry5.profileID() = "PROFILE_100G_4_NRZ_CL91_OPTICAL";
  TransceiverIdxThrift tcvr5;
  tcvr5.transceiverId() = 4;
  portEntry5.transceiverIdx() = tcvr5;

  PortInfoThrift portEntry6;
  portEntry6.portId() = 9;
  portEntry6.name() = "eth1/4/1";
  portEntry6.adminState() = PortAdminState::ENABLED;
  portEntry6.operState() = PortOperState::UP;
  portEntry6.speedMbps() = 100000;
  portEntry6.profileID() = "PROFILE_100G_4_NRZ_CL91_OPTICAL";
  TransceiverIdxThrift tcvr6;
  tcvr6.transceiverId() = 5;
  portEntry6.transceiverIdx() = tcvr6;

  portMap[portEntry1.get_portId()] = portEntry1;
  portMap[portEntry2.get_portId()] = portEntry2;
  portMap[portEntry3.get_portId()] = portEntry3;
  portMap[portEntry4.get_portId()] = portEntry4;
  portMap[portEntry5.get_portId()] = portEntry5;
  portMap[portEntry6.get_portId()] = portEntry6;
  return portMap;
}

std::map<int32_t, PortInfoThrift> createInvalidPortEntries() {
  std::map<int32_t, PortInfoThrift> portMap;

  // port name format <module name><module number>/<port number>/<subport
  // number>
  // missing <module number>

  PortInfoThrift portEntry1, portEntry2;
  portEntry1.portId() = 1;
  portEntry1.name() = "eth/5/1";

  portEntry2.portId() = 2;
  portEntry2.name() = "eth/5/1";

  portMap[portEntry1.get_portId()] = portEntry1;
  portMap[portEntry2.get_portId()] = portEntry2;
  return portMap;
}

/*
 * Set up transceiver test data
 */
std::map<int, TransceiverInfo> createTransceiverEntries() {
  std::map<int, TransceiverInfo> transceiverMap;

  TransceiverInfo transceiverEntry1;
  transceiverEntry1.present() = true;

  TransceiverInfo transceiverEntry2;
  transceiverEntry2.present() = true;

  TransceiverInfo transceiverEntry3;
  transceiverEntry3.present() = false;

  TransceiverInfo transceiverEntry4;
  transceiverEntry4.present() = false;

  TransceiverInfo transceiverEntry5;
  transceiverEntry5.present() = true;

  TransceiverInfo transceiverEntry6;
  transceiverEntry6.present() = true;

  transceiverMap[0] = transceiverEntry1;
  transceiverMap[1] = transceiverEntry2;
  transceiverMap[2] = transceiverEntry3;
  transceiverMap[3] = transceiverEntry4;
  transceiverMap[4] = transceiverEntry5;
  transceiverMap[5] = transceiverEntry6;
  return transceiverMap;
}

cli::ShowPortModel createPortModel() {
  cli::ShowPortModel model;

  cli::PortEntry entry1, entry2, entry3, entry4, entry5, entry6;
  entry1.id() = 1;
  entry1.name() = "eth1/5/1";
  entry1.adminState() = "Enabled";
  entry1.linkState() = "Down";
  entry1.speed() = "100G";
  entry1.profileId() = "PROFILE_100G_4_NRZ_CL91_COPPER";
  entry1.tcvrID() = 0;
  entry1.tcvrPresent() = "Present";

  entry2.id() = 2;
  entry2.name() = "eth1/5/2";
  entry2.adminState() = "Disabled";
  entry2.linkState() = "Down";
  entry2.speed() = "25G";
  entry2.profileId() = "PROFILE_25G_1_NRZ_CL74_COPPER";
  entry2.tcvrID() = 1;
  entry2.tcvrPresent() = "Present";

  entry3.id() = 3;
  entry3.name() = "eth1/5/3";
  entry3.adminState() = "Enabled";
  entry3.linkState() = "Up";
  entry3.speed() = "100G";
  entry3.profileId() = "PROFILE_100G_4_NRZ_CL91_COPPER";
  entry3.tcvrID() = 2;
  entry3.tcvrPresent() = "Absent";

  entry4.id() = 8;
  entry4.name() = "fab402/9/1";
  entry4.adminState() = "Enabled";
  entry4.linkState() = "Up";
  entry4.speed() = "100G";
  entry4.profileId() = "PROFILE_100G_4_NRZ_NOFEC_COPPER";
  entry4.tcvrID() = 3;
  entry4.tcvrPresent() = "Absent";

  entry5.id() = 7;
  entry5.name() = "eth1/10/2";
  entry5.adminState() = "Enabled";
  entry5.linkState() = "Up";
  entry5.speed() = "100G";
  entry5.profileId() = "PROFILE_100G_4_NRZ_CL91_OPTICAL";
  entry5.tcvrID() = 4;
  entry5.tcvrPresent() = "Present";

  entry6.id() = 9;
  entry6.name() = "eth1/4/1";
  entry6.adminState() = "Enabled";
  entry6.linkState() = "Up";
  entry6.speed() = "100G";
  entry6.profileId() = "PROFILE_100G_4_NRZ_CL91_OPTICAL";
  entry6.tcvrID() = 5;
  entry6.tcvrPresent() = "Present";

  // sorted by name
  model.portEntries() = {entry6, entry1, entry2, entry3, entry5, entry4};
  return model;
}

class CmdShowPortTestFixture : public CmdHandlerTestBase {
 public:
  CmdShowPortTraits::ObjectArgType queriedEntries;
  std::map<int32_t, facebook::fboss::PortInfoThrift> mockPortEntries;
  std::map<int32_t, facebook::fboss::TransceiverInfo> mockTransceiverEntries;
  cli::ShowPortModel normalizedModel;

  void SetUp() override {
    CmdHandlerTestBase::SetUp();
    mockPortEntries = createPortEntries();
    mockTransceiverEntries = createTransceiverEntries();
    normalizedModel = createPortModel();
  }
};

TEST_F(CmdShowPortTestFixture, sortByName) {
  auto model = CmdShowPort().createModel(
      mockPortEntries, mockTransceiverEntries, queriedEntries);

  EXPECT_THRIFT_EQ(model, normalizedModel);
}

TEST_F(CmdShowPortTestFixture, invalidPortName) {
  auto invalidPortEntries = createInvalidPortEntries();

  try {
    CmdShowPort().createModel(
        invalidPortEntries, mockTransceiverEntries, queriedEntries);
    FAIL();
  } catch (const std::invalid_argument& expected) {
    ASSERT_STREQ(
        "Invalid port name: eth/5/1\n"
        "Port name must match 'moduleNum/port/subport' pattern",
        expected.what());
  }
}

TEST_F(CmdShowPortTestFixture, queryClient) {
  setupMockedAgentServer();
  EXPECT_CALL(getMockAgent(), getAllPortInfo(_))
      .WillOnce(Invoke([&](auto& entries) { entries = mockPortEntries; }));

  EXPECT_CALL(getQsfpService(), getTransceiverInfo(_, _))
      .WillOnce(Invoke(
          [&](auto& entries, auto) { entries = mockTransceiverEntries; }));

  auto cmd = CmdShowPort();
  CmdShowPortTraits::ObjectArgType queriedEntries;
  auto model = cmd.queryClient(localhost(), queriedEntries);

  EXPECT_THRIFT_EQ(model, normalizedModel);
}

TEST_F(CmdShowPortTestFixture, printOutput) {
  std::stringstream ss;
  CmdShowPort().printOutput(normalizedModel, ss);

  std::string output = ss.str();
  std::string expectOutput =
      " ID  Name        AdminState  LinkState  Transceiver  TcvrID  Speed  ProfileID                       \n"
      "-------------------------------------------------------------------------------------------------------------\n"
      " 9   eth1/4/1    Enabled     Up         Present      5       100G   PROFILE_100G_4_NRZ_CL91_OPTICAL \n"
      " 1   eth1/5/1    Enabled     Down       Present      0       100G   PROFILE_100G_4_NRZ_CL91_COPPER  \n"
      " 2   eth1/5/2    Disabled    Down       Present      1       25G    PROFILE_25G_1_NRZ_CL74_COPPER   \n"
      " 3   eth1/5/3    Enabled     Up         Absent       2       100G   PROFILE_100G_4_NRZ_CL91_COPPER  \n"
      " 7   eth1/10/2   Enabled     Up         Present      4       100G   PROFILE_100G_4_NRZ_CL91_OPTICAL \n"
      " 8   fab402/9/1  Enabled     Up         Absent       3       100G   PROFILE_100G_4_NRZ_NOFEC_COPPER \n\n";

  EXPECT_EQ(output, expectOutput);
}

} // namespace facebook::fboss
