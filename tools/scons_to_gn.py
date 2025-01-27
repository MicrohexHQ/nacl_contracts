#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import optparse
import os
import sys

import scons_to_gn

TOOLS_DIR = os.path.dirname(__file__)

"""Convert from Scons to GN.

Takes a set of SCONS input files and generates a output file per input, as
well as a global alias file.
"""


def RunTests():
  test_path = os.path.abspath(os.path.join(TOOLS_DIR, 'scons_to_gn'))
  fail_count = 0
  for filename in os.listdir(test_path):
    if fnmatch.fnmatch(filename, '*_test.py'):
      print 'Testing: ' + filename
      filepath = os.path.join(test_path, filename)
      val = os.system(sys.executable + ' ' + filepath)
      if val:
        print 'FAILED: ' + filename
        fail_count += 1
      print '\n'


def main(argv):
  usage = 'usage: %prog [options] <scons files> ...'
  parser = optparse.OptionParser(usage)
  parser.add_option('-v', '--verbose', dest='verbose',
      action='store_true', default=False)
  parser.add_option('-t', '--test', dest='test',
      action='store_true', default=False)

  options, args = parser.parse_args(argv)
  if options.test:
    RunTests()
    return 0

  if len(args) == 0:
    parser.error('Expecting list of sources.')

  for name in args:
    tracker = scons_to_gn.ObjectTracker(name)
    tracker.Dump(sys.stdout)
  return 0


if __name__ == '__main__':
  retval = main(sys.argv[1:])
  sys.exit(retval)
