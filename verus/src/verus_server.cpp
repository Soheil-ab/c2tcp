#include "verus.hpp"

typedef tbb::concurrent_vector<udp_packet_t*> vec_udp;

int s, err;
int port;

double deltaDBar = 1.0;
double wMax = 0.0;
double dMax = 0.0;
double wBar = 1;
double dTBar = 0.0;
double dMaxLast =-10;
double dEst = 0.0;
int S = 0;
int ssId = 0;
double dMin = 1000.0;
double wList[MAX_W_DELAY_CURVE];

double delay;
int curveStop = MAX_W_DELAY_CURVE;
int maxWCurve = 0;
long long pktSeq = 0;
unsigned long long seqLast = 0;

double timeToRun;

bool slowStart = true;
bool exitSlowStart = false;
bool haveSpline = false;
bool terminate = false;
bool lossPhase = false;

char command[512];
char *name;

pthread_mutex_t lockSendingList;
pthread_mutex_t lockSPline;
pthread_mutex_t lockWList;
pthread_mutex_t restartLock;
pthread_mutex_t missingQueue;

pthread_t receiver_tid, delayProfile_tid, sending_tid, timeout_tid;

struct sockaddr_in adr_clnt;

struct timeval startTime;
struct timeval lastAckTime;

socklen_t len_inet;
spline1dinterpolant spline;

std::atomic<long long> wCrt(0);
std::atomic<long long> tempS(0);
std::vector<double> delaysEpochList;
std::map <unsigned long long, long long> sendingList;
std::map <int, udp_packet_t*> missingsequence_queue;

// output files
std::ofstream receiverLog;
std::ofstream lossLog;
std::ofstream verusLog;

// Boost timeout timer
boost::asio::io_service io;
boost::asio::deadline_timer timer (io, boost::posix_time::milliseconds(SS_INIT_TIMEOUT));

void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
    std::cout << "caught seg fault \n";

    verusLog.close();
    lossLog.close();
    receiverLog.close();

    exit(0);
}

static void displayError(const char *on_what) {
    fputs(strerror(errno),stderr);
    fputs(": ",stderr);
    fputs(on_what,stderr);
    fputc('\n',stderr);

    std::cout << "Error \n";

    exit(0);
}

void write2Log (std::ofstream &logFile, std::string arg1, std::string arg2, std::string arg3, std::string arg4, std::string arg5) {
  double relativeTime;
  struct timeval currentTime;

  gettimeofday(&currentTime,NULL);
    relativeTime = (currentTime.tv_sec-startTime.tv_sec)+(currentTime.tv_usec-startTime.tv_usec)/1000000.0;

    logFile << relativeTime << "," << arg1;

    if (arg2 != "")
      logFile << "," << arg2;
    if (arg3 != "")
      logFile << "," << arg3;
    if (arg4 != "")
      logFile << "," << arg4;
    if (arg5 != "")
      logFile << "," << arg5;

    logFile << "\n";

    return;
}

double ewma (double vals, double delay, double alpha) {
    double avg;

    // checking if the value is negative, meanning it has not been udpated
    if (vals < 0)
        avg = delay;
    else
        avg = vals * alpha + (1-alpha) * delay;

    return avg;
}

udp_packet_t *
udp_pdu_init(int seq, unsigned int packetSize, int w, int ssId) {
    udp_packet_t *pdu;
    struct timeval timestamp;

    if (packetSize <= sizeof(udp_packet_t)) {
        printf("defined packet size is smaller than headers");
        exit(0);
    }

    pdu = (udp_packet_t*)malloc(packetSize);

    if (pdu) {
        pdu->seq = seq;
        pdu->w = w;
        pdu->ss_id = ssId;
        gettimeofday(&timestamp,NULL);
        pdu->seconds = timestamp.tv_sec;
        pdu->millis = timestamp.tv_usec;
    }
  return pdu;
}

