/*
 *  Copyright (c) 2018-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include "fboss/qsfp_service/test/TransceiverManagerTestHelper.h"

#include "fboss/qsfp_service/TransceiverStateMachine.h"
#include "fboss/qsfp_service/module/cmis/CmisModule.h"
#include "fboss/qsfp_service/module/tests/FakeTransceiverImpl.h"
#include "fboss/qsfp_service/test/hw_test/HwTransceiverUtils.h"

namespace facebook::fboss {

/*
 * The recommended way to use TransceiverStateMachineTest to verify the event:
 * 1) Call getAllStates() to get all the states of state machine
 * 2) Call verifyStateMachine() with only the supported states to check the
 *    logic of processing a specified event. NOTE: this function will also
 *    erase the supported states from the input `states`
 * 3) Finally call verifyStateUnchanged() to check the rest of states still in
 *    the original state set to make sure even the code tries to process the
 *    specified event, these states will still stay the same.
 */
class TransceiverStateMachineTest : public TransceiverManagerTestHelper {
 public:
  CmisModule* overrideCmisModule() {
    // Set port status to DOWN so that we can remove the transceiver correctly
    transceiverManager_->setOverrideTcvrToPortAndProfileForTesting(
        overrideTcvrToPortAndProfile_);
    transceiverManager_->refreshStateMachines();
    transceiverManager_->setOverrideAgentPortStatusForTesting(
        false /* up */, true /* enabled */, false /* clearOnly */);
    transceiverManager_->refreshStateMachines();

    auto xcvrImpl = std::make_unique<Cmis200GTransceiver>(id_);
    // This override function use ids starting from 1
    transceiverManager_->overrideMgmtInterface(
        static_cast<int>(id_) + 1,
        uint8_t(TransceiverModuleIdentifier::QSFP_PLUS_CMIS));
    XLOG(INFO) << "Making CMIS QSFP for " << id_;
    auto xcvr = static_cast<CmisModule*>(
        transceiverManager_->overrideTransceiverForTesting(
            id_,
            std::make_unique<CmisModule>(
                transceiverManager_.get(), std::move(xcvrImpl), 1)));

    // Remove the override config we set before
    transceiverManager_->setOverrideTcvrToPortAndProfileForTesting(
        emptyOverrideTcvrToPortAndProfile_);
    transceiverManager_->setOverrideAgentPortStatusForTesting(
        false /* up */, true /* enabled */, true /* clearOnly */);

    return xcvr;
  }

  void setState(TransceiverStateMachineState state) {
    // Always create a new transceiver so that we can make sure the state can
    // go back to the beginning state
    xcvr_ = overrideCmisModule();
    xcvr_->detectPresence();
    switch (state) {
      case TransceiverStateMachineState::NOT_PRESENT:
        // default state is always NOT_PRESENT.
        break;
      case TransceiverStateMachineState::PRESENT:
        // Because we want to verify two events: DETECT_TRANSCEIVER and
        // READ_EEPROM seperately, we have to make sure we updateQsfpData with
        // allPages=True after `DETECT_TRANSCEIVER` but before `READ_EEPROM`
        // to match the same behavior as QsfpModule::refreshLocked()
        transceiverManager_->updateStateBlocking(
            id_, TransceiverStateMachineEvent::DETECT_TRANSCEIVER);
        xcvr_->updateQsfpData(true);
        break;
      case TransceiverStateMachineState::DISCOVERED:
        transceiverManager_->refreshStateMachines();
        break;
      case TransceiverStateMachineState::IPHY_PORTS_PROGRAMMED:
        transceiverManager_->setOverrideTcvrToPortAndProfileForTesting(
            overrideTcvrToPortAndProfile_);
        transceiverManager_->refreshStateMachines();
        break;
      case TransceiverStateMachineState::XPHY_PORTS_PROGRAMMED:
        transceiverManager_->setOverrideTcvrToPortAndProfileForTesting(
            overrideTcvrToPortAndProfile_);
        transceiverManager_->refreshStateMachines();
        // Use updateStateBlocking() to skip PhyManager check
        transceiverManager_->updateStateBlocking(
            id_, TransceiverStateMachineEvent::PROGRAM_XPHY);
        break;
      // TODO(joseph5wu) Will support the reset states later
      default:
        break;
    }
    auto curState = transceiverManager_->getCurrentState(id_);
    EXPECT_EQ(curState, state)
        << "Transceiver=0 state doesn't match state expected="
        << apache::thrift::util::enumNameSafe(state)
        << ", actual=" << apache::thrift::util::enumNameSafe(curState);
  }

