// Copyright 2004-present Facebook. All Rights Reserved.
#pragma once

#include <folly/futures/Future.h>

#include "common/fb303/cpp/FacebookBase2.h"

#include "fboss/agent/hw/gen-cpp2/hardware_stats_types.h"
#include "fboss/agent/if/gen-cpp2/ctrl_types.h"
#include "fboss/lib/phy/gen-cpp2/phy_types.h"
#include "fboss/lib/phy/gen-cpp2/prbs_types.h"
#include "fboss/mka_service/handlers/MacsecHandler.h"
#include "fboss/qsfp_service/TransceiverManager.h"
#include "fboss/qsfp_service/if/gen-cpp2/QsfpService.h"

DECLARE_string(sak_list_warmboot_config);
DECLARE_int32(phy_service_macsec_port);

namespace facebook {
namespace fboss {

class QsfpServiceHandler
    : public facebook::fboss::QsfpServiceSvIf,
      public ::facebook::fb303::FacebookBase2DeprecationMigration {
 public:
  QsfpServiceHandler(
      std::unique_ptr<TransceiverManager> manager,
      std::shared_ptr<mka::MacsecHandler> handler);
  ~QsfpServiceHandler() override = default;

  void init();
  facebook::fb303::cpp2::fb_status getStatus() override;
  TransceiverType getType(int32_t idx) override;
  /*
   * Returns all qsfp information for the transceiver
   */
  void getTransceiverInfo(
      std::map<int32_t, TransceiverInfo>& info,
      std::unique_ptr<std::vector<int32_t>> ids) override;

  /*
   * Returns raw DOM page data for each passed in transceiver.
   */
  void getTransceiverRawDOMData(
      std::map<int32_t, RawDOMData>& info,
      std::unique_ptr<std::vector<int32_t>> ids) override;

  /*
   * Returns a union of the two raw DOM data format for each passed in
   * transceiver.
   */
  void getTransceiverDOMDataUnion(
      std::map<int32_t, DOMDataUnion>& info,
      std::unique_ptr<std::vector<int32_t>> ids) override;

  /*
   * Store port status information and return relevant transceiver map.
   */
  void syncPorts(
      std::map<int32_t, TransceiverInfo>& info,
      std::unique_ptr<std::map<int32_t, PortStatus>> ports) override;

  /*
   * Customise the transceiver based on the speed at which it has
   * been configured to operate at
   */
  void customizeTransceiver(int32_t idx, cfg::PortSpeed speed) override;

  /*
   * Return a pointer to the transceiver manager.
   */
  TransceiverManager* getTransceiverManager() const {
    return manager_.get();
  }

  void pauseRemediation(
      int32_t timeout,
      std::unique_ptr<std::vector<std::string>> portList) override;

  int32_t getRemediationUntilTime() override;

  void readTransceiverRegister(
      std::map<int32_t, ReadResponse>& response,
      std::unique_ptr<ReadRequest> request) override;

  void writeTransceiverRegister(
      std::map<int32_t, WriteResponse>& response,
      std::unique_ptr<WriteRequest> request) override;

  /*
   * Thrift call servicing routine for programming one PHY port
   */
  void programXphyPort(int32_t portId, cfg::PortProfileID portProfileId)
      override;

  void getXphyInfo(phy::PhyInfo& response, int32_t portID) override;

  /*
   * Handle: PortID
   * Change the PRBS setting on a port. Useful when debugging a link
   * down or flapping issue.
   */
  void setPortPrbs(
      int32_t portId,
      phy::PrbsComponent component,
      std::unique_ptr<phy::PortPrbsState> state) override;

  /*
   * Handle: Interface name
   * Change the PRBS setting on a port. Useful when debugging a link
   * down or flapping issue.
   */
  void setInterfacePrbs(
      std::unique_ptr<std::string> portName,
      phy::PrbsComponent component,
      std::unique_ptr<prbs::InterfacePrbsState> state) override;

  /*
   * Get the PRBS state on a port
   */
  void getInterfacePrbsState(
      prbs::InterfacePrbsState& prbsState,
      std::unique_ptr<std::string> portName,
      phy::PrbsComponent component) override;

  /*
   * Get the PRBS stats on an interface. Useful when debugging a link
   * down or flapping issue.
   */
  void getInterfacePrbsStats(
      phy::PrbsStats& response,
      std::unique_ptr<std::string> portName,
      phy::PrbsComponent component) override;

  /*
   * Get the PRBS stats on a port. Useful when debugging a link
   * down or flapping issue.
   */
  void getPortPrbsStats(
      phy::PrbsStats& response,
      int32_t portId,
      phy::PrbsComponent component) override;

  void clearInterfacePrbsStats(
      std::unique_ptr<std::string> portName,
      phy::PrbsComponent component) override;

  /*
   * Get the list of supported PRBS polynomials for the given port and
   * prbs component
   */
  void getSupportedPrbsPolynomials(
      std::vector<prbs::PrbsPolynomial>& prbsCapabilities,
      std::unique_ptr<std::string> portName,
      phy::PrbsComponent component) override;

  /*
   * Clear the PRBS stats counter on a port. Useful when debugging a link
   * down or flapping issue.
   * This clearPortPrbsStats will result in:
   * 1. reset ber (due to reset accumulated error count if implemented)
   * 2. reset maxBer
   * 3. reset numLossOfLock to 0
   * 4. set timeLastCleared to now
   * 5. set timeLastLocked to timeLastCollect if locked else epoch
   * 6. locked status not changed
   * 7. timeLastCollect not changed
   */
  void clearPortPrbsStats(int32_t portId, phy::PrbsComponent component)
      override;

  void getMacsecCapablePorts(std::vector<int32_t>& ports) override;

  void listHwObjects(
      std::string& out,
      std::unique_ptr<std::vector<HwObjectType>> hwObjects,
      bool cached) override;

  bool getSdkState(std::unique_ptr<std::string> fileName) override;

  void publishLinkSnapshots(
      std::unique_ptr<std::vector<std::string>> portNames) override;

#if FOLLY_HAS_COROUTINES
  folly::coro::Task<bool> co_sakInstallRx(
      std::unique_ptr<mka::MKASak> sak,
      std::unique_ptr<mka::MKASci> sciToAdd) override;

  folly::coro::Task<bool> co_sakInstallTx(
      std::unique_ptr<mka::MKASak> sak) override;

  folly::coro::Task<bool> co_sakDeleteRx(
      std::unique_ptr<mka::MKASak> sak,
      std::unique_ptr<mka::MKASci> sciToRemove) override;

  folly::coro::Task<bool> co_sakDelete(
      std::unique_ptr<mka::MKASak> sak) override;

  folly::coro::Task<std::unique_ptr<mka::MKASakHealthResponse>>
  co_sakHealthCheck(std::unique_ptr<mka::MKASak> sak) override;

  folly::coro::Task<std::unique_ptr<mka::MacsecPortPhyMap>>
  co_macsecGetPhyPortInfo(
      std::unique_ptr<std::vector<std::string>> portNames) override;

  folly::coro::Task<PortOperState> co_macsecGetPhyLinkInfo(
      std::unique_ptr<std::string> portName) override;

  folly::coro::Task<std::unique_ptr<phy::PhyInfo>> co_getPhyInfo(
      std::unique_ptr<std::string> portName) override;

  folly::coro::Task<bool> co_deleteAllSc(
      std::unique_ptr<std::string> portName) override;

  folly::coro::Task<bool> co_setupMacsecState(
      std::unique_ptr<std::vector<std::string>> portList,
      bool macsecDesired,
      bool dropUnencrypted) override;

  folly::coro::Task<std::unique_ptr<std::map<std::string, MacsecStats>>>
  co_getAllMacsecPortStats(bool readFromHw) override;

  folly::coro::Task<std::unique_ptr<std::map<std::string, MacsecStats>>>
  co_getMacsecPortStats(
      std::unique_ptr<std::vector<std::string>> portNames,
      bool readFromHw) override;

#endif

 private:
  // Forbidden copy constructor and assignment operator
  QsfpServiceHandler(QsfpServiceHandler const&) = delete;
  QsfpServiceHandler& operator=(QsfpServiceHandler const&) = delete;

  void validateHandler() const;

  std::unique_ptr<TransceiverManager> manager_{nullptr};
  std::shared_ptr<mka::MacsecHandler> macsecHandler_;
};
} // namespace fboss
} // namespace facebook
