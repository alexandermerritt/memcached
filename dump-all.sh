#! /usr/bin/env bash
set -e
set -u
#echo 'set root 0 0 5\n12345\ndump' | telnet localhost 11211

for ip in 192.168.1.221 192.168.1.222
do
    echo 'dump at /scratch/merrital' | netcat -C $ip 11211
done

