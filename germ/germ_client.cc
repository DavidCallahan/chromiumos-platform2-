// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_client.h"

#include <utility>

#include <sysexits.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <psyche/psyche_connection.h>

#include "germ/constants.h"

namespace germ {

void GermClient::ReceiveService(std::unique_ptr<BinderProxy> proxy) {
  LOG(INFO) << "Received service with handle " << proxy->handle();
  proxy_ = std::move(proxy);
  germ_ = protobinder::CreateInterface<IGerm>(proxy_.get());
  callback_.Run();
  Quit();
}

int GermClient::Launch(const std::string& name,
                       const std::vector<std::string>& command_line) {
  callback_ = base::Bind(&GermClient::DoLaunch, weak_ptr_factory_.GetWeakPtr(),
                         name, command_line);
  return Run();
}

void GermClient::DoLaunch(const std::string& name,
                          const std::vector<std::string>& command_line) {
  DCHECK(germ_);
  germ::LaunchRequest request;
  germ::LaunchResponse response;
  request.set_name(name);
  soma::ContainerSpec::Executable* executable =
      request.mutable_spec()->add_executables();
  for (const auto& cmdline_token : command_line) {
    executable->add_command_line(cmdline_token);
  }
  Status status = germ_->Launch(&request, &response);
  if (!status) {
    LOG(ERROR) << "Failed to launch service '" << name << "': " << status;
    return;
  }
  LOG(INFO) << "Launched service '" << name << "' with pid " << response.pid();
}

int GermClient::Terminate(pid_t pid) {
  callback_ =
      base::Bind(&GermClient::DoTerminate, weak_ptr_factory_.GetWeakPtr(), pid);
  return Run();
}

void GermClient::DoTerminate(pid_t pid) {
  DCHECK(germ_);
  TerminateRequest request;
  TerminateResponse response;
  request.set_pid(pid);
  Status status = germ_->Terminate(&request, &response);
  if (!status) {
    LOG(ERROR) << "Failed to terminate service with pid " << pid << ": "
               << status;
  }
}

int GermClient::OnInit() {
  int return_code = PsycheDaemon::OnInit();
  if (return_code != EX_OK)
    return return_code;

  LOG(INFO) << "Requesting service " << kGermServiceName;
  if (!psyche_connection()->GetService(
          kGermServiceName, base::Bind(&GermClient::ReceiveService,
                                       weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Failed to request service " << kGermServiceName
               << " from psyche";
    return EX_UNAVAILABLE;
  }
  return EX_OK;
}

}  // namespace germ