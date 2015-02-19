#! /usr/bin/env bash
set -e
set -u

thisdir=$(cd $(dirname $0) && pwd)

getib() {
    /sbin/ifconfig ib0 | grep 'inet addr' | cut -d ':' -f 2 | cut -d ' ' -f1
}

# -m is in MiB
$thisdir/memcached \
    -f 2.5 \
    -p 11211 \
    -l $(getib) \
    -M \
    -m $((16 * 1024)) \
    $@

