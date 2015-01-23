// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/third_party_vpn_driver.h"

#include <fcntl.h>
#include <unistd.h>

#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/connection.h"
#include "shill/control_interface.h"
#include "shill/device_info.h"
#include "shill/dhcp_config.h"
#include "shill/error.h"
#include "shill/file_io.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/property_accessor.h"
#include "shill/store_interface.h"
#include "shill/third_party_vpn_dbus_adaptor.h"
#include "shill/virtual_device.h"
#include "shill/vpn_service.h"

namespace shill {


namespace Logging {

static auto kModuleLogScope = ScopeLogger::kVPN;
static std::string ObjectID(const ThirdPartyVpnDriver *v) {
  return "(third_party_vpn_driver)";
}

}  // namespace Logging

namespace {

const int kReconnectOfflineTimeoutSeconds = 2 * 60;
const int32_t kConstantMaxMtu = (1 << 16) - 1;
const char kExtensionIdProperty[] = "ExtensionId";

}  // namespace

const VPNDriver::Property ThirdPartyVpnDriver::kProperties[] = {
  { kProviderHostProperty, 0 },
  { kProviderTypeProperty, 0 }
};

ThirdPartyVpnDriver *ThirdPartyVpnDriver::active_client_ = nullptr;

ThirdPartyVpnDriver::ThirdPartyVpnDriver(ControlInterface *control,
                                         EventDispatcher *dispatcher,
                                         Metrics *metrics, Manager *manager,
                                         DeviceInfo *device_info,
                                         const std::string &name)
    : VPNDriver(dispatcher, manager, kProperties, arraysize(kProperties)),
      control_(control),
      dispatcher_(dispatcher),
      metrics_(metrics),
      device_info_(device_info),
      name_(name),
      tun_fd_(-1),
      default_service_callback_tag_(0),
      parameters_expected_(false) {
  file_io_ = FileIO::GetInstance();
}

ThirdPartyVpnDriver::~ThirdPartyVpnDriver() {
  Cleanup(Service::kStateIdle, Service::kFailureUnknown,
          Service::kErrorDetailsNone);
}

void ThirdPartyVpnDriver::InitPropertyStore(PropertyStore *store) {
  VPNDriver::InitPropertyStore(store);
  store->RegisterDerivedString(
      kExtensionIdProperty,
      StringAccessor(
          new CustomWriteOnlyAccessor<ThirdPartyVpnDriver, std::string>(
              this, &ThirdPartyVpnDriver::SetExtensionId,
              &ThirdPartyVpnDriver::ClearExtensionId, nullptr)));
}

bool ThirdPartyVpnDriver::Load(StoreInterface *storage,
                               const std::string &storage_id) {
  bool return_value = VPNDriver::Load(storage, storage_id);
  if (adaptor_interface_ == nullptr) {
    storage->GetString(storage_id, kExtensionIdProperty, &extension_id_);
    adaptor_interface_.reset(control_->CreateThirdPartyVpnAdaptor(this));
  }
  return return_value;
}

bool ThirdPartyVpnDriver::Save(StoreInterface *storage,
                               const std::string &storage_id,
                               bool save_credentials) {
  bool return_value = VPNDriver::Save(storage, storage_id, save_credentials);
  storage->SetString(storage_id, kExtensionIdProperty, extension_id_);
  return return_value;
}

void ThirdPartyVpnDriver::ClearExtensionId(Error *error) {
  error->Populate(Error::kNotSupported,
                  "Clearing extension id is not supported.");
}

bool ThirdPartyVpnDriver::SetExtensionId(const std::string &value,
                                         Error *error) {
  if (adaptor_interface_ == nullptr) {
    extension_id_ = value;
    adaptor_interface_.reset(control_->CreateThirdPartyVpnAdaptor(this));
    return true;
  }
  error->Populate(Error::kAlreadyExists, "Extension ID is set");
  return false;
}

void ThirdPartyVpnDriver::UpdateConnectionState(
    Service::ConnectState connection_state, std::string *error_message) {
  if (active_client_ != this) {
    error_message->append("Unexpected call");
    return;
  }
  if (service_ && connection_state >= Service::kStateConnected &&
      connection_state <= Service::kStateOnline) {
    service_->SetState(connection_state);
  } else {
    error_message->append("Invalid argument");
  }
}

void ThirdPartyVpnDriver::SendPacket(const std::vector<uint8_t> &ip_packet,
                                     std::string *error_message) {
  if (active_client_ != this) {
    error_message->append("Unexpected call");
    return;
  } else if (tun_fd_ < 0) {
    error_message->append("Device not open");
    return;
  } else if (file_io_->Write(tun_fd_, ip_packet.data(), ip_packet.size()) !=
             static_cast<ssize_t>(ip_packet.size())) {
    error_message->append("Partial write");
    adaptor_interface_->EmitPlatformMessage(
        static_cast<uint32_t>(PlatformMessage::kError));
  }
}

void ThirdPartyVpnDriver::ProcessIp(
    const std::map<std::string, std::string> &parameters, const char *key,
    std::string *target, bool mandatory, std::string *error_message) {
  // TODO(kaliamoorthi): Add IPV6 support.
  auto it = parameters.find(key);
  if (it != parameters.end()) {
    if (IPAddress(parameters.at(key)).family() == IPAddress::kFamilyIPv4) {
      *target = parameters.at(key);
    } else {
      error_message->append(key).append(" is not a valid IP;");
    }
  } else if (mandatory) {
    error_message->append(key).append(" is missing;");
  }
}

void ThirdPartyVpnDriver::ProcessDnsServerArray(
    const std::map<std::string, std::string> &parameters, const char *key,
    char delimiter, std::vector<std::string> *target, bool mandatory,
    std::string *error_message) {
  std::vector<std::string> string_array;
  auto it = parameters.find(key);
  if (it != parameters.end()) {
    base::SplitString(parameters.at(key), delimiter, &string_array);

    // Eliminate invalid IPs
    for (auto value = string_array.begin(); value != string_array.end();) {
      if (IPAddress(*value).family() != IPAddress::kFamilyIPv4) {
        value = string_array.erase(value);
      } else {
        ++value;
      }
    }

    if (!string_array.empty()) {
      target->swap(string_array);
    } else {
      error_message->append(key).append(" has no valid values or is empty;");
    }
  } else if (mandatory) {
    error_message->append(key).append(" is missing;");
  }
}

void ThirdPartyVpnDriver::ProcessSearchDomainArray(
    const std::map<std::string, std::string> &parameters, const char *key,
    char delimiter, std::vector<std::string> *target, bool mandatory,
    std::string *error_message) {
  std::vector<std::string> string_array;
  auto it = parameters.find(key);
  if (it != parameters.end()) {
    base::SplitString(parameters.at(key), delimiter, &string_array);

    if (!string_array.empty()) {
      target->swap(string_array);
    } else {
      error_message->append(key).append(" has no valid values or is empty;");
    }
  } else if (mandatory) {
    error_message->append(key).append(" is missing;");
  }
}

void ThirdPartyVpnDriver::ProcessInt32(
    const std::map<std::string, std::string> &parameters, const char *key,
    int32_t *target, int32_t min_value, int32_t max_value, bool mandatory,
    std::string *error_message) {
  int32_t value = 0;
  auto it = parameters.find(key);
  if (it != parameters.end()) {
    if (base::StringToInt(parameters.at(key), &value) && value >= min_value &&
        value <= max_value) {
      *target = value;
    } else {
      error_message->append(key).append(" not in expected range;");
    }
  } else if (mandatory) {
    error_message->append(key).append(" is missing;");
  }
}

void ThirdPartyVpnDriver::SetParameters(
    const std::map<std::string, std::string> &parameters,
    std::string *error_message) {
  // TODO(kaliamoorthi): Add IPV6 support.
  if (!parameters_expected_ || active_client_ != this) {
    error_message->append("Unexpected call");
    return;
  }

  ProcessIp(parameters, "address", &ip_properties_.address, true,
            error_message);
  ProcessIp(parameters, "broadcast_address", &ip_properties_.broadcast_address,
            false, error_message);
  ProcessIp(parameters, "gateway", &ip_properties_.gateway, false,
            error_message);
  ProcessIp(parameters, "bypass_tunnel_for_ip", &ip_properties_.trusted_ip,
            true, error_message);

  ProcessInt32(parameters, "subnet_prefix", &ip_properties_.subnet_prefix, 0,
               31, true, error_message);
  ProcessInt32(parameters, "mtu", &ip_properties_.mtu, DHCPConfig::kMinMTU,
               kConstantMaxMtu, false, error_message);

  ProcessSearchDomainArray(parameters, "domain_search", ':',
                           &ip_properties_.domain_search, false, error_message);
  ProcessDnsServerArray(parameters, "dns_servers", ' ',
                        &ip_properties_.dns_servers, true, error_message);

  if (error_message->empty()) {
    device_->UpdateIPConfig(ip_properties_);
    StopConnectTimeout();
    parameters_expected_ = false;
  }
}

void ThirdPartyVpnDriver::OnInput(InputData *data) {
  // TODO(kaliamoorthi): This is not efficient, transfer the descriptor over to
  // chrome browser or use a pipe in between. Avoid using DBUS for packet
  // transfer.
  std::vector<uint8_t> ip_packet(data->buf, data->buf + data->len);
  adaptor_interface_->EmitPacketReceived(ip_packet);
}

void ThirdPartyVpnDriver::OnInputError(const std::string &error) {
  LOG(ERROR) << error;
  CHECK_EQ(active_client_, this);
  adaptor_interface_->EmitPlatformMessage(
      static_cast<uint32_t>(PlatformMessage::kError));
}

void ThirdPartyVpnDriver::Cleanup(Service::ConnectState state,
                                  Service::ConnectFailure failure,
                                  const std::string &error_details) {
  SLOG(this, 2) << __func__ << "(" << Service::ConnectStateToString(state)
               << ", " << error_details << ")";
  StopConnectTimeout();
  if (default_service_callback_tag_) {
    manager()->DeregisterDefaultServiceCallback(default_service_callback_tag_);
    default_service_callback_tag_ = 0;
  }
  int interface_index = -1;
  if (device_) {
    interface_index = device_->interface_index();
    device_->DropConnection();
    device_->SetEnabled(false);
    device_ = nullptr;
  }
  if (interface_index >= 0) {
    device_info_->DeleteInterface(interface_index);
  }
  tunnel_interface_.clear();
  if (service_) {
    if (state == Service::kStateFailure) {
      service_->SetErrorDetails(error_details);
      service_->SetFailure(failure);
    } else {
      service_->SetState(state);
    }
    service_ = nullptr;
  }
  if (tun_fd_ > 0) {
    file_io_->Close(tun_fd_);
    tun_fd_ = -1;
  }
  io_handler_.reset();
  if (active_client_ == this) {
    adaptor_interface_->EmitPlatformMessage(
        static_cast<uint32_t>(PlatformMessage::kDisconnected));
    active_client_ = nullptr;
  }
  parameters_expected_ = false;
}

void ThirdPartyVpnDriver::Connect(const VPNServiceRefPtr &service,
                                  Error *error) {
  SLOG(this, 2) << __func__;
  CHECK(adaptor_interface_);
  CHECK(!active_client_);
  StartConnectTimeout(kDefaultConnectTimeoutSeconds);
  ip_properties_ = IPConfig::Properties();
  service_ = service;
  service_->SetState(Service::kStateConfiguring);
  if (!device_info_->CreateTunnelInterface(&tunnel_interface_)) {
    Error::PopulateAndLog(error, Error::kInternalError,
                          "Could not create tunnel interface.");
    Cleanup(Service::kStateFailure, Service::kFailureInternal,
            "Unable to create tun interface");
  }
  // Wait for the ClaimInterface callback to continue the connection process.
}

bool ThirdPartyVpnDriver::ClaimInterface(const std::string &link_name,
                                         int interface_index) {
  if (link_name != tunnel_interface_) {
    return false;
  }
  CHECK(!active_client_);

  SLOG(this, 2) << "Claiming " << link_name << " for OpenVPN tunnel";

  CHECK(!device_);
  device_ = new VirtualDevice(control_, dispatcher(), metrics_, manager(),
                              link_name, interface_index, Technology::kVPN);
  device_->SetEnabled(true);

  tun_fd_ = device_info_->OpenTunnelInterface(tunnel_interface_);
  if (tun_fd_ < 0) {
    Cleanup(Service::kStateFailure, Service::kFailureInternal,
            "Unable to open tun interface");
  } else {
    io_handler_.reset(dispatcher_->CreateInputHandler(
        tun_fd_,
        base::Bind(&ThirdPartyVpnDriver::OnInput, base::Unretained(this)),
        base::Bind(&ThirdPartyVpnDriver::OnInputError,
                   base::Unretained(this))));
    default_service_callback_tag_ = manager()->RegisterDefaultServiceCallback(
        base::Bind(&ThirdPartyVpnDriver::OnDefaultServiceChanged,
                   base::Unretained(this)));
    active_client_ = this;
    parameters_expected_ = true;
    adaptor_interface_->EmitPlatformMessage(
        static_cast<uint32_t>(PlatformMessage::kConnected));
  }
  return true;
}

void ThirdPartyVpnDriver::Disconnect() {
  SLOG(this, 2) << __func__;
  CHECK(adaptor_interface_);
  if (active_client_ == this) {
    Cleanup(Service::kStateIdle, Service::kFailureUnknown,
            Service::kErrorDetailsNone);
  }
}

std::string ThirdPartyVpnDriver::GetProviderType() const {
  return std::string(kProviderThirdPartyVpn);
}

void ThirdPartyVpnDriver::OnConnectionDisconnected() {
  SLOG(this, 2) << __func__;
  CHECK_EQ(active_client_, this);
  adaptor_interface_->EmitPlatformMessage(
      static_cast<uint32_t>(PlatformMessage::kRestartClient));
  // TODO(kaliamoorthi): Figure out the right behavior for third party clients.
  StartConnectTimeout(kReconnectOfflineTimeoutSeconds);
  if (device_) {
    device_->DropConnection();
  }
  if (service_) {
    service_->SetState(Service::kStateAssociating);
  }
}

void ThirdPartyVpnDriver::OnConnectTimeout() {
  SLOG(this, 2) << __func__;
  VPNDriver::OnConnectTimeout();
  Cleanup(Service::kStateFailure, Service::kFailureConnect,
          "Connection timed out");
}

void ThirdPartyVpnDriver::OnDefaultServiceChanged(
    const ServiceRefPtr &service) {
  SLOG(this, 2) << __func__ << "(" << (service ? service->unique_name() : "-")
               << ")";
  CHECK_EQ(active_client_, this);
  // TODO(kaliamoorthi): Figure out the right behavior for third party clients.
  if (service && service != service_ && service->IsConnected()) {
    adaptor_interface_->EmitPlatformMessage(
        static_cast<uint32_t>(PlatformMessage::kNewUnderlyingNetwork));
  } else {
    adaptor_interface_->EmitPlatformMessage(
        static_cast<uint32_t>(PlatformMessage::kDefaultUnderlyingNetwork));
  }
}

}  // namespace shill