#!/bin/bash

source setup.sh
#sudo killall verus_server sprout*

c2tcp=$1;
cubic=$2;
westwood=$3;
sprout=$4;
verus=$5;
tcp_codel=$6
bbr=$7
port=${8}
i=$9                  #trace_file_index

latency=10
target=50
initial_alpha=150    # it is scaled ==> Actual Initial_alpha=1.5
tuning_period=500     #500ms

if [ ${c2tcp} -eq 1 ]
then
 ./run-c2tcp.sh $latency $port $i test 100 $target $initial_alpha $tuning_period
 echo "-------------------------------------------------------------" >&2
fi

if [ $cubic -eq 1 ]
then
  echo "TCP CUBIC: ${DOWNLINKS[$i]}" >&2
  sudo modprobe tcp_cubic
  sudo su <<EOF
echo "cubic" > /proc/sys/net/ipv4/tcp_congestion_control
echo "0" > /proc/sys/net/ipv4/tcp_c2tcp_enable
EOF
 ./run-iperf ${DOWNLINKS[$i]} ${UPLINKS[$i]} cubic-${DOWNLINKS[$i]} $((port+10+2*i)) $latency ${duration[$i]} 2>>log &
  sleep ${duration[$i]}
  killall iperf3 iperf mm-link
  logfile="cubic-${DOWNLINKS[$i]}"
  out="up-$logfile-$latency-${duration[$i]}"
  echo "Results ..." >&2
  ./mm-throughput-graph-modified 500 $out 1>tmp
  echo "-------------------------------------------------------------" >&2
fi

if [ $bbr -eq 1 ]
then
  echo "BBR: ${DOWNLINKS[$i]}" >&2
  sudo su <<EOF
echo "bbr" > /proc/sys/net/ipv4/tcp_congestion_control
EOF
    ./run-iperf ${DOWNLINKS[$i]} ${UPLINKS[$i]} bbr-${DOWNLINKS[$i]} $((port+20+2*i)) $latency ${duration[$i]} 2>>log &
   sleep ${duration[$i]}
   killall iperf3 iperf mm-link
   logfile="bbr-${DOWNLINKS[$i]}"
   out="up-$logfile-$latency-${duration[$i]}"
   echo "Results ..." >&2

   ./mm-throughput-graph-modified 500 $out 1>tmp
   echo "-------------------------------------------------------------" >&2
fi

if [ $verus -eq 1 ]
then
  echo "VERUS: ${DOWNLINKS[$i]}" >&2
  ./run-verus ${DOWNLINKS[$i]} ${UPLINKS[$i]} verus-${DOWNLINKS[$i]} $((port+30+2*i)) $latency ${duration[$i]} 2>>log &
  sleep ${duration[$i]}
  killall iperf3 iperf mm-link
  logfile="verus-${DOWNLINKS[$i]}"
  out="down-$logfile-$latency-${duration[$i]}"
  echo "Results ..." >&2
  ./mm-throughput-graph-modified 500 $out 1>tmp
  echo "-------------------------------------------------------------" >&2
fi

if [ $sprout -eq 1 ]
then
  echo "SPROUT: ${DOWNLINKS[$i]}" >&2
  ./run-sprout ${DOWNLINKS[$i]} ${UPLINKS[$i]} sprout-${DOWNLINKS[$i]} $((port+40+2*i)) $latency 2>>log &
  sleep ${duration[$i]}
  killall iperf3 iperf mm-link
    logfile="sprout-${DOWNLINKS[$i]}"
    out="up-$logfile-$latency"
   echo "Results ..." >&2
   ./mm-throughput-graph-modified 500 $out 1>tmp
   echo "-------------------------------------------------------------" >&2
fi

if [ $tcp_codel -eq 1 ]
then
  echo "CUBIC+CODEL: ${DOWNLINKS[$i]}" >&2
  sudo modprobe tcp_cubic
  sudo su <<EOF
echo "cubic" > /proc/sys/net/ipv4/tcp_congestion_control
echo "0" > /proc/sys/net/ipv4/tcp_c2tcp_enable
EOF
 ./run-iperf-codel ${DOWNLINKS[$i]} ${UPLINKS[$i]} codel-${DOWNLINKS[$i]} $((port+50+2*i)) $latency ${duration[$i]} 2>>log &
  sleep ${duration[$i]}
  killall iperf3 iperf mm-link
  logfile="codel-${DOWNLINKS[$i]}"
  out="up-$logfile-$latency-${duration[$i]}"
  echo "Results ..." >&2
  ./mm-throughput-graph-modified 500 $out 1>tmp
  echo "-------------------------------------------------------------" >&2
fi

if [ $westwood -eq 1 ]
then
  echo "TCP Westwood: ${DOWNLINKS[$i]}" >&2
  sudo modprobe tcp_westwood
  sudo su <<EOF
 echo "westwood" > /proc/sys/net/ipv4/tcp_congestion_control
EOF
    ./run-iperf ${DOWNLINKS[$i]} ${UPLINKS[$i]} westwood-${DOWNLINKS[$i]} $((port+60+2*i)) $latency ${duration[$i]} 2>>log &
    sleep ${duration[$i]}
    killall iperf3 iperf mm-link
    logfile="westwood-${DOWNLINKS[$i]}"
    out="up-$logfile-$latency-${duration[$i]}"
    echo "Results ..." >&2
    ./mm-throughput-graph-modified 500 $out 1>tmp
    echo "-------------------------------------------------------------" >&2
fi