void TimeoutHandler( const boost::system::error_code& e) {
    double timeouttimer = 0;

    if (e) return;

    write2Log (lossLog, "Timeout", "", "", "", "");

    if (seqLast == 0) {
        write2Log (lossLog, "Restart", "lost first packet", "", "", "");
        restartSlowStart();
        return;
    }

    if (slowStart) {
        slowStart = false;
        write2Log (lossLog, "Exit slow start", "timeout", "", "", "");
    }
    else {
        // timeout means that no packets in flight, so we should reset the packets in flight
        // we should also change the sequence last (last acked packet) to the last sent packet
        lossPhase = true;
        wCrt = 0;
        dEst = dMin;

        pthread_mutex_lock(&lockSendingList);
        if (sendingList.size() > 0) {
            write2Log (lossLog, "clearing sending list because of timeout", "", "", "", "");
            seqLast = sendingList.rbegin()->first;
            sendingList.clear();
        }
        pthread_mutex_unlock(&lockSendingList);

        // resetting the missingsequence queue
        pthread_mutex_lock(&missingQueue);
        missingsequence_queue.clear();
        pthread_mutex_unlock(&missingQueue);
    }

    //update timer and restart
    timeouttimer=fmin (MAX_TIMEOUT, fmax((5*delay), MIN_TIMEOUT));
    timer.expires_from_now (boost::posix_time::milliseconds(timeouttimer));
    timer.async_wait(&TimeoutHandler);

    return;
}

void restartSlowStart(void) {

    pthread_mutex_lock(&restartLock);

    int i;

    maxWCurve = 0;
    dEst =0.0;
    seqLast = 0;
    wBar =1;
    dTBar = 0.0;
    wCrt = 0;
    dMin = 1000.0;
    pktSeq =0;
    dMax = 0.0;
    deltaDBar = 1.0;
    wMax = 0.0;
    dMaxLast = -10;
    slowStart = true;
    lossPhase = false;
    haveSpline = false;

    // stop the delay profile curve thread and restart it
    pthread_cancel(delayProfile_tid);
    if (pthread_create(&(delayProfile_tid), NULL, &delayProfile_thread, NULL) != 0)
        std::cout << "can't create thread: " <<  strerror(err) << "\n";

    // increase slow start session ID
    ssId ++;

    // cleaning up
    pthread_mutex_lock(&lockSendingList);
    sendingList.clear();
    pthread_mutex_unlock(&lockSendingList);
    pthread_mutex_lock(&missingQueue);
    missingsequence_queue.clear();
    pthread_mutex_unlock(&missingQueue);
    delaysEpochList.clear();

    for (i=0; i<MAX_W_DELAY_CURVE; i++)
        wList[i]=-1;

    // sending the first packet for slow start
    tempS = 1;

    //update timeout timer and restart
    timer.expires_from_now (boost::posix_time::milliseconds(SS_INIT_TIMEOUT));
    timer.async_wait(&TimeoutHandler);

    pthread_mutex_unlock(&restartLock);
}

double calcDelayCurve (double delay) {
    int w;

    for (w=2; w < MAX_W_DELAY_CURVE; w++) {
        if (!haveSpline) {
            pthread_mutex_lock(&lockWList);
            if (wList[w] > delay) {
                pthread_mutex_unlock(&lockWList);
                return (w-1);
            }
            pthread_mutex_unlock(&lockWList);
        }
        else {
            pthread_mutex_lock(&lockSPline);
            try {
                if (spline1dcalc(spline, w) > delay) {
                    pthread_mutex_unlock(&lockSPline);
                    return (w-1);
                }
            }
            catch (alglib::ap_error exc) {
                std::cout << "alglib1 " << exc.msg.c_str() << "\n";
                write2Log (lossLog, "CalcDelayCurve error", exc.msg.c_str(), "", "", "");
            }
            pthread_mutex_unlock(&lockSPline);
        }
    }

    if (!haveSpline)
        return calcDelayCurve(delay-DELTA2); // special case: when verus starts working and we don't have a curve.
    else
        return -1000.0;
}

double calcDelayCurveInv (double w) {
    double ret;

    if (!haveSpline) {
        if (w < MAX_W_DELAY_CURVE) {
            pthread_mutex_lock(&lockWList);
            ret = wList[(int)w];
            pthread_mutex_unlock(&lockWList);
            return ret;
        }
        else
            return wList[MAX_W_DELAY_CURVE];
    }

    pthread_mutex_lock(&lockSPline);
    try{
        ret = spline1dcalc(spline, w);
    }
    catch (alglib::ap_error exc) {
        std::cout << "alglib2 " << exc.msg.c_str() << "\n";
        write2Log (lossLog, "Alglib error", exc.msg.c_str(), std::to_string(w), "", "");
    }
    pthread_mutex_unlock(&lockSPline);
    return ret;
}

