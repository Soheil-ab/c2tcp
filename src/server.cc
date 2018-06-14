//============================================================================
// Author      : Soheil Abbasloo (ab.soheil@nyu.edu)
// Version     : V2.0
//============================================================================

/*
  MIT License
  Copyright (c) 2017 Soheil Abbasloo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "flow.h"
#include <pthread.h>
#include <sched.h>
#include <sys/types.h>        // needed for socket(), uint8_t, uint16_t, uint32_t
pthread_mutex_t lockit;
#include <unistd.h>
#include <math.h>
#include <time.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

typedef unsigned int u32;
typedef int s32;
struct timeval tv_start,tv_start2;	//Start time (after three way handshake)
struct timeval tv_stamped,tv_stamped2;
uint64_t start_of_client;
struct timeval tv_end,tv_end2;		//End time
bool done=false;
bool check=false;
uint64_t setup_time;  //Flow Completion Time
char *downlink;
char *uplink;
char *log_file;
int it=0;
bool got_message=0;
int qsize=100; //pkts
int flow_index=0;
int target_ratio=150;
u32 target=50; //50ms
u32 report_period=5;//5s
#define FLOW_NUM 1
int sock[FLOW_NUM];
int sock_for_cnt[FLOW_NUM];

struct tcp_c2tcp_info {
    u32 c2tcp_min_rtt;      /* min-filtered RTT in uSec */
    u32 c2tcp_avg_urtt;     /* averaged RTT in uSec from the previous info request till now*/
    u32 c2tcp_cnt;          /* number of RTT samples uSed for averaging */
}tcp_info;

struct sTrace
{
    double time;
    double bw;
    double minRtt;
};
struct sInfo
{
    sTrace *trace;
    int sock;
    int num_lines;
};
int delay_ms;
int client_port;
sTrace *trace;

#define DBGSERVER  0

#define TARGET_RATIO_MIN 100
#define TARGET_RATIO_MAX 1000

/*C2TCP*/
#define  TCP_C2TCP_ENABLE 50
#define  TCP_C2TCP_ALPHA_INTERVAL 51
#define  TCP_C2TCP_ALPHA_SETPOINT 52
#define  TCP_C2TCP_ALPHA 53
#define  TCP_C2TCP_X 54


uint64_t raw_timestamp( void )
{
    struct timespec ts;
    clock_gettime( CLOCK_REALTIME, &ts );
    uint64_t us = ts.tv_nsec / 1000;
    us += (uint64_t)ts.tv_sec * 1000000;
    return us;
}
uint64_t timestamp_begin(bool set)
{
        static uint64_t start;
        if(set)
            start = raw_timestamp();
        return start;
}
uint64_t timestamp_end( void )
{
        return raw_timestamp() - timestamp_begin(0);
}

uint64_t initial_timestamp( void )
{
        static uint64_t initial_value = raw_timestamp();
        return initial_value;
}

uint64_t timestamp( void )
{
        return raw_timestamp() - initial_timestamp();
}

//Start server
void start_server(int flow_num, int client_port);

//thread functions
void* DataThread(void*);
void* CntThread(void*);

//Print usage information
void usage();

int get_info(int sk, struct tcp_c2tcp_info *info)
{
	int tcp_info_length = sizeof(*info);

    return getsockopt( sk, SOL_TCP, TCP_CC_INFO, (void *)info, (socklen_t *)&tcp_info_length );
};

void handler(int sig) {
	void *array[10];
	size_t size;
	DBGMARK(DBGSERVER,2,"=============================================================== Start\n");
	size = backtrace(array, 20);
	fprintf(stderr, "We got signal %d:\n", sig);
	DBGMARK(DBGSERVER,2,"=============================================================== End\n");
	exit(1);
}

int main(int argc, char **argv)
{
    DBGPRINT(DBGSERVER,4,"Main\n");
    if(argc!=10)
	{
        DBGERROR("argc:%d\n",argc);
        for(int i=0;i<argc;i++)
        	DBGERROR("argv[%d]:%s\n",i,argv[i]);
		usage();
		return 0;
	}

	signal(SIGSEGV, handler);   // install our handler
	signal(SIGTERM, handler);   // install our handler
	signal(SIGABRT, handler);   // install our handler
	signal(SIGFPE, handler);   // install our handler
    signal(SIGKILL,handler);   // install our handler
    int flow_num;
	const char* cnt_ip;
    cnt_ip="10.10.10.10";
	int cnt_port;

	bool qplus_enable;
	flow_num=FLOW_NUM;
	//atoi(argv[1]);
	delay_ms=atoi(argv[1]);
	client_port=atoi(argv[2]);
	qplus_enable=0;
    downlink=argv[3];
    uplink=argv[4];
    log_file=argv[5];
    target_ratio=atoi(argv[7]);
    target=atoi(argv[6]);
    qsize=atoi(argv[8]);
    report_period=atoi(argv[9]);
	start_server(flow_num, client_port);
	DBGMARK(DBGSERVER,5,"DONE!\n");
	return 0;
}

