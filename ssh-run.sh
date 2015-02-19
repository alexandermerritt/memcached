#! /usr/bin/env bash
set -e
set -u
thisdir=$(cd $(dirname $0) && pwd)
nohup $thisdir/run.sh > /dev/null < /dev/null 2>&1 &
