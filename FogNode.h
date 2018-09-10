#ifndef _6390_PROJECT_FOG_NODE_H_
#define _6390_PROJECT_FOG_NODE_H_

#include "Debug.h"
#include "PeerRecord.h"
#include "Packets.h"

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>


#include <cstddef>

#include <algorithm>
#include <functional>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#define RECV_BUFFER_SIZE (256)

uint32_t get_ip_addr(const char *hostname);

class FogNode
{
    public:
        FogNode(NodeSignature signature, int updateInterval, int maxResponseTime, unsigned short udpPort, vector<PeerRecord> peers);
        void initialize();
        void run();
        
    private:
        void handleIOTMessage();
        void updatePeers();
        void handlePeerMessage(PeerRecord& peer);
        
        void handleRequest(IOTRequestPacketWrapper& pw);
        void popFromQueue(bool isCloud);
        void pushToQueue(IOTRequestPacketWrapper& pw, bool isCloud);
        void forwardPacket(IOTRequestPacketWrapper& pw);
        void handleNextRequestInQueue(bool isCloud);
        
        static int encodeIndex(int ind);
        static int decodeIndex(int val);
        static uint64_t readTimer(int timerfd);
        static void setTimer(int timerfd, int initialSecs, int initialMs = 0, int intervalSecs = 0, int intervalMs = 0);
        
        NodeSignature signature;
        unsigned short udpPort;
        int updateInterval, maxResponseTime;
        vector<PeerRecord> peers;
        
        vector<reference_wrapper<PeerRecord>> peersByQueueDelay;
        
        sockaddr_in addrtcp, addrudp;
        int udpfd, tcpfd, updatetimerfd, cloudtimerfd, fogtimerfd;
        
        int epollFdCount;
        int epollfd;
        
        int queueDelay;
        queue<IOTRequestPacketWrapper> fogQueue;
        queue<IOTRequestPacketWrapper> cloudQueue;

};

#endif /* _6390_PROJECT_FOG_NODE_H_ */
