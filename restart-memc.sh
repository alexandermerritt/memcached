#! /usr/bin/env bash
set -e ; set -u

hosts="kid1 kid2 kid3 kid4 kid5 kid6 kid7"
rm -f hostfile
for h in $hosts; do echo $h >> hostfile ; done

args="--inline-stdout -h hostfile"

commands[0]="killall -q -s SIGTERM memcached || true"
commands[1]="~amerritt/documents/research/memcached.git/ssh-run.sh"
for ((i=0; i<${#commands[@]}; i++))
do
    pssh $args "${commands[$i]}"
done

