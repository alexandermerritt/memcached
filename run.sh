#! /usr/bin/env bash
set -e
set -u

getib() {
    /sbin/ifconfig ib0 | grep 'inet addr' | cut -d ':' -f 2 | cut -d ' ' -f1
}

echo listening on $(getib)

# -m is in MiB
./memcached -f 2 -p 11211 -u memcache -l $(getib) -M -m $((16 * 1024)) $@