  std::set<TransceiverStateMachineState> getAllStates() const {
    return {
        TransceiverStateMachineState::NOT_PRESENT,
        TransceiverStateMachineState::PRESENT,
        TransceiverStateMachineState::DISCOVERED,
        TransceiverStateMachineState::IPHY_PORTS_PROGRAMMED,
        TransceiverStateMachineState::XPHY_PORTS_PROGRAMMED,
        // TODO(joseph5wu) Will support the reset states later
        // TransceiverStateMachineState::TRANSCEIVER_PROGRAMMED,
        // TransceiverStateMachineState::ACTIVE,
        // TransceiverStateMachineState::INACTIVE,
        // TransceiverStateMachineState::UPGRADING,
    };
  }

  template <typename PRE_UPDATE_FN, typename VERIFY_FN>
  void verifyStateMachine(
      const std::set<TransceiverStateMachineState>& supportedStates,
      TransceiverStateMachineEvent event,
      TransceiverStateMachineState expectedState,
      std::set<TransceiverStateMachineState>& states,
      PRE_UPDATE_FN preUpdate,
      VERIFY_FN verify) {
    for (auto preState : supportedStates) {
      if (states.find(preState) == states.end()) {
        // Current state is no longer in the state set, skip checking it
        continue;
      }
      setState(preState);
      // Call preUpdate() before actual stateUpdate()
      preUpdate();
      // Trigger state update with `event`
      transceiverManager_->updateStateBlocking(id_, event);
      auto curState = transceiverManager_->getCurrentState(id_);
      EXPECT_EQ(curState, expectedState)
          << "Transceiver=0 state doesn't match after Event="
          << apache::thrift::util::enumNameSafe(event)
          << ", preState=" << apache::thrift::util::enumNameSafe(preState)
          << ", expected new state="
          << apache::thrift::util::enumNameSafe(expectedState)
          << ", actual=" << apache::thrift::util::enumNameSafe(curState);

      // Verify the result after update finishes
      verify();

      // Remove from the state set
      states.erase(preState);
      ::testing::Mock::VerifyAndClearExpectations(transceiverManager_.get());
    }
  }

  template <typename PRE_UPDATE_FN>
  void verifyStateUnchanged(
      TransceiverStateMachineEvent event,
      std::set<TransceiverStateMachineState>& states,
      PRE_UPDATE_FN preUpdate) {
    for (auto state : states) {
      setState(state);
      // Call preUpdate() before actual stateUpdate()
      preUpdate();
      // Trigger state update with `event`
      transceiverManager_->updateStateBlocking(id_, event);
      auto newState = transceiverManager_->getCurrentState(id_);
      EXPECT_EQ(newState, state)
          << "Transceiver=0 state doesn't match after Event="
          << apache::thrift::util::enumNameSafe(event)
          << ", preState=" << apache::thrift::util::enumNameSafe(state)
          << ", newState=" << apache::thrift::util::enumNameSafe(newState);

      ::testing::Mock::VerifyAndClearExpectations(transceiverManager_.get());
    }
  }

  CmisModule* xcvr_;
  const TransceiverID id_ = TransceiverID(0);
  const TransceiverManager::OverrideTcvrToPortAndProfile
      overrideTcvrToPortAndProfile_ = {
          {id_,
           {
               {PortID(1), cfg::PortProfileID::PROFILE_100G_4_NRZ_CL91_OPTICAL},
           }}};
  const TransceiverManager::OverrideTcvrToPortAndProfile
      emptyOverrideTcvrToPortAndProfile_ = {};
};

TEST_F(TransceiverStateMachineTest, defaultState) {
  overrideCmisModule();
  EXPECT_EQ(
      transceiverManager_->getCurrentState(id_),
      TransceiverStateMachineState::NOT_PRESENT);
  // Check state machine attributes should be reset to default values
  const auto& stateMachine =
      transceiverManager_->getStateMachineForTesting(id_);
  EXPECT_EQ(
      stateMachine.get_attribute(transceiverMgrPtr), transceiverManager_.get());
  EXPECT_EQ(stateMachine.get_attribute(transceiverID), id_);
  EXPECT_FALSE(stateMachine.get_attribute(isIphyProgrammed));
  EXPECT_FALSE(stateMachine.get_attribute(isXphyProgrammed));
  EXPECT_FALSE(stateMachine.get_attribute(isTransceiverProgrammed));
  EXPECT_TRUE(stateMachine.get_attribute(needMarkLastDownTime));
  EXPECT_FALSE(stateMachine.get_attribute(needResetDataPath));
}

TEST_F(TransceiverStateMachineTest, detectTransceiver) {
  auto allStates = getAllStates();
  // Only NOT_PRESENT can accept DETECT_TRANSCEIVER event
  verifyStateMachine(
      {TransceiverStateMachineState::NOT_PRESENT},
      TransceiverStateMachineEvent::DETECT_TRANSCEIVER,
      TransceiverStateMachineState::PRESENT /* expected state */,
      allStates,
      []() {} /* empty preUpdateFn */,
      []() {} /* empty verifyFn */);
  // Other states should not change even though we try to process the event
  verifyStateUnchanged(
      TransceiverStateMachineEvent::DETECT_TRANSCEIVER, allStates, []() {
      } /* empty preUpdateFn */);
}

