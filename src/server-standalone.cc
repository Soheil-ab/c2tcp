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
#define DBGSERVER 0
#include <cstdlib>
#include "define.h"
#define MAX_CWND 40000
#define MIN_CWND 4
#define MIN_RTT_DURING_TRAINING 21

int main(int argc, char **argv)
{
    DBGPRINT(DBGSERVER,4,"Main\n");
    if(argc!=5)
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
//	delay_ms=atoi(argv[1]);
	client_port=atoi(argv[1]);
	qplus_enable=0;
//    downlink=argv[3];
//    uplink=argv[4];
//    log_file=argv[5];
    target=atoi(argv[2]);
    target_ratio=atoi(argv[3]);
//    qsize=atoi(argv[8]);
    report_period=atoi(argv[4]);
//	codel=atoi(argv[10]);
    start_server(flow_num, client_port);
	DBGMARK(DBGSERVER,5,"DONE!\n");
	return 0;
}

void usage()
{
	DBGMARK(0,0,"./server [port] [Target] [Initial Alpha] [Report Period: 1 sec]\n");
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
        char scheme_tmp[]="cubic";
	if (setsockopt(sock[i], IPPROTO_TCP, TCP_CONGESTION, scheme_tmp, strlen(scheme_tmp)) < 0) 
	{
		DBGMARK(0,0,"TCP congestion doesn't exist: %s\n",strerror(errno));
		return;
	}
	/*Enable C2TCP*/
	int enable_c2tcp=1;
	if (setsockopt(sock[i], IPPROTO_TCP, TCP_C2TCP_ENABLE, &enable_c2tcp, sizeof(enable_c2tcp)) < 0) 
	{
		DBGMARK(0,0,"Cannot enable C2TCP: %s\n",strerror(errno));
		return;
	}


    }
    char container_cmd[500];

    char cmd[1000];
    char final_cmd[1000];

    //DBGPRINT(DBGSERVER,0,"%s\n",final_cmd);
    info->trace=trace;
    info->num_lines=num_lines;
    initial_timestamp();


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
			//DBGMARK(0,0,"sock::%d, index:%d\n",sock[flow_index],flow_index);
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
        DBGPRINT(0,0,"Server is Connected to the client...\n");
            flow_index++;
	}
    pthread_join(data_thread, NULL);
}
void* CntThread(void* information)
{
//    printf("testing\n");
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
        //TCP_DEEPCC_ENABLE
        int enable_deepcc=2;
        if (setsockopt(sock_for_cnt[i], IPPROTO_TCP, TCP_DEEPCC_ENABLE, &enable_deepcc, sizeof(enable_deepcc)) < 0) 
        {
            DBGMARK(0,0,"CHECK KERNEL VERSION (0514+) ;CANNOT ENABLE DEEPCC %s\n",strerror(errno));
            return ((void* )0);
        } 
    }
    while(true)
	{

		usleep(report_period*1000);
  //     printf("testing\n");
       for(int i=0;i<flow_index;i++)
       {
         ret1= get_deepcc_info(sock_for_cnt[i],&deepcc_info);
         if(ret1<0)
         {
             DBGMARK(0,0,"setsockopt: for index:%d flow_index:%d TCP_C2TCP ... %s (ret1:%d)\n",i,flow_index,strerror(errno),ret1);
             return((void *)0);
         }
         u32 avg_ms=(deepcc_info.avg_urtt/1000);
         u32 minrtt_ms=(deepcc_info.min_rtt/1000);
         if(target==0)   
             target = minrtt_ms*2;


         DBGMARK(DBGSERVER,5,"min:%dus\t avg: %dus\t cnt:%d\t target_ratio:%d\t target=%dms\n",tcp_info.c2tcp_min_rtt,tcp_info.c2tcp_avg_urtt,tcp_info.c2tcp_cnt,target_ratio,target);
         u32 target_max=target*100/100;
         u32 target_min=target*85/100;

//         target_ratio=(target_max*100)/minrtt_ms;
         if (target_max<avg_ms)
         {
//            target_ratio-=avg_ms/target_max*50;
            target_ratio=target_ratio-(2*(avg_ms-target_max)*target_ratio)/(avg_ms); // alpha_2=alpha_1*(1-2(avg-t)/avg)
            if(target_ratio<TARGET_RATIO_MIN)
                target_ratio=TARGET_RATIO_MIN;
         }
         else if(avg_ms<target_max && avg_ms!=0)
         {
//            target_ratio+=target_min/avg_ms*50;
            target_ratio=target_ratio+((target_max-avg_ms)*target_ratio/2)/(avg_ms); // alpha_2=alpha_1*(1-2(t-avg)/avg)
            if(target_ratio>TARGET_RATIO_MAX)
                target_ratio=TARGET_RATIO_MAX;
         }
/*         else if (avg_ms==0)
         {
            target_ratio=TARGET_RATIO_MIN;
         }
*/
         DBGMARK(DBGSERVER,5,"New target_ratio:%d\n",target_ratio);
         ret1 = setsockopt(sock_for_cnt[i], IPPROTO_TCP,TCP_C2TCP_ALPHA, &target_ratio, sizeof(target_ratio));
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
    u64 loop;
    u64  remaining_size;

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
    loop=flow->flowinfo.size/BUFSIZ*1024;
    //Calculate remaining size to be sent
    remaining_size=flow->flowinfo.size*1024-loop*BUFSIZ;
    //Send data with 8192 bytes each loop
    //DBGPRINT(DBGSERVER,5,"size:%" PRId64 "\trem_size:%u,loop:%" PRId64 "\n",flow->flowinfo.size*1024,remaining_size,loop);
    DBGPRINT(0,0,"Server is sending the traffic ...\n");

   // for(u64 i=0;i<loop;i++)
    while(true)
    {
        len=strlen(write_message);
        while(len>0)
        {
            DBGMARK(DBGSERVER,5,"++++++\n");
            len-=send(sock_local,write_message,strlen(write_message),0);
            usleep(50);         
            DBGMARK(DBGSERVER,5,"      ------\n");
        }
        usleep(100);
    }

    //Send remaining data
    memset(write_message,1,BUFSIZ);
    write_message[remaining_size+1]='\0';
    len=strlen(write_message);
    DBGMARK(DBGSERVER,3,"remaining starts\n");
    while(len>0)
    {
        len-=send(sock_local,write_message,strlen(write_message),0);
        DBGMARK(DBGSERVER,3,"*******************\n");
    }
    DBGMARK(DBGSERVER,3,"remaining finished\n");
    flow->flowinfo.rem_size=0;
    done=true;
    DBGPRINT(DBGSERVER,1,"done=true\n");
    close(sock_local);
    DBGPRINT(DBGSERVER,1,"done\n");
    return((void *)0);
}
