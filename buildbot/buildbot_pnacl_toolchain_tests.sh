#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run toolchain torture tests and llvm testsuite tests.
# For now, run on linux64, build and run unsandboxed newlib tests
# for all 3 architectures.
# Note: This script builds the toolchain from scratch but does
#       not build the translators and hence the translators
#       are from an older revision, see comment below.

set -o xtrace
set -o nounset
set -o errexit

# NOTE:
# The pexes which are referred to below, and the pexes generated by the
# archived frontend below will be translated with translators from DEPS.
# The motivation is to ensure that newer translators can still handle
# older pexes.

# The frontend from this rev will generate pexes for the archived frontend
# test. The toolchain downloader expects this information in a specially
# formatted file. We generate that file in this script from this information,
# to keep all our versions in one place
ARCHIVED_TOOLCHAIN_REV=12922

readonly PNACL_BUILD="pnacl/build.sh"
readonly TOOLCHAIN_BUILD="toolchain_build/toolchain_build_pnacl.py"
readonly UP_DOWN_LOAD="buildbot/file_up_down_load.sh"
readonly TORTURE_TEST="tools/toolchain_tester/torture_test.py"
readonly LLVM_TEST="pnacl/scripts/llvm-test.py"

# build.sh, llvm test suite and torture tests all use this value
export PNACL_CONCURRENCY=${PNACL_CONCURRENCY:-4}

# Change the  toolchain build script (PNACL_BUILD) behavior slightly
# wrt to error logging and mecurial retry delays.
# TODO(robertm): if this special casing is still needed,
#                make this into separate vars
export PNACL_BUILDBOT=true
# Make the toolchain build script (PNACL_BUILD) more verbose.
# This will also prevent bot timeouts which otherwise gets triggered
# by long periods without console output.
export PNACL_VERBOSE=true

# For now this script runs on linux x86-64.
# It is possible to force the PNACL_BUILD to build host binaries with "-m32",
# by uncommenting below:
# export BUILD_ARCH="x86_32"
# export HOST_ARCH="x86_32"
# TODO(pnacl-team): Figure out what to do about this.
# Export this so that the test scripts know where to find the toolchain.
export TOOLCHAIN_BASE_DIR=linux_x86
export PNACL_TOOLCHAIN_DIR=${TOOLCHAIN_BASE_DIR}/pnacl_newlib
export PNACL_TOOLCHAIN_LABEL=pnacl_linux_x86
# This picks the TC which we just built, even if scons doesn't know
# how to find a 64-bit host toolchain.
readonly SCONS_PICK_TC="pnacl_newlib_dir=toolchain/${PNACL_TOOLCHAIN_DIR}"

# download-old-tc -
# Download the archived frontend toolchain, if we haven't already
download-old-tc() {
  local dst=$1

  if [[ -f "${dst}/${ARCHIVED_TOOLCHAIN_REV}.stamp" ]]; then
    echo "Using existing tarball for archived frontend"
  else
    mkdir -p "${dst}"
    rm -rf "${dst}/*"
    ${UP_DOWN_LOAD} DownloadPnaclToolchains ${ARCHIVED_TOOLCHAIN_REV} \
      ${PNACL_TOOLCHAIN_LABEL} \
      ${dst}/${PNACL_TOOLCHAIN_LABEL}.tgz
    mkdir -p ${dst}/${PNACL_TOOLCHAIN_LABEL}
    tar xz -C ${dst}/${PNACL_TOOLCHAIN_LABEL} \
      -f ${dst}/${PNACL_TOOLCHAIN_LABEL}.tgz
    touch "${dst}/${ARCHIVED_TOOLCHAIN_REV}.stamp"
  fi
}


clobber() {
  echo @@@BUILD_STEP clobber@@@
  rm -rf scons-out
  # Don't clobber pnacl_translator; these bots currently don't build
  # it, but they use the DEPSed-in version.
  rm -rf toolchain/linux_x86/pnacl_newlib* \
      toolchain/mac_x86/pnacl_newlib* \
      toolchain/win_x86/pnacl_newlib*
}

handle-error() {
  echo "@@@STEP_FAILURE@@@"
}

ignore-error() {
  echo "@==  IGNORING AN ERROR  ==@"
}

#### Support for running arm sbtc tests on this bot, since we have
# less coverage on the main waterfall now:
# http://code.google.com/p/nativeclient/issues/detail?id=2581
readonly SCONS_COMMON="./scons --verbose bitcode=1 -j${PNACL_CONCURRENCY}"
readonly SCONS_COMMON_SLOW="./scons --verbose bitcode=1 -j2"