int calcSi (double wBar) {
    int S;
    int n;

    n = (int) ceil(dTBar/(EPOCH/1000.0));

    if (n > 1)
        S = (int) fmax (0, (wBar+wCrt*(2-n)/(n-1)));
    else
        S = (int) fmax (0, (wBar - wCrt));

    return S;
}

void* sending_thread (void *arg)
{
    int i, ret;
    int sPkts;
    udp_packet_t *pdu;
    //struct timeval currentTime;

    while (!terminate) {
        while (tempS > 0) {
            sPkts = tempS;
            tempS = 0;

            for (i=0; i<sPkts; i++) {
                pktSeq ++;
                pdu = udp_pdu_init(pktSeq, MTU, wBar, ssId);
                //gettimeofday(&currentTime,NULL);

                ret = sendto(s, pdu, MTU, MSG_DONTWAIT, (struct sockaddr *)&adr_clnt, len_inet);

		// std::cout << "Sending packet..." << std::endl;

                if (ret < 0) {
                    // if UDP buffer of OS is full, we exit slow start and treat the current packet as lost
                    if (errno == ENOBUFS || errno == EAGAIN || errno == EWOULDBLOCK) {
                        if (slowStart) {
                            lossPhase = true;
                            exitSlowStart = true;
                            wBar = 0.49 * pdu->w; // this is so that we dont switch exitslowstart until we receive packets that are not from slow start
                            dEst = 0.75*dMin*VERUS_R; // setting dEst to half of the allowed maximum delay, for effeciency purposes
                            slowStart = false;

                            // this packet was not sent we should decrease the packet seq number and free the pdu
                            pktSeq --;
                            free(pdu);

                            write2Log (lossLog, "Exit slow start", "reached maximum OS UDP buffer size", std::to_string(wCrt), "", "");
                            break;
                        }
                        else {
                            // this is normal sending, OS UDP buffer is full, discard this packet and treat as lost
                            wBar = fmax(1.0, VERUS_M_DECREASE * wBar);
                            dEst = calcDelayCurveInv (wBar);

                            // this packet was not sent we should decrease the packet seq number and free the pdu
                            pktSeq --;
                            free(pdu);

                            write2Log (lossLog, "Loss", "reached maximum OS UDP buffer size", std::to_string(errno), "", "");
                            break;
                        }
                    }
                    else
                        displayError("sendto(2)");
                }
                // storing sending packet info in sending list with sending time
                pthread_mutex_lock(&lockSendingList);
                sendingList[pdu->seq]=pdu->w;
                pthread_mutex_unlock(&lockSendingList);
                free (pdu);

                // sending one new packet -> increase packets in flight
                wCrt ++;
            }
            if (tempS > 0 && !slowStart)
                write2Log (lossLog, "Epoch Error", "couldn't send everything within the epoch. Have more to send", std::to_string(tempS.load()), std::to_string(slowStart), "");
        }
    }
    return NULL;
}

