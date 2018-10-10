// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2018 Yang Honggang <yanghonggang@umcloud.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */
#include <string>
#include <sys/stat.h>

#include "common/ceph_argparse.h"
#include "common/errno.h"
#include "common/safe_io.h"

#include "common/sctp_crc32.h"
#include "common/crc32c_intel_baseline.h"
#include "common/crc32c_aarch64.h"
#include "common/config.h"
#include "common/strtol.h"

using namespace std;

void usage()
{
  cout << " usage: <file> <offset> <length>" << std::endl;
  exit(1);
}

int main(int argc, const char** argv)
{
  vector<const char*> args;
  argv_to_vec(argc, argv, args);

  string dev_path, off_str, len_str;
  uint64_t off = 0;
  uint64_t len = 0;

  string val;
  ostringstream err;
  for (std::vector<const char*>::iterator i = args.begin(); i != args.end();) {
    if (ceph_argparse_flag(args, i, "-h", "--help", (char*)NULL)) {
      usage();
    } else if (dev_path.empty()) {
      dev_path = *i++;
    } else if (off_str.empty()) {
      off_str = *i++;
      string interr;
      off = strict_strtoll(off_str.c_str(), 16, &interr);
      if (interr.length() > 0) {
        cerr << "error parsing interger value " << interr << std::endl;
        exit(EXIT_FAILURE);
      }
    } else if (len_str.empty()) {
      len_str = *i++;
      string interr;
      len = strict_strtoll(len_str.c_str(), 16, &interr);
      if (interr.length() > 0) {
        cerr << "error parsing interger value " << interr << std::endl;
        exit(EXIT_FAILURE);
      }
    } else {
      usage();
    }
  }

  if (dev_path.empty() || off_str.empty() || len_str.empty()) {
    usage();
  }
  int dev_fd = ::open(dev_path.c_str(), O_RDONLY);
  if (dev_fd < 0) {
    cerr << "error opening " << dev_path << ": " << cpp_strerror(errno)
         << std::endl;
    exit(1);
  }

  bufferptr bptr(len);
  ssize_t got = safe_pread(dev_fd, bptr.c_str(), len, off);
  if ((uint64_t)got != len) {
    cerr << "error reading " << dev_path << ": " << cpp_strerror(errno)
         << std::endl;
    goto out_close;
  }

  {
    bufferlist bl;
    bl.append(bptr);
    cerr << "crc: " << std::hex << bl.crc32c(-1) << std::dec << std::endl;
  }
out_close:
  if (dev_fd >= 0)
    ::close(dev_fd);

  return 0;
}