build-run-prerequisites() {
  local platform=$1
  ${SCONS_COMMON} ${SCONS_PICK_TC} platform=${platform} \
    sel_ldr sel_universal irt_core
}


scons-tests-translator() {
  local platform=$1
  local flags="--mode=opt-host,nacl use_sandboxed_translator=1 \
               platform=${platform} -k"
  local targets="small_tests medium_tests large_tests"

  # ROUND 1: regular builds
  # generate pexes with full parallelism
  echo "@@@BUILD_STEP scons-sb-trans-pexe [${platform}] [${targets}]@@@"
  ${SCONS_COMMON} ${SCONS_PICK_TC} ${flags} ${targets} \
      translate_in_build_step=0 do_not_run_tests=1 || handle-error

  # translate pexes
  echo "@@@BUILD_STEP scons-sb-trans-trans [${platform}] [${targets}]@@@"
  if [[ ${platform} = arm ]] ; then
      # For ARM we use less parallelism to avoid mysterious QEMU crashes.
      # We also force a timeout for translation only.
      export QEMU_PREFIX_HOOK="timeout 120"
      # Run sb translation twice in case we failed to translate some of the
      # pexes.  If there was an error in the first run this shouldn't
      # trigger a buildbot error.  Only the second run can make the bot red.
      ${SCONS_COMMON_SLOW} ${SCONS_PICK_TC} ${flags} ${targets} \
          do_not_run_tests=1 || ignore-error
      ${SCONS_COMMON_SLOW} ${SCONS_PICK_TC} ${flags} ${targets} \
          do_not_run_tests=1 || handle-error
      # Do not use the prefix hook for running actual tests as
      # it will break some of them due to exit code sign inversion.
      unset QEMU_PREFIX_HOOK
  else
      ${SCONS_COMMON} ${SCONS_PICK_TC} ${flags} ${targets} \
          do_not_run_tests=1 || handle-error
  fi
  # finally run the tests
  echo "@@@BUILD_STEP scons-sb-trans-run [${platform}] [${targets}]@@@"
  ${SCONS_COMMON_SLOW} ${SCONS_PICK_TC} ${flags} ${targets} || handle-error

  # ROUND 2: builds with "fast translation"
  flags="${flags} translate_fast=1"
  echo "@@@BUILD_STEP scons-sb-trans-pexe [fast] [${platform}] [${targets}]@@@"
  ${SCONS_COMMON} ${SCONS_PICK_TC} ${flags} ${targets} \
      translate_in_build_step=0 do_not_run_tests=1 || handle-error

  echo "@@@BUILD_STEP scons-sb-trans-trans [fast] [${platform}] [${targets}]@@@"
  if [[ ${platform} = arm ]] ; then
      # For ARM we use less parallelism to avoid mysterious QEMU crashes.
      # We also force a timeout for translation only.
      export QEMU_PREFIX_HOOK="timeout 120"
      # Run sb translation twice in case we failed to translate some of the
      # pexes.  If there was an error in the first run this shouldn't
      # trigger a buildbot error.  Only the second run can make the bot red.
      ${SCONS_COMMON_SLOW} ${SCONS_PICK_TC} ${flags} ${targets} \
          do_not_run_tests=1 || ignore-error
      ${SCONS_COMMON_SLOW} ${SCONS_PICK_TC} ${flags} ${targets} \
          do_not_run_tests=1 || handle-error
      # Do not use the prefix hook for running actual tests as
      # it will break some of them due to exit code sign inversion.
      unset QEMU_PREFIX_HOOK
  else
      ${SCONS_COMMON} ${SCONS_PICK_TC} ${flags} ${targets} \
          do_not_run_tests=1 || handle-error
  fi
  echo "@@@BUILD_STEP scons-sb-trans-run [fast] [${platform}] [${targets}]@@@"
  ${SCONS_COMMON_SLOW} ${SCONS_PICK_TC} ${flags} ${targets} || handle-error
}

scons-tests-x86-64-zero-based-sandbox() {
  echo "@@@BUILD_STEP hello_world (x86-64 zero-based sandbox)@@@"
  local flags="--mode=opt-host,nacl platform=x86-64 \
               x86_64_zero_based_sandbox=1"
  ${SCONS_COMMON} ${SCONS_PICK_TC} ${flags} "run_hello_world_test"
}