void* delayProfile_thread (void *arg)
{
    int i;
    std::vector<int> x;
    std::vector<double> y;

    int max_i;
    double N = 15.0;
    int cnt;
    double* xs = NULL;
    double* ys = NULL;
    real_1d_array xi;
    real_1d_array yi;
    ae_int_t info;
    spline1dfitreport rep;
    spline1dinterpolant splineTemp;

    while (!terminate) {
        if (slowStart){
            // still in slowstart phase we should not calculate the delay curve
            usleep (1);
        }
        else{
            // in normal mode we should compute the delay curve and sleep for the delay curve refresh timer
            // clearing the vectors
            x.clear();
            y.clear();

            max_i = 1;
            cnt = 0;
            maxWCurve = MAX_W_DELAY_CURVE;

            pthread_mutex_lock(&lockWList);
            for (i=1; i<maxWCurve; i++) {
                if (wList[i] >=0) {
                    if (i < curveStop) {
                        x.push_back(i);
                        y.push_back(wList[i]);

                        cnt++;
                        xs=(double*)realloc(xs,cnt*sizeof(double));
                        ys=(double*)realloc(ys,cnt*sizeof(double));

                        xs[cnt-1] = (double) i;
                        ys[cnt-1] = (double) wList[i];
                        max_i = i;
                    }
                    else{
                        wList[i] = -1;
                    }
                }
            }
            pthread_mutex_unlock(&lockWList);

            curveStop = MAX_W_DELAY_CURVE;
            maxWCurve = 0;

            xi.setcontent(cnt, xs);
            yi.setcontent(cnt, ys);

            while (max_i/N < 5) {
                N--;
                if (N < 1) {
                    write2Log (lossLog, "Restart", "Alglib M<4!", "", "", "");
                    restartSlowStart();
                    return NULL;
                }
            }
            if (max_i/N > 4) {
                try {
                    // if alglib takes long time to compute, we can use the previous spline in other threads
                    spline1dfitpenalized(xi, yi, max_i/N, 2.0, info, splineTemp, rep);

                    // copy newly calculated splinetemp to update spline
                    pthread_mutex_lock(&lockSPline);
                    spline=splineTemp;
                    pthread_mutex_unlock(&lockSPline);

                    haveSpline = true;
                }
                catch (alglib::ap_error exc) {
                    write2Log (lossLog, "Restart", "Spline exception", exc.msg.c_str(), "", "");
                    restartSlowStart();
                }
            }
            usleep (CURVE_TIMER);
        }
    }
    return NULL;
}

void updateUponReceivingPacket (double delay, int w) {

    if (wCrt > 0)
        wCrt --;

    // processing the delay and updating the verus parameters and the delay curve points
    delaysEpochList.push_back(delay);
    dTBar = ewma (dTBar, delay, 0.875);

    // updating the minimum delay
    if (delay < dMin)
        dMin = delay;

    // not to update the wList with any values that comes within the loss phase
    if (!lossPhase) {
        pthread_mutex_lock(&lockWList);
        wList[w] = ewma (wList[w], delay, 0.875);
        pthread_mutex_unlock(&lockWList);

        if (maxWCurve < w)
            maxWCurve = w;
    }
    else {    // still in loss phase, received an ACK, do similar to congestion avoidance
        wBar += 1.0/wBar;

        if(haveSpline)
            dEst = fmin (dEst, calcDelayCurveInv (wBar));
    }
    return;
}

void createMissingPdu (int i) {
    udp_packet_t* pdu;
    pdu = (udp_packet_t *) malloc(sizeof(udp_packet_t));
    struct timeval currentTime;

    gettimeofday(&currentTime,NULL);
    pdu->seq = i;
    pdu->seconds = currentTime.tv_sec;
    pdu->millis = currentTime.tv_usec;

    pthread_mutex_lock(&lockSendingList);
    pdu->w = sendingList.find(pdu->seq)->second;
    pthread_mutex_unlock(&lockSendingList);

    pthread_mutex_lock(&missingQueue);
    missingsequence_queue[i]=pdu;
    pthread_mutex_unlock(&missingQueue);

    return;
}

