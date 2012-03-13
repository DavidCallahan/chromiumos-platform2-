// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/async_connection.h"

#include <netinet/in.h>

#include <vector>

#include <gtest/gtest.h>

#include "shill/ip_address.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_sockets.h"

using std::string;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnNew;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Test;

namespace shill {

namespace {
const char kInterfaceName[] = "int0";
const char kConnectAddress[] = "10.11.12.13";
const int kConnectPort = 10203;
const int kErrorNumber = 30405;
const int kSocketFD = 60708;
}  // namespace {}

class AsyncConnectionTest : public Test {
 public:
  AsyncConnectionTest()
      : async_connection_(kInterfaceName, &dispatcher_, &sockets_,
                          callback_target_.callback()),
        address_(IPAddress::kFamilyIPv4) { }

  virtual void SetUp() {
    EXPECT_TRUE(address_.SetAddressFromString(kConnectAddress));
  }
  virtual void TearDown() {
    if (async_connection_.fd_ >= 0) {
      EXPECT_CALL(sockets(), Close(kSocketFD))
          .WillOnce(Return(0));
    }
  }

 protected:
  class ConnectCallbackTarget {
   public:
    ConnectCallbackTarget()
        : callback_(NewCallback(this, &ConnectCallbackTarget::CallTarget)) {}

    MOCK_METHOD2(CallTarget, void(bool success, int fd));
    Callback2<bool, int>::Type *callback() { return callback_.get(); }

   private:
    scoped_ptr<Callback2<bool, int>::Type> callback_;
  };

  void ExpectReset() {
    EXPECT_STREQ(kInterfaceName, async_connection_.interface_name_.c_str());
    EXPECT_EQ(&dispatcher_, async_connection_.dispatcher_);
    EXPECT_EQ(&sockets_, async_connection_.sockets_);
    EXPECT_EQ(callback_target_.callback(), async_connection_.callback_);
    EXPECT_EQ(-1, async_connection_.fd_);
    EXPECT_TRUE(async_connection_.connect_completion_callback_.get());
    EXPECT_FALSE(async_connection_.connect_completion_handler_.get());
  }

  void StartConnection() {
    EXPECT_CALL(sockets_, Socket(_, _, _))
        .WillOnce(Return(kSocketFD));
    EXPECT_CALL(sockets_, SetNonBlocking(kSocketFD))
        .WillOnce(Return(0));
    EXPECT_CALL(sockets_, BindToDevice(kSocketFD, StrEq(kInterfaceName)))
        .WillOnce(Return(0));
     EXPECT_CALL(sockets(), Connect(kSocketFD, _, _))
         .WillOnce(Return(-1));
     EXPECT_CALL(sockets_, Error())
        .WillOnce(Return(EINPROGRESS));
    EXPECT_CALL(dispatcher(), CreateReadyHandler(kSocketFD,
                                                 IOHandler::kModeOutput,
                                                 connect_completion_callback()))
        .WillOnce(ReturnNew<IOHandler>());
    EXPECT_TRUE(async_connection().Start(address_, kConnectPort));
  }

  void OnConnectCompletion(int fd) {
    async_connection_.OnConnectCompletion(fd);
  }
  AsyncConnection &async_connection() { return async_connection_; }
  StrictMock<MockSockets> &sockets() { return sockets_; }
  MockEventDispatcher &dispatcher() { return dispatcher_; }
  const IPAddress &address() { return address_; }
  int fd() { return async_connection_.fd_; }
  void set_fd(int fd) { async_connection_.fd_ = fd; }
  StrictMock<ConnectCallbackTarget> &callback_target() {
    return callback_target_;
  }
  Callback1<int>::Type *connect_completion_callback() {
    return async_connection_.connect_completion_callback_.get();
  }

