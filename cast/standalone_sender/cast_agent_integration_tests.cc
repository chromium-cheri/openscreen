// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/cast_trust_store.h"
#include "cast/common/certificate/testing/test_helpers.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/receiver/public/receiver_socket_factory.h"
#include "cast/standalone_sender/cast_agent.h"
#include "gtest/gtest.h"
#include "platform/api/serial_delete_ptr.h"
#include "platform/api/time.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "platform/impl/task_runner.h"

namespace openscreen {
namespace cast {
namespace {

// Based heavily on ReceiverSocketsClient from cast_socket_e2e_test.cc.
class MockReceiver final : public ReceiverSocketFactory::Client,
                           public VirtualConnectionRouter::SocketErrorHandler {
 public:
  explicit MockReceiver(VirtualConnectionRouter* router) : router_(router) {}
  ~MockReceiver() = default;

  const IPEndpoint& endpoint() const { return endpoint_; }
  CastSocket* socket() const { return socket_; }

  // ReceiverSocketFactory::Client overrides.
  void OnConnected(ReceiverSocketFactory* factory,
                   const IPEndpoint& endpoint,
                   std::unique_ptr<CastSocket> socket) override {
    OSP_CHECK(!socket_);
    OSP_LOG_INFO << "\tReceiver got connection from endpoint: " << endpoint;
    endpoint_ = endpoint;
    socket_ = socket.get();
    router_->TakeSocket(this, std::move(socket));
  }

  void OnError(ReceiverSocketFactory* factory, Error error) override {
    OSP_NOTREACHED() << error;
  }

  // VirtualConnectionRouter::SocketErrorHandler overrides.
  void OnClose(CastSocket* socket) override {}
  void OnError(CastSocket* socket, Error error) override {
    OSP_NOTREACHED() << error;
  }

 private:
  VirtualConnectionRouter* router_;
  IPEndpoint endpoint_;
  std::atomic<CastSocket*> socket_{nullptr};
};

class CastAgentIntegrationTest : public ::testing::Test {
 public:
  void SetUp() override {
    PlatformClientPosix::Create(std::chrono::milliseconds(50),
                                std::chrono::milliseconds(50));
    task_runner_ = reinterpret_cast<TaskRunnerImpl*>(
        PlatformClientPosix::GetInstance()->GetTaskRunner());

    // receiver_router_ = MakeSerialDelete<VirtualConnectionRouter>(
    //     task_runner_, &receiver_vc_manager_);
    // mock_receiver_ = std::make_unique<MockReceiver>(receiver_router_.get());
    // receiver_factory_ = MakeSerialDelete<SenderSocketFactory>(
    //     task_runner_, mock_receiver_.get(), task_runner_);
    // receiver_tls_factory_ = SerialDeletePtr<TlsConnectionFactory>(
    //     task_runner_,
    //     TlsConnectionFactory::CreateFactory(receiver_factory_.get(),
    //     task_runner_)
    //         .release());
    // receiver_factory_->set_factory(receiver_tls_factory_.get());
  }

  void TearDown() override {
    receiver_router_.reset();
    receiver_tls_factory_.reset();
    receiver_factory_.reset();
    PlatformClientPosix::ShutDown();
    // Must be shut down after platform client, so joined tasks
    // depending on certs are called correctly.
    CastTrustStore::ResetInstance();
  }

  void WaitAndAssertSenderSocketConnected() {
    constexpr int kMaxAttempts = 10;
    constexpr std::chrono::milliseconds kSocketWaitDelay(250);
    for (int i = 0; i < kMaxAttempts; ++i) {
      OSP_LOG_INFO << "\tChecking for CastSocket, attempt " << i + 1 << "/"
                   << kMaxAttempts;
      if (mock_receiver_->socket()) {
        break;
      }
      std::this_thread::sleep_for(kSocketWaitDelay);
    }
    ASSERT_TRUE(mock_receiver_->socket());
  }

  TaskRunnerImpl* task_runner_;
  VirtualConnectionManager receiver_vc_manager_;
  SerialDeletePtr<VirtualConnectionRouter> receiver_router_;
  std::unique_ptr<MockReceiver> mock_receiver_;
  SerialDeletePtr<ReceiverSocketFactory> receiver_factory_;
  SerialDeletePtr<TlsConnectionFactory> receiver_tls_factory_;
};

// TEST_F(CastAgentIntegrationTest, CanConnect) {
//   absl::optional<InterfaceInfo> loopback = GetLoopbackInterfaceForTesting();
//   ASSERT_TRUE(loopback.has_value());

//   ErrorOr<GeneratedCredentials> creds =
//       GenerateCredentials("Test Device Certificate");
//   ASSERT_TRUE(creds.is_value());
//   CastTrustStore::CreateInstanceForTest(creds.value().root_cert_der);

//   auto agent = MakeSerialDelete<CastAgent>(
//       task_runner_, task_runner_, loopback.value(),
//       creds.value().provider.get(), creds.value().tls_credentials);
//   EXPECT_TRUE(agent->Start().ok());
//   AssertConnect(loopback.value().GetIpAddressV4());
//   EXPECT_TRUE(agent->Stop().ok());
// }

}  // namespace
}  // namespace cast
}  // namespace openscreen
