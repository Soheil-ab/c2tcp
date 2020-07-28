sudo sysctl -q net.ipv4.tcp_wmem="4096 32768 4194304" #Doubling the default value from 16384 to 32768
sudo sysctl -w -q net.ipv4.tcp_low_latency=1
sudo sysctl -w -q net.ipv4.tcp_autocorking=0
sudo sysctl -w -q net.ipv4.tcp_no_metrics_save=1
sudo sysctl -w -q net.ipv4.ip_forward=1

DOWNLINKS=("downlink-4g-no-cross-subway.pps" "downlink-4g-with-cross-subway.pps" "downlink-3g-no-cross-subway.pps" "downlink-4g-no-cross-times" "downlink-4g-with-cross-times" "downlink-3g-with-cross-times-1" "downlink-3g-no-cross-times-1" "ATT-LTE-driving-2016.down" "ATT-LTE-driving.down"  "TMobile-LTE-driving.down" "TMobile-UMTS-driving.down" "Verizon-EVDO-driving.down" "Verizon-LTE-driving.down");
UPLINKS=("wired48" "wired48" "wired48" "wired48" "wired48" "wired48" "wired48" "wired48" "wired48" "wired48" "wired48" "wired48" "wired48" "wired48");
duration=("690" "710" "250" "950" "940" "210" "340" "120" "790" "480" "930" "1065" "1365");

