#!/bin/bash

if [ $# != 8 ]
then
    echo -e "usage:$0 latency port trace_id it qsize target initial_alpha period"
    echo "$@"
    echo "$#"
    exit
fi
source setup.sh
sudo killall -s9 server client

latency=$1
port=$2
i=$3;
it=$4
qsize=$5
target=$6
initial_alpha=$7
period=$8

x=100
codel=2

source setup.sh

sudo su <<EOF
echo "cubic" > /proc/sys/net/ipv4/tcp_congestion_control
echo "1" > /proc/sys/net/ipv4/tcp_c2tcp_enable
echo "1" > /proc/sys/net/ipv4/tcp_c2tcp_auto
echo "$x" > /proc/sys/net/ipv4/tcp_c2tcp_x
echo "$initial_alpha" > /proc/sys/net/ipv4/tcp_c2tcp_alpha_setpoint
echo "$initial_alpha" > /proc/sys/net/ipv4/tcp_c2tcp_alpha_interval
EOF

echo "Running C2TCP: ${DOWNLINKS[$i]}" >&2

sudo ./server $latency $port ${DOWNLINKS[$i]} ${UPLINKS[$i]} c2tcp-${DOWNLINKS[$i]}-${UPLINKS[$i]}-$latency-${qsize}-${target}-${initial_alpha}-${period}-${it} $target $initial_alpha ${qsize} ${period} &
sleep 4;echo "will be done in ${duration[$i]} seconds ..." >&2

sleep ${duration[$i]}
sudo killall server mm-link client
echo "Finished." >&2


echo "Doing Analysis ..." >&2


./mm-throughput-graph-modified $((latency*100)) down-c2tcp-${DOWNLINKS[$i]}-${UPLINKS[$i]}-$latency-${qsize}-${target}-${initial_alpha}-${period}-${it} 1>down-c2tcp-${DOWNLINKS[$i]}-${UPLINKS[$i]}-$latency-${qsize}-${target}-${initial_alpha}-${period}-${it}-smoothed.html
echo "Done" >&2


sudo su <<EOF
echo "0" > /proc/sys/net/ipv4/tcp_c2tcp_enable
EOF