TEST_F(TransceiverStateMachineTest, readEeprom) {
  auto allStates = getAllStates();
  // Only PRESENT can accept READ_EEPROM event
  verifyStateMachine(
      {TransceiverStateMachineState::PRESENT},
      TransceiverStateMachineEvent::READ_EEPROM,
      TransceiverStateMachineState::DISCOVERED /* expected state */,
      allStates,
      [this]() {
        // Make sure `discoverTransceiver` has been called
        EXPECT_CALL(*transceiverManager_, verifyEepromChecksums(id_)).Times(1);
      },
      [this]() {
        // Enter DISCOVERED will also call `resetProgrammingAttributes`
        const auto& stateMachine =
            transceiverManager_->getStateMachineForTesting(id_);
        EXPECT_FALSE(stateMachine.get_attribute(isIphyProgrammed));
        EXPECT_FALSE(stateMachine.get_attribute(isXphyProgrammed));
        EXPECT_FALSE(stateMachine.get_attribute(isTransceiverProgrammed));
        EXPECT_TRUE(stateMachine.get_attribute(needMarkLastDownTime));

        // Before fetching TransceiverInfo, make sure we call refresh()
        // to update the cached TransceiverInfo
        xcvr_->refresh();
        const auto& info = transceiverManager_->getTransceiverInfo(id_);
        utility::HwTransceiverUtils::verifyDiagsCapability(
            info,
            transceiverManager_->getDiagsCapability(id_),
            false /* skipCheckingIndividualCapability */);
      });
  // Other states should not change even though we try to process the event
  verifyStateUnchanged(
      TransceiverStateMachineEvent::READ_EEPROM, allStates, []() {
      } /* empty preUpdateFn */);
}

TEST_F(TransceiverStateMachineTest, programIphy) {
  auto allStates = getAllStates();
  // Both NOT_PRESENT and DISCOVERED can accept PROGRAM_IPHY event
  verifyStateMachine(
      {TransceiverStateMachineState::NOT_PRESENT,
       TransceiverStateMachineState::DISCOVERED},
      TransceiverStateMachineEvent::PROGRAM_IPHY,
      TransceiverStateMachineState::IPHY_PORTS_PROGRAMMED /* expected state */,
      allStates,
      [this]() {
        transceiverManager_->setOverrideTcvrToPortAndProfileForTesting(
            overrideTcvrToPortAndProfile_);
      },
      [this]() {
        const auto& stateMachine =
            transceiverManager_->getStateMachineForTesting(id_);
        // Now isIphyProgrammed should be true
        EXPECT_TRUE(stateMachine.get_attribute(isIphyProgrammed));
        EXPECT_FALSE(stateMachine.get_attribute(isXphyProgrammed));
        EXPECT_FALSE(stateMachine.get_attribute(isTransceiverProgrammed));
        EXPECT_TRUE(stateMachine.get_attribute(needMarkLastDownTime));

        // checked programmed iphy ports match overrideTcvrToPortAndProfile_
        for (const auto& xcvrToPortAndProfile : overrideTcvrToPortAndProfile_) {
          const auto& programmedIphyPorts =
              transceiverManager_->getProgrammedIphyPortToPortInfo(
                  xcvrToPortAndProfile.first);
          EXPECT_EQ(
              xcvrToPortAndProfile.second.size(), programmedIphyPorts.size());
          for (auto portToProfile : xcvrToPortAndProfile.second) {
            const auto& it = programmedIphyPorts.find(portToProfile.first);
            EXPECT_TRUE(it != programmedIphyPorts.end());
            EXPECT_EQ(portToProfile.second, it->second.profile);
          }
        }
      });
  // Other states should not change even though we try to process the event
  verifyStateUnchanged(
      TransceiverStateMachineEvent::PROGRAM_IPHY, allStates, []() {
      } /* empty preUpdateFn */);
}

