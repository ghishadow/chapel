#!/usr/bin/env bash
#
# Configure environment for performance testing. This should be sourced by other
# scripts that wish to make use of the variables set here.

source $(cd $(dirname $0) ; pwd)/common.bash

# It is tempting to use hostname --short, but macs only support the short form
# of the argument.
export CHPL_NIGHTLY_LOGDIR=/data/sea/cascade/chapel/NightlyPerformance/$(hostname -s)
export CHPL_NIGHTLY_STATDIR=$CHPL_NIGHTLY_LOGDIR/Stats
export CHPL_NIGHTLY_CRON_LOGDIR=$CHPL_NIGHTLY_LOGDIR
