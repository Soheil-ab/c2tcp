#! /bin/bash

# command line
if [ $# -lt 4 ]; then 
  echo "Enter qdisc, egress_interface, ingress_interface client_mac "
  exit;
fi
qdisc=$1
egress=$2
ingress=$3
client_mac=$4
set -x

# Clean up vestiges 
sudo killall -s9 cellsim sproutbt2

# Setup Sprout Relay  first
sudo tunctl -t tap-relay
sudo ifconfig tap-relay 10.0.0.1 netmask 255.255.255.0 up
sudo ifconfig tap-relay mtu 1500
sudo ethtool --offload  tap-relay gso off  tso off gro off ufo off lro off
sudo ethtool --offload  $ingress gso off tso off gro off ufo off lro off

# Setup Sprout Relay
sudo tunctl -t tap-client
sudo ifconfig tap-client 10.0.0.2 netmask 255.255.255.0 up
sudo ifconfig tap-client mtu 1500
sudo ethtool --offload  tap-client gso off  tso off gro off ufo off lro off
sudo ethtool --offload  $egress gso off tso off gro off ufo off lro off

# Allow loopback packets to be received
echo 0 | sudo tee /proc/sys/net/ipv4/conf/all/rp_filter;
echo 1 | sudo tee /proc/sys/net/ipv4/conf/all/accept_local;
echo 1 | sudo tee /proc/sys/net/ipv4/conf/all/log_martians;
echo 0 | sudo tee /proc/sys/net/ipv4/conf/default/rp_filter;
echo 1 | sudo tee /proc/sys/net/ipv4/conf/default/accept_local;
echo 1 | sudo tee /proc/sys/net/ipv4/conf/default/log_martians;
echo 0 | sudo tee /proc/sys/net/ipv4/conf/tap-client/rp_filter;
echo 1 | sudo tee /proc/sys/net/ipv4/conf/tap-client/accept_local;
echo 1 | sudo tee /proc/sys/net/ipv4/conf/tap-client/log_martians;
echo 0 | sudo tee /proc/sys/net/ipv4/conf/tap-relay/rp_filter;
echo 1 | sudo tee /proc/sys/net/ipv4/conf/tap-relay/accept_local;
echo 1 | sudo tee /proc/sys/net/ipv4/conf/tap-relay/log_martians;

# Set mtu of ingress and egress interfaces to 1420 so that 1420+14=1434 is the max. frame size
# Since SSP payloads can't exceed 1440 thats the largest Ethernet Frame we can tunnel across.
sudo ifconfig $ingress mtu 1420
sudo ifconfig $egress mtu 1420

# shuttle packets between tap-relay and egress_interface
sudo xterm -e ~/BeatingSkype/SproutTunnel/alfalfa/src/examples/sproutbt2 $qdisc $egress $client_mac &

# shuttle packets between tap-client and ingress_interface
sudo xterm -e ~/BeatingSkype/SproutTunnel/alfalfa/src/examples/sproutbt2 10.0.0.1 60001 $qdisc $ingress $client_mac &

# Setup cellsim between them
# Get tap-clients MAC address
tap_client_mac=`ifconfig tap-client | grep HWaddr | awk '{print $5}'`

sudo xterm -e ~/BeatingSkype/SproutTunnel/multisend/sender/cellsim-tap ~/BeatingSkype/multisend/sender/pps-examples/1000.pps ~/BeatingSkype/multisend/sender/pps-examples/1000.pps $tap_client_mac -1 tap-relay tap-client &