TEST_F(TransceiverStateMachineTest, programIphyFailed) {
  auto allStates = getAllStates();
  // If we never set overrideTcvrToPortAndProfile_, programming iphy won't work
  verifyStateMachine(
      {TransceiverStateMachineState::DISCOVERED},
      TransceiverStateMachineEvent::PROGRAM_IPHY,
      TransceiverStateMachineState::DISCOVERED /* expected state */,
      allStates,
      []() {} /* empty preUpdateFn */,
      [this]() {
        const auto& stateMachine =
            transceiverManager_->getStateMachineForTesting(id_);
        // Now isIphyProgrammed should still be false
        EXPECT_FALSE(stateMachine.get_attribute(isIphyProgrammed));
        EXPECT_FALSE(stateMachine.get_attribute(isXphyProgrammed));
        EXPECT_FALSE(stateMachine.get_attribute(isTransceiverProgrammed));
        EXPECT_TRUE(stateMachine.get_attribute(needMarkLastDownTime));

        // checked programmed iphy ports
        const auto& programmedIphyPorts =
            transceiverManager_->getProgrammedIphyPortToPortInfo(id_);
        EXPECT_TRUE(programmedIphyPorts.empty());

        // Now set the override transceiver to port and profile to make
        // programming iphy ports work
        transceiverManager_->setOverrideTcvrToPortAndProfileForTesting(
            overrideTcvrToPortAndProfile_);
        // Then try again, it should succeed
        transceiverManager_->updateStateBlocking(
            id_, TransceiverStateMachineEvent::PROGRAM_IPHY);
        const auto& newStateMachine =
            transceiverManager_->getStateMachineForTesting(id_);
        // Now isIphyProgrammed should be true
        EXPECT_TRUE(newStateMachine.get_attribute(isIphyProgrammed));
        EXPECT_FALSE(newStateMachine.get_attribute(isXphyProgrammed));
        EXPECT_FALSE(newStateMachine.get_attribute(isTransceiverProgrammed));
        // checked programmed iphy ports
        const auto& newProgrammedIphyPorts =
            transceiverManager_->getProgrammedIphyPortToPortInfo(id_);
        EXPECT_EQ(newProgrammedIphyPorts.size(), 1);
      });
}

TEST_F(TransceiverStateMachineTest, programXphy) {
  auto allStates = getAllStates();
  // Only IPHY_PORTS_PROGRAMMED can accept PROGRAM_XPHY event
  verifyStateMachine(
      {TransceiverStateMachineState::IPHY_PORTS_PROGRAMMED},
      TransceiverStateMachineEvent::PROGRAM_XPHY,
      TransceiverStateMachineState::XPHY_PORTS_PROGRAMMED /* expected state */,
      allStates,
      [this]() {
        // Make sure `programExternalPhyPorts` has been called
        EXPECT_CALL(*transceiverManager_, programExternalPhyPorts(id_))
            .Times(1);
      },
      [this]() {
        const auto& stateMachine =
            transceiverManager_->getStateMachineForTesting(id_);
        // Now isXphyProgrammed should be true
        EXPECT_TRUE(stateMachine.get_attribute(isIphyProgrammed));
        EXPECT_TRUE(stateMachine.get_attribute(isXphyProgrammed));
        EXPECT_FALSE(stateMachine.get_attribute(isTransceiverProgrammed));
        EXPECT_TRUE(stateMachine.get_attribute(needMarkLastDownTime));
      });
  // Other states should not change even though we try to process the event
  verifyStateUnchanged(
      TransceiverStateMachineEvent::PROGRAM_XPHY, allStates, [this]() {
        // Make sure `programExternalPhyPorts` has never been called
        EXPECT_CALL(*transceiverManager_, programExternalPhyPorts(id_))
            .Times(0);
      } /* empty preUpdateFn */);
}

ACTION(ThrowFbossError) {
  throw FbossError("Mock FbossError");
}
TEST_F(TransceiverStateMachineTest, programXphyFailed) {
  auto allStates = getAllStates();
  // If programExternalPhyPorts() failed, state shouldn't change
  verifyStateMachine(
      {TransceiverStateMachineState::IPHY_PORTS_PROGRAMMED},
      TransceiverStateMachineEvent::PROGRAM_XPHY,
      TransceiverStateMachineState::IPHY_PORTS_PROGRAMMED /* expected state */,
      allStates,
      [this]() {
        EXPECT_CALL(*transceiverManager_, programExternalPhyPorts(id_))
            .Times(2)
            .WillOnce(ThrowFbossError());
      },
      [this]() {
        const auto& stateMachine =
            transceiverManager_->getStateMachineForTesting(id_);
        EXPECT_TRUE(stateMachine.get_attribute(isIphyProgrammed));
        // Now isXphyProgrammed should still be false
        EXPECT_FALSE(stateMachine.get_attribute(isXphyProgrammed));
        EXPECT_FALSE(stateMachine.get_attribute(isTransceiverProgrammed));

        // Then try again, it should succeed
        transceiverManager_->updateStateBlocking(
            id_, TransceiverStateMachineEvent::PROGRAM_XPHY);
        const auto& newStateMachine =
            transceiverManager_->getStateMachineForTesting(id_);
        EXPECT_TRUE(newStateMachine.get_attribute(isIphyProgrammed));
        // Now isXphyProgrammed should be true
        EXPECT_TRUE(stateMachine.get_attribute(isXphyProgrammed));
        EXPECT_FALSE(newStateMachine.get_attribute(isTransceiverProgrammed));
      });
}
} // namespace facebook::fboss