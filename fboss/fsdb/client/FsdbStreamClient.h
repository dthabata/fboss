// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <fb303/ThreadCachedServiceData.h>
#include <folly/SocketAddress.h>
#include <folly/experimental/coro/AsyncScope.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <thrift/lib/cpp2/async/ClientBufferedStream.h>
#include <thrift/lib/cpp2/async/Sink.h>
#ifndef IS_OSS
#include "fboss/fsdb/if/gen-cpp2/fsdb_oper_types.h"
#endif

#include <atomic>
#include <functional>

namespace folly {
class CancellationToken;
}

namespace apache::thrift {
template <class>
class Client;
} // namespace apache::thrift

namespace facebook::fboss::fsdb {
class FsdbService;

class FsdbStreamClient : public folly::AsyncTimeout {
 public:
  enum class State : uint16_t { DISCONNECTED, CONNECTED, CANCELLED };

  using FsdbStreamStateChangeCb = std::function<void(State, State)>;
  FsdbStreamClient(
      const std::string& clientId,
      folly::EventBase* streamEvb,
      folly::EventBase* connRetryEvb,
      const std::string& counterPrefix,
      FsdbStreamStateChangeCb stateChangeCb = [](State /*old*/,
                                                 State /*newState*/) {});
  virtual ~FsdbStreamClient() override;

  void setServerToConnect(
      const std::string& ip,
      uint16_t port,
      /* allow reset for use in tests*/
      bool allowReset = false);
  void cancel();

  bool isConnectedToServer() const;
  bool isCancelled() const;
  const std::string& clientId() const {
    return clientId_;
  }
  State getState() const {
    return *state_.rlock();
  }
  bool serviceLoopRunning() const {
    return serviceLoopRunning_.load();
  }
  const std::string& getCounterPrefix() const {
    return counterPrefix_;
  }

#ifndef IS_OSS
  template <typename PubUnit>
  using PubStreamT = apache::thrift::ClientSink<PubUnit, OperPubFinalResponse>;
  template <typename SubUnit>
  using SubStreamT = apache::thrift::ClientBufferedStream<SubUnit>;
  using StatePubStreamT = PubStreamT<OperState>;
  using DeltaPubStreamT = PubStreamT<OperDelta>;
  using StateSubStreamT = SubStreamT<OperState>;
  using DeltaSubStreamT = SubStreamT<OperDelta>;

  using StreamT = std::variant<
      StatePubStreamT,
      DeltaPubStreamT,
      StateSubStreamT,
      DeltaSubStreamT>;
#endif

 private:
  void createClient(const std::string& ip, uint16_t port);
  void resetClient();
  void connectToServer(const std::string& ip, uint16_t port);
  void timeoutExpired() noexcept override;

#if FOLLY_HAS_COROUTINES && !defined(IS_OSS)
  folly::coro::Task<void> serviceLoopWrapper();
  virtual folly::coro::Task<StreamT> setupStream() = 0;
  virtual folly::coro::Task<void> serveStream(StreamT&& stream) = 0;
#endif

 protected:
  void setState(State state);
#ifndef IS_OSS
  std::unique_ptr<apache::thrift::Client<FsdbService>> client_;
#endif

 private:
  std::string getConnectedCounterName() {
    return counterPrefix_ + ".connected";
  }
  std::string clientId_;
  folly::EventBase* streamEvb_;
  folly::EventBase* connRetryEvb_;
  folly::Synchronized<State> state_{State::DISCONNECTED};
  std::string counterPrefix_;
  std::unique_ptr<folly::ScopedEventBaseThread> clientEvbThread_;
  std::optional<folly::SocketAddress> serverAddress_;
  FsdbStreamStateChangeCb stateChangeCb_;
  std::atomic<bool> serviceLoopRunning_{false};
#if FOLLY_HAS_COROUTINES
  folly::coro::CancellableAsyncScope serviceLoopScope_;
#endif
  fb303::ThreadCachedServiceData::TLTimeseries disconnectEvents_;
};

} // namespace facebook::fboss::fsdb
