# C2TCP v2.0

C2TCP v2.0: A Flexible Cellular TCP to Meet Stringent Delay Requirements {<https://arxiv.org/abs/1810.13241v2,https://ieeexplore.ieee.org/document/8638955>}.

(The source code of the earlier version of C2TCP {<https://bit.ly/2G0SP9a>} published in IFIP Networking Confernece, May 2018, can be accessed on <https://github.com/Soheil-ab/C2TCP-IFIP>)

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

BBR, C2TCP, Westwood, and Cubic are already part of the patch. To install Sprout and Verus follow the following instructions: 

1. Build Sprout (http://alfalfa.mit.edu/)

	```sh  
	sudo apt-get install libboost-math-dev libboost-math1.54.0 libprotobuf8 libprotobuf-dev 
	cd ~/c2tcp/alfalfa
	./autogen.sh
	./configure --enable-examples && make	
	```

2. Build Verus (https://github.com/yzaki/verus)

	Required packages: libtbb libasio libalglib libboost-system

	```sh
	sudo apt-get install build-essential autoconf libtbb-dev libasio-dev libalglib-dev libboost-system-dev
	cd ~/c2tcp/verus
	autoreconf -i
	./configure && make
	```

### Patching C2TCP Kernel: Install the prepared debian packages.

Simply install the debian packages of the patched kernel:

    cd ~/c2tcp/linux-patch
    sudo dpkg -i linux-image*
    sudo dpkg -i linux-header*
    sudo reboot 
 
### Verify the new kernel

	uname -r

You should see something like 4.13.1-c2tcp.v2.01. If not follow the instructions on https://github.com/Soheil-ab/C2TCP-IFIP and bring the 4.13.1-c2tcp.v2.01 image on top of the grub list. For instance, you can use grub-customizer application.
	
Check whether C2TCP is there:
	

	sysctl net.ipv4.tcp_c2tcp_enable

	
You should see:
	
	net.ipv4.tcp_c2tcp_enable = 0
	
We will later enable C2TCP during our evaluation

In the current implementation, C2TCP's Tuner has been implemented in user-space. So, lets build C2TCP's Server-client app which includes The Tuner.  

### Build C2TCP's Server-Client App
Simply run the following:

    cd ~/c2tcp/
    ./build.sh

### Install our Cellular Traces gathered in NYC
You need to get the traces from following link and copy them in Mahimahi's folder

    git clone https://github.com/Soheil-ab/Cellular-Traces-2018.git    
    sudo cp Cell*/* /usr/local/share/maimahi/traces/

### Running The Evaluation

For the simplicity, first we disable password-timeout of sudo command:

	sudo visudo

Now add following line and save it:

	Defaults    timestamp_timeout=-1	
	
We have put required commands to run evaluation and generate the results for differnet schemes in one script.
Here, we run C2TCP (with Application Target of 50ms), Cubic, BBR, TCP Westwood using the ATT-LTE-2016 trace file with following command:
(For more information on how to use the script, please check comments in "run.sh" and "evaluate.sh" scripts)

	cd ~/c2tcp/
	./evaluate.sh 1 1 1 0 0 0 1 5000 7

If everything goes fine, you should see the results.
