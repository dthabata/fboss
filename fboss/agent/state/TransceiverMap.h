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

#include "fboss/agent/gen-cpp2/switch_state_types.h"
#include "fboss/agent/state/NodeMap.h"
#include "fboss/agent/state/Thrifty.h"
#include "fboss/agent/state/Transceiver.h"
#include "fboss/agent/types.h"

namespace facebook::fboss {

class SwitchState;

using TransceiverMapTraits = NodeMapTraits<TransceiverID, TransceiverSpec>;

/*
 * A container for all the present Transceivers
 */
class TransceiverMap
    : public ThriftyNodeMapT<
          TransceiverMap,
          TransceiverMapTraits,
          ThriftyNodeMapTraits<int16_t, state::TransceiverSpecFields>> {
 public:
  TransceiverMap();
  ~TransceiverMap() override;

  const std::shared_ptr<TransceiverSpec>& getTransceiver(
      TransceiverID id) const {
    return getNode(id);
  }
  std::shared_ptr<TransceiverSpec> getTransceiverIf(TransceiverID id) const {
    return getNodeIf(id);
  }

  void addTransceiver(const std::shared_ptr<TransceiverSpec>& tcvr);
  void updateTransceiver(const std::shared_ptr<TransceiverSpec>& tcvr);
  void removeTransceiver(TransceiverID id);

  TransceiverMap* modify(std::shared_ptr<SwitchState>* state);

 private:
  // Inherit the constructors required for clone()
  using ThriftyNodeMapT::ThriftyNodeMapT;
  friend class CloneAllocator;
};
} // namespace facebook::fboss