 private:
  MockEventDispatcher dispatcher_;
  StrictMock<MockSockets> sockets_;
  StrictMock<ConnectCallbackTarget> callback_target_;
  AsyncConnection async_connection_;
  IPAddress address_;
};

TEST_F(AsyncConnectionTest, InitState) {
  ExpectReset();
  EXPECT_EQ(string(), async_connection().error());
}

TEST_F(AsyncConnectionTest, StartSocketFailure) {
  EXPECT_CALL(sockets(), Socket(_, _, _))
      .WillOnce(Return(-1));
  EXPECT_CALL(sockets(), Error())
      .WillOnce(Return(kErrorNumber));
  EXPECT_FALSE(async_connection().Start(address(), kConnectPort));
  ExpectReset();
  EXPECT_STREQ(strerror(kErrorNumber), async_connection().error().c_str());
}

TEST_F(AsyncConnectionTest, StartNonBlockingFailure) {
  EXPECT_CALL(sockets(), Socket(_, _, _))
      .WillOnce(Return(kSocketFD));
  EXPECT_CALL(sockets(), SetNonBlocking(kSocketFD))
      .WillOnce(Return(-1));
  EXPECT_CALL(sockets(), Error())
      .WillOnce(Return(kErrorNumber));
  EXPECT_CALL(sockets(), Close(kSocketFD))
      .WillOnce(Return(0));
  EXPECT_FALSE(async_connection().Start(address(), kConnectPort));
  ExpectReset();
  EXPECT_STREQ(strerror(kErrorNumber), async_connection().error().c_str());
}

TEST_F(AsyncConnectionTest, StartBindToDeviceFailure) {
  EXPECT_CALL(sockets(), Socket(_, _, _))
      .WillOnce(Return(kSocketFD));
  EXPECT_CALL(sockets(), SetNonBlocking(kSocketFD))
      .WillOnce(Return(0));
  EXPECT_CALL(sockets(), BindToDevice(kSocketFD, StrEq(kInterfaceName)))
      .WillOnce(Return(-1));
  EXPECT_CALL(sockets(), Error())
      .WillOnce(Return(kErrorNumber));
  EXPECT_CALL(sockets(), Close(kSocketFD))
      .WillOnce(Return(0));
  EXPECT_FALSE(async_connection().Start(address(), kConnectPort));
  ExpectReset();
  EXPECT_STREQ(strerror(kErrorNumber), async_connection().error().c_str());
}

TEST_F(AsyncConnectionTest, SynchronousFailure) {
  EXPECT_CALL(sockets(), Socket(_, _, _))
      .WillOnce(Return(kSocketFD));
  EXPECT_CALL(sockets(), SetNonBlocking(kSocketFD))
      .WillOnce(Return(0));
  EXPECT_CALL(sockets(), BindToDevice(kSocketFD, StrEq(kInterfaceName)))
      .WillOnce(Return(0));
  EXPECT_CALL(sockets(), Connect(kSocketFD, _, _))
      .WillOnce(Return(-1));
  EXPECT_CALL(sockets(), Error())
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_CALL(sockets(), Close(kSocketFD))
      .WillOnce(Return(0));
  EXPECT_FALSE(async_connection().Start(address(), kConnectPort));
  ExpectReset();
}

MATCHER_P2(IsSocketAddress, address, port, "") {
  const struct sockaddr_in *arg_saddr =
      reinterpret_cast<const struct sockaddr_in *>(arg);
  IPAddress arg_addr(IPAddress::kFamilyIPv4,
                     ByteString(reinterpret_cast<const unsigned char *>(
                         &arg_saddr->sin_addr.s_addr),
                                sizeof(arg_saddr->sin_addr.s_addr)));
  return address.Equals(arg_addr) && arg_saddr->sin_port == htons(port);
}

TEST_F(AsyncConnectionTest, SynchronousStart) {
  EXPECT_CALL(sockets(), Socket(_, _, _))
      .WillOnce(Return(kSocketFD));
  EXPECT_CALL(sockets(), SetNonBlocking(kSocketFD))
      .WillOnce(Return(0));
  EXPECT_CALL(sockets(), BindToDevice(kSocketFD, StrEq(kInterfaceName)))
      .WillOnce(Return(0));
  EXPECT_CALL(sockets(), Connect(kSocketFD,
                                  IsSocketAddress(address(), kConnectPort),
                                  sizeof(struct sockaddr_in)))
      .WillOnce(Return(-1));
  EXPECT_CALL(dispatcher(),
              CreateReadyHandler(kSocketFD,
                                 IOHandler::kModeOutput,
                                 connect_completion_callback()))
        .WillOnce(ReturnNew<IOHandler>());
  EXPECT_CALL(sockets(), Error())
      .WillOnce(Return(EINPROGRESS));
  EXPECT_TRUE(async_connection().Start(address(), kConnectPort));
  EXPECT_EQ(kSocketFD, fd());
}

TEST_F(AsyncConnectionTest, AsynchronousFailure) {
  StartConnection();
  EXPECT_CALL(sockets(), GetSocketError(kSocketFD))
      .WillOnce(Return(1));
  EXPECT_CALL(sockets(), Error())
      .WillOnce(Return(kErrorNumber));
  EXPECT_CALL(callback_target(), CallTarget(false, -1));
  EXPECT_CALL(sockets(), Close(kSocketFD))
      .WillOnce(Return(0));
  OnConnectCompletion(kSocketFD);
  ExpectReset();
  EXPECT_STREQ(strerror(kErrorNumber), async_connection().error().c_str());
}

TEST_F(AsyncConnectionTest, AsynchronousSuccess) {
  StartConnection();
  EXPECT_CALL(sockets(), GetSocketError(kSocketFD))
      .WillOnce(Return(0));
  EXPECT_CALL(callback_target(), CallTarget(true, kSocketFD));
  OnConnectCompletion(kSocketFD);
  ExpectReset();
}

TEST_F(AsyncConnectionTest, SynchronousSuccess) {
  EXPECT_CALL(sockets(), Socket(_, _, _))
      .WillOnce(Return(kSocketFD));
  EXPECT_CALL(sockets(), SetNonBlocking(kSocketFD))
      .WillOnce(Return(0));
  EXPECT_CALL(sockets(), BindToDevice(kSocketFD, StrEq(kInterfaceName)))
      .WillOnce(Return(0));
  EXPECT_CALL(sockets(), Connect(kSocketFD,
                                  IsSocketAddress(address(), kConnectPort),
                                  sizeof(struct sockaddr_in)))
      .WillOnce(Return(0));
  EXPECT_CALL(callback_target(), CallTarget(true, kSocketFD));
  EXPECT_TRUE(async_connection().Start(address(), kConnectPort));
  ExpectReset();
}

}  // namespace shill