# This test is a bitcode stability test, which builds pexes for all the tests
# using an old version of the toolchain frontend, and then translates those
# pexes using the current version of the translator. It's simpler than using
# archived pexes, because archived pexes for old scons tests may not match the
# current scons tests (e.g. if the expected output changes or if a new test
# is added). The only thing that would break this approach is if a new test
# is added that is incompatible with the old frontend. For this case there
# simply needs to be a mechanism to disable that test (which could be as simple
# as using disable_tests here on the scons command line).
# Note: If this test is manually interrupted or killed during the run, the
# toolchain install might end up missing or replaced with the old one.
# To fix, copy the current one from toolchains/current_tc or blow it away
# and re-run gclient runhooks.
archived-frontend-test() {
  local arch=$1
  # Build the IRT with the latest toolchain before building user
  # pexes with the archived toolchain.
  echo "@@@BUILD_STEP archived_frontend [${arch}]\
        rev ${ARCHIVED_TOOLCHAIN_REV} BUILD IRT@@@"
  ${SCONS_COMMON} ${SCONS_PICK_TC} --mode=opt-host,nacl platform=${arch} \
    irt_core || handle-error


  echo "@@@BUILD_STEP archived_frontend [${arch}]\
        rev ${ARCHIVED_TOOLCHAIN_REV} BUILD@@@"
  local targets="small_tests medium_tests large_tests"
  local flags="--mode=opt-host,nacl platform=${arch} \
               translate_in_build_step=0 skip_trusted_tests=1 \
               skip_nonstable_bitcode=1"

  rm -rf scons-out/nacl-${arch}*

  # Get the archived frontend.
  # If the correct cached frontend is in place, the hash will match and the
  # download will be a no-op. Otherwise the downloader will fix it.
  download-old-tc toolchain/${TOOLCHAIN_BASE_DIR}/archived_tc

  # Save the current toolchain.
  mkdir -p toolchain/${TOOLCHAIN_BASE_DIR}/current_tc
  rm -rf toolchain/${TOOLCHAIN_BASE_DIR}/current_tc/*
  mv toolchain/${PNACL_TOOLCHAIN_DIR} \
    toolchain/${TOOLCHAIN_BASE_DIR}/current_tc/${PNACL_TOOLCHAIN_LABEL}

  # Link the old frontend into place. If we select a different toolchain,
  # SCons will attempt to rebuild the IRT.
  ln -s archived_tc/${PNACL_TOOLCHAIN_LABEL} toolchain/${PNACL_TOOLCHAIN_DIR}

  # Build the pexes with the old frontend.
  ${SCONS_COMMON} ${SCONS_PICK_TC} \
    do_not_run_tests=1 ${flags} ${targets} || handle-error

  # Put the current toolchain back in place.
  rm toolchain/${PNACL_TOOLCHAIN_DIR}
  mv toolchain/${TOOLCHAIN_BASE_DIR}/current_tc/${PNACL_TOOLCHAIN_LABEL} \
    toolchain/${PNACL_TOOLCHAIN_DIR}

  # Translate them with the new translator, and run the tests.
  # Use the sandboxed translator, which runs the newer ABI verifier
  # (building w/ the old frontend only runs the old ABI verifier).
  flags="${flags} use_sandboxed_translator=1"
  # The pexes for sandboxed and fast translation tests are identical but scons
  # uses a different directory.
  local pexe_dir="scons-out/nacl-${arch}-pnacl-pexe-clang"
  cp -a ${pexe_dir} "scons-out/nacl-${arch}-pnacl-sbtc-pexe-clang"
  cp -a ${pexe_dir} "scons-out/nacl-${arch}-pnacl-fast-sbtc-pexe-clang"

  echo "@@@BUILD_STEP archived_frontend [${arch}]\
        rev ${ARCHIVED_TOOLCHAIN_REV} RUN@@@"
  local archived_flags="${flags} built_elsewhere=1 toolchain_feature_version=0"
  # For QEMU limit parallelism to avoid flake.
  if [[ ${arch} = arm ]] ; then
    ${SCONS_COMMON_SLOW} ${SCONS_PICK_TC} \
      ${archived_flags} ${targets} || handle-error
    # Also test the fast-translation option
    echo "@@@BUILD_STEP archived_frontend [${arch} translate-fast]\
        rev ${ARCHIVED_TOOLCHAIN_REV} RUN@@@"
    ${SCONS_COMMON_SLOW} ${SCONS_PICK_TC} ${archived_flags} translate_fast=1 \
      ${targets} || handle-error
  else
    ${SCONS_COMMON} ${SCONS_PICK_TC} \
      ${archived_flags} ${targets} || handle-error
    # Also test the fast-translation option
    echo "@@@BUILD_STEP archived_frontend [${arch} translate-fast]\
        rev ${ARCHIVED_TOOLCHAIN_REV} RUN@@@"
    ${SCONS_COMMON} ${SCONS_PICK_TC} ${archived_flags} translate_fast=1 \
      ${targets} || handle-error
  fi
}


tc-test-bot() {
  local archset="$1"
  clobber

  # Only build MIPS stuff on mips bots
  if [[ ${archset} == "mips" ]]; then
    export PNACL_BUILD_MIPS=true
    # Don't run any of the tests yet
    echo "MIPS bot: Only running build, and not tests"
    archset=
  fi

  # Build the un-sandboxed toolchain. The build script outputs its own buildbot
  # annotations.
  # Build and use the 64-bit llvm build, to get 64-bit versions of the build
  # tools such as fpcmp (used for llvm test suite). For some reason it matters
  # that they match the build machine. TODO(dschuff): Is this still necessary?
  ${TOOLCHAIN_BUILD} --verbose --sync --clobber --build-64bit-host \
    --testsuite-sync \
    --install toolchain/linux_x86/pnacl_newlib

  # Linking the tests require additional sdk libraries like libnacl.
  # Do this once and for all early instead of attempting to do it within
  # each test step and having some late test steps rely on early test
  # steps building the prerequisites -- sometimes the early test steps
  # get skipped.
  echo "@@@BUILD_STEP install sdk libraries @@@"
  ${PNACL_BUILD} sdk
  for arch in ${archset}; do
    # Similarly, build the run prerequisites (sel_ldr and the irt) early.
    echo "@@@BUILD_STEP build run prerequisites [${arch}]@@@"
    build-run-prerequisites ${arch}
  done

  # Run the torture tests.
  for arch in ${archset}; do
    if [[ "${arch}" == "x86-32" ]]; then
      # Torture tests on x86-32 are covered by tc-tests-all in
      # buildbot_pnacl.sh.
      continue
    fi
    echo "@@@BUILD_STEP torture_tests $arch @@@"
    ${TORTURE_TEST} pnacl ${arch} --verbose \
      --concurrency=${PNACL_CONCURRENCY} || handle-error
  done


  local optset
  optset[1]="--opt O3f --opt O2b"
  for arch in ${archset}; do
    # Run all appropriate frontend/backend optimization combinations.
    # For now, this means running 2 combinations for x86 since each
    # takes about 20 minutes on the bots, and making a single run
    # elsewhere since e.g. arm takes about 75 minutes.  In a perfect
    # world, all 4 combinations would be run.
    if [[ ${archset} =~ x86 ]]; then
      optset[2]="--opt O3f --opt O0b"
    fi
    for opt in "${optset[@]}"; do
      echo "@@@BUILD_STEP llvm-test-suite ${arch} ${opt} @@@"
      python ${LLVM_TEST} --testsuite-clean
      python ${LLVM_TEST} \
        --testsuite-configure --testsuite-run --testsuite-report \
        --arch ${arch} ${opt} -v -c || handle-error
    done

    echo "@@@BUILD_STEP libcxx-test ${arch} @@@"
    python ${LLVM_TEST} --libcxx-test --arch ${arch} -c || handle-error

    archived-frontend-test ${arch}

    # Note: we do not build the sandboxed translator on this bot
    # because this would add another 20min to the build time.
    # The upshot of this is that we are using the sandboxed
    # toolchain which is currently deps'ed in.
    # There is a small upside here: we will notice that bitcode has
    # changed in a way that is incompatible with older translators.
    # TODO(pnacl-team): rethink this.
    # Note: the tests which use sandboxed translation are at the end,
    # because they can sometimes hang on arm, causing buildbot to kill the
    # script without running any more tests.
    scons-tests-translator ${arch}

    if [[ ${arch} = x86-64 ]] ; then
      scons-tests-x86-64-zero-based-sandbox
    fi

  done
}


if [ $# = 0 ]; then
  # NOTE: this is used for manual testing only
  tc-test-bot "x86-64 x86-32 arm"
else
  "$@"
fi