void usage()
{
	DBGMARK(0,0,"./server [Delay(ms)] [port] [DL-trace] [UP-trace] [log] [Target] [Initial Alpha] [qsize in pkts] [Report Period: 1 sec]\n");
}

void start_server(int flow_num, int client_port)
{
	cFlow *flows;
    int num_lines=0;
	bool qplus_enable=0;
	FILE* filep;
	sInfo *info;
	info = new sInfo;
	char line[4096];
	int msec = 0, trigger = 10; /* 10ms */
	clock_t before = clock();
	flows = new cFlow[flow_num];

	int cnt_port=PORT_CTR;
	if(flows==NULL)
	{
		DBGMARK(0,0,"flow generation failed\n");
		return;
	}

	//threads
	pthread_t data_thread;
	pthread_t cnt_thread;

	//Server address
	struct sockaddr_in server_addr[FLOW_NUM];
	//Client address
	struct sockaddr_in client_addr[FLOW_NUM];
	//Controller address
	struct sockaddr_in ctr_addr;
    for(int i=0;i<FLOW_NUM;i++)
    {
        memset(&server_addr[i],0,sizeof(server_addr[i]));
        //IP protocol
        server_addr[i].sin_family=AF_INET;
        //Listen on "0.0.0.0" (Any IP address of this host)
        server_addr[i].sin_addr.s_addr=INADDR_ANY;
        //Specify port number
        server_addr[i].sin_port=htons(client_port+i);

        //Init socket
        if((sock[i]=socket(PF_INET,SOCK_STREAM,0))<0)
        {
            DBGMARK(0,0,"sockopt: %s\n",strerror(errno));
            return;
        }

        int reuse = 1;
        if (setsockopt(sock[i], SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
            perror("setsockopt(SO_REUSEADDR) failed");
        //Bind socket on IP:Port
        if(bind(sock[i],(struct sockaddr *)&server_addr[i],sizeof(struct sockaddr))<0)
        {
            DBGMARK(0,0,"bind error srv_ctr_ip: 000000: %s\n",strerror(errno));
            close(sock[i]);
            return;
        }
    }
    char container_cmd[500];
    sprintf(container_cmd,"sudo ./client $MAHIMAHI_BASE 1 2700 100.64.0.4 %d",client_port);
    for(int i=1;i<FLOW_NUM;i++)
    {
    	sprintf(container_cmd,"%s & sleep 5;sudo ./client $MAHIMAHI_BASE 1 2700 100.64.0.4 %d",container_cmd,client_port+i);
    }
    char cmd[1000];
    char final_cmd[1000];
    sprintf(cmd, "sudo -u `logname` mm-delay %d mm-link /usr/local/share/mahimahi/traces/%s /usr/local/share/mahimahi/traces/%s --once --uplink-log=up-%s --downlink-log=down-%s --uplink-queue=droptail --uplink-queue-args=\"packets=%d\" --downlink-queue=droptail --downlink-queue-args=\"packets=%d\" -- sh -c \'%s\' &",delay_ms,uplink,downlink,log_file,log_file,qsize,qsize,container_cmd);
    //sprintf(final_cmd,"sudo tcpdump -i any -w server-%s.pcap & %s",log_file,cmd);
    sprintf(final_cmd,"%s",cmd);

    //DBGPRINT(DBGSERVER,0,"%s\n",final_cmd);
    info->trace=trace;
    info->num_lines=num_lines;
    initial_timestamp();
    system(final_cmd);

    //Start listen
	//The maximum number of concurrent connections is 10
	for(int i=0;i<FLOW_NUM;i++)
    {
        listen(sock[i],10);
    }
	int sin_size=sizeof(struct sockaddr_in);
	while(flow_index<flow_num)
	{
		int value=accept(sock[flow_index],(struct sockaddr *)&client_addr[flow_index],(socklen_t*)&sin_size);
		if(value<0)
		{
			perror("accept error\n");
			DBGMARK(0,0,"sockopt: %s\n",strerror(errno));
			DBGMARK(0,0,"sock::%d, index:%d\n",sock[flow_index],flow_index);
            close(sock[flow_index]);
			return;
		}
		sock_for_cnt[flow_index]=value;
		flows[flow_index].flowinfo.sock=value;
		flows[flow_index].dst_addr=client_addr[flow_index];
		if(pthread_create(&data_thread, NULL , DataThread, (void*)&flows[flow_index]) < 0)
		{
			perror("could not create thread\n");
			close(sock[flow_index]);
			return;
		}
            if (flow_index==0)
            {
                if(pthread_create(&cnt_thread, NULL , CntThread, (void*)info) < 0)
                {
                    perror("could not create control thread\n");
                    close(sock[flow_index]);
                    return;
                }
            }
        fprintf(stderr,"Server is Connected to the client...\n");
            flow_index++;
	}
    pthread_join(data_thread, NULL);
}
void* CntThread(void* information)
{
	struct sched_param param;
    param.__sched_priority=sched_get_priority_max(SCHED_RR);
    int policy=SCHED_RR;
    int s = pthread_setschedparam(pthread_self(), policy, &param);
    if (s!=0)
    {
        DBGERROR("Cannot set priority (%d) for the Main: %s\n",param.__sched_priority,strerror(errno));
    }

    s = pthread_getschedparam(pthread_self(),&policy,&param);
    if (s!=0)
    {
        DBGERROR("Cannot get priority for the Data thread: %s\n",strerror(errno));
    }
    uint64_t fct_=start_of_client-initial_timestamp();
    sInfo* info = (sInfo*)information;
	int val1,pre_val1=0,val3=1,val4=0,val5=0,val6=0;
	int val2,pre_val2=0;
    int64_t tmp;
	int ret1;
	int ret2;
    bool strated=0;
	socklen_t optlen;
	optlen = sizeof val1;
	double preTime=0;
	double delta=0;
    int64_t offset=0;
    double bias_time=0;
	int reuse = 1;
    for(int i=0;i<FLOW_NUM;i++)
    {
        if (setsockopt(sock_for_cnt[i], IPPROTO_TCP, TCP_NODELAY, &reuse, sizeof(reuse)) < 0)
        {
            DBGMARK(0,0,"ERROR: set TCP_NODELAY option %s\n",strerror(errno));
            return((void *)0);
        }
    }
    while(true)
	{
       sleep(report_period);
       for(int i=0;i<flow_index;i++)
       {
         ret1= get_info(sock_for_cnt[i],&tcp_info);
         if(ret1<0)
         {
             DBGMARK(0,0,"setsockopt: for index:%d flow_index:%d TCP_C2TCP ... %s (ret1:%d)\n",i,flow_index,strerror(errno),ret1);
             return((void *)0);
         }
         DBGMARK(DBGSERVER,5,"min:%dus\t avg: %dus\t cnt:%d\t target_ratio:%d\t target=%dms\n",tcp_info.c2tcp_min_rtt,tcp_info.c2tcp_avg_urtt,tcp_info.c2tcp_cnt,target_ratio,target);
         u32 target_max=target*100/100;
         u32 target_min=target*85/100;
         u32 avg_ms=(tcp_info.c2tcp_avg_urtt/1000);
         if (target_max<avg_ms)
         {
            target_ratio-=avg_ms/target_max*50;
            if(target_ratio<TARGET_RATIO_MIN)
                target_ratio=TARGET_RATIO_MIN;
         }
         else if(avg_ms<target_min && avg_ms!=0)
         {
            target_ratio+=target_min/avg_ms*50;
            if(target_ratio>TARGET_RATIO_MAX)
                target_ratio=TARGET_RATIO_MAX;
         }
         else if (avg_ms==0)
         {
            target_ratio=TARGET_RATIO_MIN;
         }
         DBGMARK(DBGSERVER,5,"New target_ratio:%d\n",target_ratio);
         ret1 = setsockopt(sock_for_cnt[i], IPPROTO_TCP,TCP_C2TCP_ALPHA_INTERVAL, &target_ratio, sizeof(target_ratio));
         ret1 += setsockopt(sock_for_cnt[i], IPPROTO_TCP,TCP_C2TCP_ALPHA_SETPOINT, &target_ratio, sizeof(target_ratio));
         if(ret1<0)
         {
             DBGMARK(0,0,"setsockopt: for index:%d flow_index:%d ... %s (ret1:%d)\n",i,flow_index,strerror(errno),ret1);
             return((void *)0);
         }
       }
    }
	return((void *)0);
}
void* DataThread(void* info)
{
	struct sched_param param;
    param.__sched_priority=sched_get_priority_max(SCHED_RR);
    int policy=SCHED_RR;
    int s = pthread_setschedparam(pthread_self(), policy, &param);
    if (s!=0)
    {
        DBGERROR("Cannot set priority (%d) for the Main: %s\n",param.__sched_priority,strerror(errno));
    }

    s = pthread_getschedparam(pthread_self(),&policy,&param);
    if (s!=0)
    {
        DBGERROR("Cannot get priority for the Data thread: %s\n",strerror(errno));
    }
    pthread_t send_msg_thread;

	cFlow* flow = (cFlow*)info;
	int sock_local = flow->flowinfo.sock;
	char* src_ip;
	char write_message[BUFSIZ+1];
	char read_message[1024]={0};
	int len;
	char *savePtr;
	char* dst_addr;
	int loop;
	int remaining_size;

	memset(write_message,1,BUFSIZ);
	write_message[BUFSIZ]='\0';
	/**
	 * Get the RQ from client : {src_add} {flowid} {size} {dst_add}
	 */
	len=recv(sock_local,read_message,1024,0);
	if(len<=0)
	{
		DBGMARK(DBGSERVER,1,"recv failed! \n");
		close(sock_local);
		return 0;
	}
	/**
	 * For Now: we send the src IP in the RQ to!
	 */
	src_ip=strtok_r(read_message," ",&savePtr);
	if(src_ip==NULL)
	{
		//discard message:
		DBGMARK(DBGSERVER,1,"id: %d discarding this message:%s \n",flow->flowinfo.flowid,savePtr);
		close(sock_local);
		return 0;
	}
	char * isstr = strtok_r(NULL," ",&savePtr);
	if(isstr==NULL)
	{
		//discard message:
		DBGMARK(DBGSERVER,1,"id: %d discarding this message:%s \n",flow->flowinfo.flowid,savePtr);
		close(sock_local);
		return 0;
	}
	flow->flowinfo.flowid=atoi(isstr);
	char* size_=strtok_r(NULL," ",&savePtr);
	flow->flowinfo.size=1024*atoi(size_);
    DBGPRINT(DBGSERVER,4,"%s\n",size_);
	dst_addr=strtok_r(NULL," ",&savePtr);
	if(dst_addr==NULL)
	{
		//discard message:
		DBGMARK(DBGSERVER,1,"id: %d discarding this message:%s \n",flow->flowinfo.flowid,savePtr);
		close(sock_local);
		return 0;
	}
	char* time_s_=strtok_r(NULL," ",&savePtr);
    char *endptr;
    start_of_client=strtoimax(time_s_,&endptr,10);
	got_message=1;
    DBGPRINT(DBGSERVER,2,"Got message: %" PRIu64 " us\n",timestamp());
    flow->flowinfo.rem_size=flow->flowinfo.size;
    DBGPRINT(DBGSERVER,2,"time_rcv:%" PRIu64 " get:%s\n",start_of_client,time_s_);

	//Get detailed address
	strtok_r(src_ip,".",&savePtr);
	if(dst_addr==NULL)
	{
		//discard message:
		DBGMARK(DBGSERVER,1,"id: %d discarding this message:%s \n",flow->flowinfo.flowid,savePtr);
		close(sock_local);
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////////
	char query[150];	//query=data_size+' '+deadline+' '+agnostic
	char strTmp[150];
	char strTmp2[150];

	int sockfd;	//Socket

	//////////////////////////////////////////////////////////////////////////////

	//Calculate loops. In each loop, we can send BUFSIZ (8192) bytes of data
	loop=flow->flowinfo.size*1024/BUFSIZ;
	//Calculate remaining size to be sent
	remaining_size=flow->flowinfo.size*1024-loop*BUFSIZ;
	//Send data with 8192 bytes each loop
    DBGPRINT(DBGSERVER,5,"size:%d\trem_size:%d,loop:%d\n",flow->flowinfo.size*1024,remaining_size,loop);
	fprintf(stderr,"Server is sending the traffic ...\n");
    for(int i=0;i<loop;i++)
	{
		len=strlen(write_message);
		while(len>0)
		{
			DBGMARK(DBGSERVER,9,"++++++\n");
			len-=send(sock_local,write_message,strlen(write_message),0);
			DBGMARK(DBGSERVER,9,"      ------\n");
		}
	}
	//Send remaining data
	memset(write_message,1,BUFSIZ);
	write_message[remaining_size]='\0';
	len=strlen(write_message);
	DBGMARK(DBGSERVER,3,"\n");
	while(len>0)
	{
		len-=send(sock_local,write_message,strlen(write_message),0);
		DBGMARK(DBGSERVER,3,"\n");
	}
	DBGMARK(DBGSERVER,3,"\n");
	flow->flowinfo.rem_size=0;
    done=true;
    DBGPRINT(DBGSERVER,1,"done=true\n");
    close(sock_local);
    DBGPRINT(DBGSERVER,1,"done\n");
	return((void *)0);
}