void removeExpiredPacketsFromSeqQueue (struct timeval receivedtime) {
    bool timerExpiry = false;
    udp_packet_t* pdu;
    double missingsequence_delay;

    // accessing the first element in the queue to check if its expired
    pdu = missingsequence_queue.begin()->second;
    missingsequence_delay = (receivedtime.tv_sec-pdu->seconds)*1000.0+(receivedtime.tv_usec-pdu->millis)/1000.0;

    while (missingsequence_delay >= MISSING_PKT_EXPIRY) { // missing packet is treated lost after MISSING_PKT_EXPIRY
        // this means that we have identified a packet loss
        // storing the w of the first missing pdu expiry to use it in the multiplicative decrease
        if (!timerExpiry) {
            timerExpiry = true;

            if (!lossPhase) { // if its a new loss phase then we do multiplicative decrease, otherwise it belonges to the same loss phase
                lossPhase = true;

                write2Log (lossLog, "Missing packet expired", std::to_string(pdu->seq), "", "", ""); // we are only recordering the first missing packet expiry per loss phase

                // get the w of the lost packet and do multiplicative decrease
                wBar = fmin( wBar, VERUS_M_DECREASE * pdu->w);

                if (slowStart){
                    pthread_mutex_lock(&lockWList);
                    dEst = wList[(int)wBar];
                    pthread_mutex_unlock(&lockWList);
                }
                else
                    dEst = calcDelayCurveInv (wBar);
            }

            // We encountered packet losses, exit slow start
            if (slowStart && (int)wBar > 20) {
                curveStop = fmax (100, pdu->w);
                slowStart = false;

                wCrt = 0;
                pthread_mutex_lock(&lockSendingList);
                seqLast = sendingList.rbegin()->first;
                pthread_mutex_unlock(&lockSendingList);

                missingsequence_queue.clear();
            write2Log (lossLog, "Exit slow start", "lost a packet with wBar ", std::to_string(wBar), "", "");
            }
        }
        // erase the pdu from the missing queue as well as from the sendinglist
        if (missingsequence_queue.find(pdu->seq) != missingsequence_queue.end()) {
            missingsequence_queue.erase(pdu->seq);
        }

        pthread_mutex_lock(&lockSendingList);
        if (sendingList.find(pdu->seq) != sendingList.end()) {
            sendingList.erase(pdu->seq);
        }
        pthread_mutex_unlock(&lockSendingList);

        free(pdu);

        if (wCrt > 0)
            wCrt--;

        missingsequence_delay = 0;

        if (missingsequence_queue.size() > 0) {
            pdu = missingsequence_queue.begin()->second;
            missingsequence_delay = (receivedtime.tv_sec-pdu->seconds)*1000.0+(receivedtime.tv_usec-pdu->millis)/1000.0;
        }
    }
    return;
}

void* receiver_thread (void *arg)
{
    unsigned int i;
    double timeouttimer=0.0;
    socklen_t len_inet;
    udp_packet_t *pdu;
    struct timeval receivedtime;

    len_inet = sizeof(struct sockaddr_in);

    sprintf (command, "%s/Losses.out", name);
    lossLog.open(command);
    sprintf (command, "%s/Receiver.out", name);
    receiverLog.open(command);

    pdu = (udp_packet_t *) malloc(sizeof(udp_packet_t));

    while (!terminate) {

        if (recvfrom(s, pdu, sizeof(udp_packet_t), 0, (struct sockaddr *)&adr_clnt, &len_inet) < 0)
            displayError("Receiver thread error");

        pthread_mutex_lock(&restartLock);

        // we have started a new SS session, this packet belongs to the old SS session, so we discard it
        if (pdu->ss_id < ssId) {
            pthread_mutex_unlock(&restartLock);
            continue;
        }

        gettimeofday(&receivedtime,NULL);
        delay = (receivedtime.tv_sec-pdu->seconds)*1000.0+(receivedtime.tv_usec-pdu->millis)/1000.0;

        if (slowStart && delay > SS_EXIT_THRESHOLD) { // if the current delay exceeds half a second during slow start we should time out and exit slow start
            wBar = VERUS_M_DECREASE * pdu->w;
            dEst = 0.75 * dMin * VERUS_R; // setting dEst to half of the allowed maximum delay, for effeciency purposes
            lossPhase = true;
            slowStart = false;
            exitSlowStart = true;

            write2Log (lossLog, "Exit slow start", "exceeding SS_EXIT_THRESHOLD", "", "", "");
        }

        //update timer and restart
        timeouttimer=fmin (MAX_TIMEOUT, fmax((5*delay), MIN_TIMEOUT));
        timer.expires_from_now (boost::posix_time::milliseconds(timeouttimer));
        timer.async_wait(&TimeoutHandler);

        write2Log (receiverLog, std::to_string(pdu->seq), std::to_string(delay), std::to_string(wCrt), std::to_string(wBar), "");

        // exiting the loss phase in case we are receiving new packets ack with w equal or smaller to the new w after the loss
        if (lossPhase && pdu->w <= wBar) {
            delaysEpochList.clear();
            lossPhase = false;
            exitSlowStart = false;
        }

        // Receiving exactly the next sequence number, everything is ok no losses
        if (pdu->seq == seqLast+1) {
            updateUponReceivingPacket (delay, pdu->w);
        }
        else if (pdu->seq < seqLast) {
            // received a packet with seq number smaller than the anticipated one (out of order). Need to check if that packet is there in the missing queue
            pthread_mutex_lock(&missingQueue);
            if (missingsequence_queue.find(pdu->seq) != missingsequence_queue.end()) {
                missingsequence_queue.erase(pdu->seq);
                updateUponReceivingPacket (delay, pdu->w);
            }
            else
                write2Log (lossLog, "Received an expired out of sequence packet ", std::to_string(pdu->seq), std::to_string(seqLast), "", "");
            pthread_mutex_unlock(&missingQueue);
        }
        else { // creating an out of sequence packet and inserting it to the out of sequence queue
            for (i=seqLast+1; i<pdu->seq; i++)
                createMissingPdu (i);
            updateUponReceivingPacket (delay, pdu->w);
        }

        // setting the last received sequence number to the current received one for next packet arrival processing
        // making sure we dont take out of order packet
        if (pdu->seq >= seqLast+1 && pdu->ss_id == ssId) {
            seqLast = pdu->seq;
            gettimeofday(&lastAckTime,NULL);
        }

        // freeing that received pdu from the sendinglist map
        pthread_mutex_lock(&lockSendingList);
        if (sendingList.find(pdu->seq) != sendingList.end()) {
            sendingList.erase(pdu->seq);
        }
        pthread_mutex_unlock(&lockSendingList);

        // ------------------ verus slow start ------------------
        if(slowStart) {
            // since we received an ACK we can increase the sending window by 1 packet according to slow start
            wBar ++;
            tempS += fmax(0, wBar-wCrt);   // let the sending thread start sending the new packets
        }
        pthread_mutex_unlock(&restartLock);
    }
    return NULL;
}

