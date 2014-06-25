// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/constants.h"
#include "common/util.h"
#include "http_server/server.h"
#include "http_server/connection_delegate.h"

#include <iostream>
#include <string>
#include <cctype>
#include <cinttypes>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/files/file_path.h>

using std::cerr;
using std::cout;
using std::endl;
using std::ostream;
using std::string;

using base::FilePath;

static void Usage(ostream& ostream) {
  ostream
    << "Usage:\n"
    << "  p2p-http-server [OPTION..]\n"
    << "\n"
    << "Options:\n"
    << " --help           Show help options\n"
    << " --directory=DIR  Directory to serve from (default: .)\n"
    << " --port=PORT      TCP port number to listen on (default: 16725)\n"
    << " -v=NUMBER        Verbosity level (default: 0)\n"
    << "\n";
}

int main(int argc, char* argv[]) {
  int ret = 1;

  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging_settings.lock_log = logging::LOCK_LOG_FILE;
  logging_settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;
  logging::InitLogging(logging_settings);
  p2p::util::SetupSyslog(p2p::constants::kHttpServerBinaryName,
                         false /* include_pid */);

  LOG(INFO) << p2p::constants::kHttpServerBinaryName
            << " " << PACKAGE_VERSION << " starting";

  if (cl->HasSwitch("help")) {
    Usage(cout);
    return 0;
  }

  uint16_t port = p2p::constants::kHttpServerDefaultPort;
  string port_str = cl->GetSwitchValueNative("port");
  if (port_str.size() > 0) {
    char* endp;
    port = strtol(port_str.c_str(), &endp, 0);
    if (*endp != '\0') {
      cerr << "Error parsing `" << port_str << "' as port number" << endl;
      return 1;
    }
  }

  FilePath directory = cl->GetSwitchValuePath("directory");
  if (directory.empty()) {
    directory = FilePath(FilePath::kCurrentDirectory);
  }

  p2p::http_server::Server server(
      directory, port, STDOUT_FILENO,
      p2p::http_server::ConnectionDelegate::Construct);
  LOG(INFO) << "Maximum download rate per connection set to "
            << p2p::constants::kMaxSpeedPerDownload << " bytes/sec";
  server.SetMaxDownloadRate(p2p::constants::kMaxSpeedPerDownload);
  server.Start();

  GMainLoop* loop = g_main_loop_new(NULL, FALSE);

  // TODO(zeuthen): Now that we've opened all the files and sockets
  // that we need, install a seccomp filter to only allow the very
  // limited set of syscalls we need onwards. See
  //
  //  http://outflux.net/teach-seccomp/
  //
  // for more information.
  //
  // This issue is currently tracked in
  //
  //  https://code.google.com/p/chromium/issues/detail?id=243406

  g_main_loop_run(loop);
  g_main_loop_unref(loop);

  server.Stop();

  return ret;
}
