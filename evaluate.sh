if [ $# == 9 ]
then
	sudo sysctl -w -q net.ipv4.ip_forward=1
	sudo sysctl -w -q net.ipv4.tcp_no_metrics_save=1
    ./run.sh $1 $2 $3 $4 $5 $6 $7 $8 $9 > log
else
	echo "usage: $0 c2tcp cubic westwood sprout verus cubic+codel bbr port trace_index"
    echo -e "Trace Array:\n 0:"downlink-4g-no-cross-subway.pps"\n 1:"downlink-4g-with-cross-subway.pps"\n 2:"downlink-3g-no-cross-subway.pps"\n 3:"downlink-4g-no-cross-times"\n 4:"downlink-4g-with-cross-times"\n 5:"downlink-3g-with-cross-times-1"\n 6:"downlink-3g-no-cross-times-1"\n 7:"ATT-LTE-driving-2016.down"\n 8:"ATT-LTE-driving.down"\n  9:"TMobile-LTE-driving.down"\n 10:"TMobile-UMTS-driving.down"\n 11:"Verizon-EVDO-driving.down"\n 12:"Verizon-LTE-driving.down"
"
fi