void* timeout_thread (void *arg)
{
    boost::asio::io_service::work work(io);

    timer.expires_from_now (boost::posix_time::milliseconds(SS_INIT_TIMEOUT));
    timer.async_wait(&TimeoutHandler);
    io.run();

    return NULL;
}

int main(int argc,char **argv) {

    int i=0;
    double bufferingDelay;
    double relativeTime=0;
    double wBarTemp;
    bool dMinStop=false;

    char dgram[512];             // Recv buffer

    struct stat info;
    struct sigaction sa;
    struct sockaddr_in adr_inet;
    struct timeval currentTime;

    // catching segmentation faults
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_sigaction;
    sa.sa_flags   = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    for (int j=0; j<MAX_W_DELAY_CURVE; j++)
        wList[j]=-1;

    if (argc < 7) {
        std::cout << "syntax should be ./verus_server -name NAME -p PORT -t TIME (sec) \n";
        exit(0);
    }

    while (i != (argc-1)) {
        i=i+1;
        if (!strcmp (argv[i], "-name")) {
            i=i+1;
            name = argv[i];
            }
        else if (!strcmp (argv[i], "-p")) {
            i=i+1;
            port = atoi (argv[i]);
            }
        else if (!strcmp (argv[i], "-t")) {
            i=i+1;
            timeToRun = std::stod(argv[i]);
            }
        else {
            std::cout << "syntax should be ./verus_server -name NAME -p PORT -t TIME (sec) \n";
            exit(0);
        }
    }

    s = socket(AF_INET,SOCK_DGRAM,0);
    if ( s == -1 )
        displayError("socket error()");

    memset(&adr_inet,0,sizeof adr_inet);
    adr_inet.sin_family = AF_INET;
    adr_inet.sin_port = htons(port);
    adr_inet.sin_addr.s_addr = INADDR_ANY;

    if ( adr_inet.sin_addr.s_addr == INADDR_NONE )
        displayError("bad address.");

    len_inet = sizeof(struct sockaddr_in);

    if (bind (s, (struct sockaddr *)&adr_inet, sizeof(adr_inet)) < 0)
        displayError("bind()");

    std::cout << "Server " << port << " waiting for request\n";

    // waiting for initialization packet
    if (recvfrom(s, dgram, sizeof (dgram), 0, (struct sockaddr *)&adr_clnt, &len_inet) < 0)
        displayError("recvfrom(2)");

    if (stat (name, &info) != 0) {
        sprintf (command, "exec mkdir %s", name);
        system(command);
    }
    sprintf (command, "%s/Verus.out", name);
    verusLog.open(command);

    // getting the start time of the program, to make relative timestamps
    gettimeofday(&startTime,NULL);

    // create mutex
    pthread_mutex_init(&lockSendingList, NULL);
    pthread_mutex_init(&lockSPline, NULL);
    pthread_mutex_init(&lockWList, NULL);
    pthread_mutex_init(&restartLock, NULL);

    // starting the threads
    if (pthread_create(&(timeout_tid), NULL, &timeout_thread, NULL) != 0)
        std::cout << "can't create thread: " <<  strerror(err) << "\n";
    if (pthread_create(&(receiver_tid), NULL, &receiver_thread, NULL) != 0)
        std::cout << "Can't create thread: " << strerror(err);
    if (pthread_create(&(sending_tid), NULL, &sending_thread, NULL) != 0)
        std::cout << "Can't create thread: " << strerror(err);
    if (pthread_create(&(delayProfile_tid), NULL, &delayProfile_thread, NULL) != 0)
        std::cout << "can't create thread: " <<  strerror(err) << "\n";

    // sending the first for slow start
    tempS = 1;

    std::cout << "Client " << port << " is connected\n";

    while (relativeTime <= timeToRun) {

        gettimeofday(&currentTime,NULL);
        relativeTime = (currentTime.tv_sec-startTime.tv_sec)+(currentTime.tv_usec-startTime.tv_usec)/1000000.0;

        // Checking if we have an missing packets that have expired so that we can trigger a loss
        pthread_mutex_lock(&missingQueue);
        if (missingsequence_queue.size() > 0)
            removeExpiredPacketsFromSeqQueue(currentTime);
        pthread_mutex_unlock(&missingQueue);

        // waiting for slow start to finish to start sending data
        if (!slowStart) {
            dMinStop=false;

            // -------------- Verus sender ------------------
            if (delaysEpochList.size() > 0) {
                dMax = *std::max_element(delaysEpochList.begin(),delaysEpochList.end());
                delaysEpochList.clear();
            }
            else {
                bufferingDelay = (currentTime.tv_sec-lastAckTime.tv_sec)*1000.0+(currentTime.tv_usec-lastAckTime.tv_usec)/1000.0;
                dMax = fmax (dMaxLast, bufferingDelay);
            }

            // only first verus epoch, dMaxLast is intialized to -10
            if (dMaxLast == -10)
                dMaxLast = dMax;

            deltaDBar = dMax - dMaxLast;

            // normal verus protocol
            if (dMaxLast/dMin > VERUS_R) {
                if (!exitSlowStart) {
                    dEst = fmax (dMin, (dEst-DELTA2));

                    if (dEst == dMin && wCrt < 2) {
                        dMin += 10;
                    }
                    else if (dEst == dMin)
                        dMinStop=true;
                }
            }
            else if (deltaDBar > 0.0001)
                dEst = fmax (dMin, (dEst-DELTA1));
            else
                dEst += DELTA2;

            wBarTemp = calcDelayCurve (dEst);

            if (wBarTemp < 0) {
                write2Log (lossLog, "Restart", "can't map delay on delay curve", "", "", "");
                restartSlowStart();
                continue;
            }

            wBar = fmax (1.0, wBarTemp);
            S = calcSi (wBar);

            dMaxLast = ewma(dMaxLast, dMax, 0.2);

            if (!dMinStop)
                tempS += S;

            write2Log (verusLog, std::to_string(dEst), std::to_string(dMin), std::to_string(wCrt), std::to_string(wBar), std::to_string(tempS));
        }
        usleep (EPOCH);
    }

    verusLog.close();
    lossLog.close();
    receiverLog.close();
    io.stop();
    ssId = -1;
    tempS = 1;

    usleep (1000000);
    terminate = true;
    usleep (1000000);

    std::cout << "Server " << port << " is exiting\n";
    close(s);

    return 0;
}
