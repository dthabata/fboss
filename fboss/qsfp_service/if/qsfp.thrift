namespace cpp2 facebook.fboss
namespace go neteng.fboss.qsfp
namespace py neteng.fboss.qsfp
namespace py3 neteng.fboss
namespace py.asyncio neteng.fboss.asyncio.qsfp

include "common/fb303/if/fb303.thrift"
include "fboss/agent/if/ctrl.thrift"
include "fboss/agent/if/fboss.thrift"
include "fboss/qsfp_service/if/transceiver.thrift"
include "fboss/agent/switch_config.thrift"
include "fboss/mka_service/if/mka_structs.thrift"
include "fboss/agent/hw/hardware_stats.thrift"
include "fboss/lib/phy/phy.thrift"
include "fboss/lib/phy/prbs.thrift"

service QsfpService extends phy.FbossCommonPhyCtrl {
  transceiver.TransceiverType getType(1: i32 idx);

  /*
   * Get all information about a transceiver
   */
  map<i32, transceiver.TransceiverInfo> getTransceiverInfo(
    1: list<i32> idx,
  ) throws (1: fboss.FbossBaseError error);
  /*
   * Customise the transceiver based on the speed at which it should run
   */
  void customizeTransceiver(
    1: i32 idx,
    2: switch_config.PortSpeed speed,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * Do a raw read on the data for a specific transceiver.
   */
  map<i32, transceiver.RawDOMData> getTransceiverRawDOMData(
    1: list<i32> idx,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * Read on the raw DOM data for a specific transceiver as a union.
   */
  map<i32, transceiver.DOMDataUnion> getTransceiverDOMDataUnion(
    1: list<i32> idx,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * Tell the qsfp service about the status of ports, retrieve transceiver information
   * for each of these ports.
   */
  map<i32, transceiver.TransceiverInfo> syncPorts(
    1: map<i32, ctrl.PortStatus> ports,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * Qsfp service has an internal remediation loop and may potentially perform
   * interruptive operation to modules that carry no active(up) link. However
   * it may cause some confusion for debugging. This function is to tell
   * qsfp-service to pause auto remediation for the specified amount of seconds.
   */
  void pauseRemediation(1: i32 timeout, 2: list<string> portList);

  /*
   * Qsfp service has an internal remediation loop and may potentially perform
   * interruptive operation to modules that carry no active(up) link. However
   * it may cause some confusion for debugging. This function is to tell
   * what is the currently pause remediation expiration time set to.
   */
  i32 getRemediationUntilTime();

  /*
  * Perform a raw register read for the specified transceivers
  */
  map<i32, transceiver.ReadResponse> readTransceiverRegister(
    1: transceiver.ReadRequest request,
  ) throws (1: fboss.FbossBaseError error);

  /*
  * Perform a raw register write for the specified transceivers
  */
  map<i32, transceiver.WriteResponse> writeTransceiverRegister(
    1: transceiver.WriteRequest request,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * MACSEC capable port ids
  */
  list<i32> getMacsecCapablePorts() throws (1: fboss.FbossBaseError error);

  /*
   * Thrift call to program one PHY port. This call will be made by agent to
   * qsfp_service
   */
  void programXphyPort(
    1: i32 portId,
    2: switch_config.PortProfileID portProfileId,
  );

  phy.PhyInfo getXphyInfo(1: i32 portID) throws (1: fboss.FbossBaseError error);

  /*
   * Change the PRBS setting on a port. Useful when debugging a link
   * down or flapping issue.
   */
  void setPortPrbs(
    1: i32 portId,
    2: phy.PrbsComponent component,
    3: phy.PortPrbsState state,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * Get the PRBS stats on a port. Useful when debugging a link
   * down or flapping issue.
   */
  phy.PrbsStats getPortPrbsStats(
    1: i32 portId,
    2: phy.PrbsComponent component,
  ) throws (1: fboss.FbossBaseError error);

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
  void clearPortPrbsStats(
    1: i32 portId,
    2: phy.PrbsComponent component,
  ) throws (1: fboss.FbossBaseError error);

  string listHwObjects(
    1: list<ctrl.HwObjectType> objects,
    2: bool cached,
  ) throws (1: fboss.FbossBaseError error);

  bool getSdkState(1: string fileName) throws (1: fboss.FbossBaseError error);

  bool sakInstallRx(
    1: mka_structs.MKASak sak,
    2: mka_structs.MKASci sciToAdd,
  ) throws (1: mka_structs.MKAServiceException ex) (cpp.coroutine);

  bool sakInstallTx(1: mka_structs.MKASak sak) throws (
    1: mka_structs.MKAServiceException ex,
  ) (cpp.coroutine);

  bool sakDeleteRx(
    1: mka_structs.MKASak sak,
    2: mka_structs.MKASci sciToRemove,
  ) throws (1: mka_structs.MKAServiceException ex) (cpp.coroutine);

  bool sakDelete(1: mka_structs.MKASak sak) throws (
    1: mka_structs.MKAServiceException ex,
  ) (cpp.coroutine);

  mka_structs.MKASakHealthResponse sakHealthCheck(
    1: mka_structs.MKASak sak,
  ) throws (1: mka_structs.MKAServiceException ex) (cpp.coroutine);

  mka_structs.MacsecPortPhyMap macsecGetPhyPortInfo(
    1: list<string> portNames = [],
  ) throws (1: fboss.FbossBaseError error) (cpp.coroutine);

  ctrl.PortOperState macsecGetPhyLinkInfo(1: string portName) throws (
    1: fboss.FbossBaseError error,
  ) (cpp.coroutine);

  phy.PhyInfo getPhyInfo(1: string portName) throws (
    1: fboss.FbossBaseError error,
  ) (cpp.coroutine);

  bool deleteAllSc(1: string portName) throws (1: fboss.FbossBaseError error) (
    cpp.coroutine,
  );

  bool setupMacsecState(
    1: list<string> portList,
    2: bool macsecDesired = false,
    3: bool dropUnencrypted = false,
  ) throws (1: fboss.FbossBaseError error) (cpp.coroutine);

  map<string, hardware_stats.MacsecStats> getAllMacsecPortStats(
    1: bool readFromHw = false,
  ) throws (1: fboss.FbossBaseError error) (cpp.coroutine);

  map<string, hardware_stats.MacsecStats> getMacsecPortStats(
    1: list<string> portNames,
    2: bool readFromHw = false,
  ) throws (1: fboss.FbossBaseError error) (cpp.coroutine);

  /*
   * Returns the list of supported PRBS polynomials for the given port and
   * prbs component
   */
  list<prbs.PrbsPolynomial> getSupportedPrbsPolynomials(
    1: string portName,
    2: phy.PrbsComponent component,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * Change the PRBS setting on a port. Useful when debugging a link
   * down or flapping issue.
   */
  void setInterfacePrbs(
    1: string portName,
    2: phy.PrbsComponent component,
    3: prbs.InterfacePrbsState state,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * Get the PRBS setting on a port.
   */
  prbs.InterfacePrbsState getInterfacePrbsState(
    1: string portName,
    2: phy.PrbsComponent component,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * Get the PRBS stats on an interface.
   */
  phy.PrbsStats getInterfacePrbsStats(
    1: string portName,
    2: phy.PrbsComponent component,
  ) throws (1: fboss.FbossBaseError error);

  /*
   * Clear the PRBS stats counter on an interface.
   * This clearInterfacePrbsStats will result in:
   * 1. Reset BER counters so that BER calculations start fresh from now
   * 2. Reset maxBer
   * 3. Reset numLossOfLock
   * 4. Set timeSinceLastClear to now
   * 5. Set timeSinceLastLocked to now if prbs is locked else epoch
   */
  void clearInterfacePrbsStats(
    1: string portName,
    2: phy.PrbsComponent component,
  ) throws (1: fboss.FbossBaseError error);
}
