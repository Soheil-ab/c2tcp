# C2TCP v2.0

C2TCP v2.2: A Flexible Cellular TCP to Meet Stringent Delay Requirements.

()To see the IEEE JSAC'19 release, please checkout branch named v2.0-jsac-19. Also, the source code of C2TCP.v1.0 {https://doi.org/10.23919/IFIPNetworking.2018.8696844 , https://arxiv.org/abs/1807.02689 } published in IFIP Networking Confernece, May 2018, can be accessed on <https://github.com/Soheil-ab/C2TCP-IFIP>)

Installation Guide
==================

Here we will provide you with detailed instructions to test C2TCP over Mahimahi.

### Getting the Source Code:

Note: C2TCP is implemented on Linux kernel 4.13.1. 

Get the source code:

	cd ~
    git clone https://github.com/Soheil-ab/c2tcp.git
	cd c2tcp

### Installing Required Tools

General Note: Installing any of the following tools/schemes, depending on your machine, might require other libraries (in addition to what have been mentioned here). So, if you get any errors mentioning that something not being found when you try to `make`, install them using `apt`.

1. Install Mahimahi (http://mahimahi.mit.edu/#getting)

	```sh  
	cd ~/c2tcp/
	sudo apt-get install build-essential git debhelper autotools-dev dh-autoreconf iptables protobuf-compiler libprotobuf-dev pkg-config libssl-dev dnsmasq-base ssl-cert libxcb-present-dev libcairo2-dev libpango1.0-dev iproute2 apache2-dev apache2-bin iptables dnsmasq-base gnuplot iproute2 apache2-api-20120211 libwww-perl
	git clone https://github.com/ravinet/mahimahi 
	cd mahimahi
	./autogen.sh && ./configure && make
	sudo make install
	sudo sysctl -w net.ipv4.ip_forward=1
	```

2. Install iperf3

	```sh
    sudo apt-get remove iperf3 libiperf0
    wget https://iperf.fr/download/ubuntu/libiperf0_3.1.3-1_amd64.deb
    wget https://iperf.fr/download/ubuntu/iperf3_3.1.3-1_amd64.deb
    sudo dpkg -i libiperf0_3.1.3-1_amd64.deb iperf3_3.1.3-1_amd64.deb
    rm libiperf0_3.1.3-1_amd64.deb iperf3_3.1.3-1_amd64.deb
	```

### Installing Other Schemes 

BBR, C2TCP, Westwood, and Cubic are already part of the the current Kernel patch.

### Patching C2TCP Kernel: Install the prepared debian packages.

Simply install the debian packages of the patched kernel:

    cd ~/c2tcp/linux-patch
    sudo dpkg -i linux-image*521*
    sudo dpkg -i linux-header*521*
    sudo reboot 

This Kernel version includes DeepCC and Orca schemes too. 
Orca: Soheil Abbasloo, et. al. "Classic Meets Modern: A Pragmatic Learning-Based COngestion COntrol for the Internet", In proc. ACM SIGCOMM 2020 https://dl.acm.org/doi/abs/10.1145/3387514.3405892
DeepCC: Soheil Abbasloo, et. al. "Wanna Make Your TCP Scheme Great for Cellular Networks? Let Machines Do It for You!", IEEE JSAC 2021 https://ieeexplore.ieee.org/document/9252929 

Please note that if you're using v2.0-jsac-19 branch, you need to switch back to the kernel named c2tcp-v2.01, becasue compared to v2.0-jsac-19 kernel, some socket option name/features are changed in the new kernel.

### Patching C2TCP Kernel: Compile the Kernel source code.

Another option is to compile your own kernel using the provided patch. You can use the instructions provided here to do that: https://github.com/Soheil-ab/C2TCP-IFIP/

The source code is available in linux-patch folder.

### Verify the new kernel

	uname -r

You should see something like 4.13.1-*0521*. If not follow the instructions on https://github.com/Soheil-ab/C2TCP-IFIP and bring the 4.13.1-*0521* image on top of the grub list. For instance, you can use grub-customizer application.
	
Check whether C2TCP is there:
	

	sysctl net.ipv4.tcp_c2tcp_enable

	
You should see:
	
	net.ipv4.tcp_c2tcp_enable = 0
	
We will later enable C2TCP during our evaluation (In this version, in contrast with v2.0-jsac-19 branch, you can enable/disable C2TCP per socket. Still you can use net.ipv4.tcp_c2tcp_enable to enable C2TCP for the entire system though.)

In the current implementation, C2TCP's Tuner has been implemented in user-space. So, lets build C2TCP's Server-client app which includes The Tuner.  

### Build C2TCP's Server-Client App
Simply run the following:

    cd ~/c2tcp/
    ./build.sh

### Install our Cellular Traces gathered in NYC
You need to get the traces from following link and copy them in Mahimahi's folder

    git clone https://github.com/Soheil-ab/Cellular-Traces-2018.git    
    sudo cp Cell*/* traces/

### Running The Evaluation

For the simplicity, first we disable password-timeout of sudo command:

	sudo visudo

Now add following line and save it:

	Defaults    timestamp_timeout=-1	

The Version includes standalone server-client applications (in contrast with v2.0-jsac-19 branch which had hardcoded mahimahi into the server.)  
A minimal set of commands to run an evaluation and generate the results is put in run-sample.sh script. 
Simply run it as:

    cd ~/c2tcp/
	./run-sample.sh

If everything goes well, you should see a figure representing throughput through time. You can also find the summary of the results in log/summary.tr file.

#### No Target!
When you set target=0 (in run-sample.sh), system will switch to a mode where it sets the Target delay automatically to 2xminRTT. It is usefull in a more general scenario in which applcations might not have any specific delay target.

