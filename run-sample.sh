source setup.sh
qsize=100
#duration=30
#down="downlink-4g-no-cross-times"
down="downlink-4g-no-cross-subway.pps"
duration=690
port=44444
tuning_period=500
init_alpha=150

user=`whoami`
anal_script="mm-thr"
target=50
#Unidirectioanl delay: 10ms
delay=10
anal_script="mm-throughput-graph-modified"
cmd="sudo ./client \$MAHIMAHI_BASE 1 $port & sleep $duration && killall client"

downlink=${down}
uplink=wired48
trace_="$uplink-$downlink"
log="$queue-$delay-$trace_-$qsize-$target-$duration"

sudo ./server $port $target $init_alpha $tuning_period &

sudo -u $user mm-delay $delay mm-link traces/$uplink traces/$downlink --uplink-log=log/up-mul-$log --downlink-log=log/down-mul-$log --uplink-queue=droptail --uplink-queue-args=\"packets=$qsize\" --downlink-queue=droptail --downlink-queue-args=\"packets=$qsize\"  -- sh -c "$cmd"

sudo killall server client
echo $log >>log/summary.tr
./$anal_script 500 log/down-mul-$log 1>log/fig-$log.svg 2>>log/summary.tr
echo "---------------------------------------">>log/summary.tr
cat log/summary.tr
firefox log/fig-$log.svg

