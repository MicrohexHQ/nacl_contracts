/*
 * Copyright 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub routine for `tcflow' for porting support.
 */

#include <errno.h>

int tcflow(int fd, int action) {
  errno = ENOSYS;
  return -1;
}
