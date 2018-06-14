# you need to run sudo ./bandwidth.sh RATE_in_Mbps e.g.,sudo ./bandwidth.sh 10 (for 10Mbps)

sudo tc qdisc del dev eth0 root
sudo tc qdisc add dev eth0 handle 1: root htb default 11
sudo tc class add dev eth0 parent 1: classid 1:1 htb rate $1mbit
sudo tc class add dev eth0 parent 1:1 classid 1:11 htb rate $1mbit
