#include "FogNode.h"

uint32_t get_ip_addr(const char *hostname)
{
    addrinfo *res;
    addrinfo hint = { 0, AF_INET };
    int retval;
    if(retval = getaddrinfo(hostname, nullptr, &hint, &res) != 0) {
        DERROR(gai_strerror(retval), -1);
    }
    if(res == nullptr || res->ai_family != AF_INET || res->ai_addr == nullptr) {
        DERROR("Cannot find " << hostname, -1);
    }
    auto ipaddr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);
    return ipaddr;
}



FogNode::FogNode(NodeSignature signature, int updateInterval, int maxResponseTime, unsigned short udpPort, vector<PeerRecord> peers)
{
    this->signature = signature;
    this->updateInterval = updateInterval;
    this->maxResponseTime = maxResponseTime;
    this->udpPort = udpPort;
    this->peers = peers;
}

void FogNode::initialize()
{

    DINFO("Setting up TCP & UDP adddress structures...");
    addrtcp = { AF_INET, htons(signature.port), { signature.ip.all } };
    addrudp = { AF_INET, htons(udpPort), { signature.ip.all } };

    DINFO("Initiating UDP socket...");
    EXIT_ON_ERROR(udpfd = socket(AF_INET, SOCK_DGRAM, 0) );

    DINFO("Initiating TCP socket...");
    EXIT_ON_ERROR(tcpfd = socket(AF_INET, SOCK_STREAM, 0));

    DINFO("Setting up TCP socket to be reused...");
    EXIT_ON_ERROR(setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, shared_ptr<const int>(new const int { 1 }).get(), sizeof(const int)));

    DINFO("Initiating peer update timer...");
    EXIT_ON_ERROR(updatetimerfd = timerfd_create(CLOCK_MONOTONIC, 0));

    DINFO("Initiating cloud process timer...");
    EXIT_ON_ERROR(cloudtimerfd = timerfd_create(CLOCK_MONOTONIC, 0));

    DINFO("Initiating fog process timer...");
    EXIT_ON_ERROR(fogtimerfd = timerfd_create(CLOCK_MONOTONIC, 0));
    
    DINFO("Sorting peers in increasing signature order...");
    sort(peers.begin(), peers.end(), PeerRecord::CompareBySignature);
    
    DINFO("Creating peer index sorted by queue delay.");
    peersByQueueDelay = vector<reference_wrapper<PeerRecord>>(peers.begin(), peers.end());
    
    DINFO("Connecting to peers with lower node signature value (ip:port)...");
    auto it = peers.begin();
    for(; it != peers.end() && it->signature < signature; it++)
    {
        PeerRecord& peer = *it;
        sockaddr_in peeraddr = {AF_INET, htons(peer.signature.port), { peer.signature.ip.all }};
        
        EXIT_ON_ERROR(peer.sockfd = socket(AF_INET, SOCK_STREAM, 0));

        int connected;
        while((connected = connect(peer.sockfd, reinterpret_cast<sockaddr*>(&peeraddr), sizeof(peeraddr))) < 0 && errno == ECONNREFUSED)
        {
            usleep(50);
        }
        EXIT_ON_ERROR(connected);
        
        EXIT_ON_ERROR(send(peer.sockfd, &signature, sizeof(signature), 0));
        
        DINFO("Connected to peer. (" << peer.signature << ")");
    }
    
    DINFO("Binding to TCP port...");
    EXIT_ON_ERROR(bind(tcpfd, reinterpret_cast<sockaddr*>(&addrtcp), sizeof(addrtcp)));

    DINFO("Listening to TCP port...");
    EXIT_ON_ERROR(listen(tcpfd, 16));
    
    DINFO("Accepting peers with higher node signature value (ip:port)...");
    int higherNodesCount = peers.end() - it;
    for(int i = 0; i < higherNodesCount; i++)
    {
        NodeSignature newPeerSignature;
        int tempfd;
        
        EXIT_ON_ERROR(tempfd = accept(tcpfd, nullptr, nullptr)); 
        
        int rcount = 0;
        while(rcount < sizeof(newPeerSignature))
        {
            int receivedBytes;
            EXIT_ON_ERROR(receivedBytes = recv(tempfd, reinterpret_cast<char*>(&newPeerSignature) + rcount, sizeof(newPeerSignature) - rcount, 0));
            rcount += receivedBytes;
        }
        
        auto peerit = lower_bound(peers.begin(), peers.end(), PeerRecord { newPeerSignature }, PeerRecord::CompareBySignature);
        if(peerit == peers.end()) {
            DERROR("Unknown peer connected.", -1);
        }
        peerit->sockfd = tempfd;
        DINFO("Peer is connected. (" << newPeerSignature << ")");
    }
    
    DINFO("All peers are connected (if any).");
    DINFO("Initializing polling process...");

    struct epoll_event ev = { EPOLLIN };

    EXIT_ON_ERROR(epollfd = epoll_create1(0));
    DINFO("Created polling file descriptor (fd=" << epollfd << ").");

    ev.data.fd = udpfd;
    EXIT_ON_ERROR(epoll_ctl(epollfd, EPOLL_CTL_ADD, udpfd, &ev));
    DINFO("UDP file descriptor (fd=" << udpfd << ") added to polling pool.");

    ev.data.fd = updatetimerfd;
    EXIT_ON_ERROR(epoll_ctl(epollfd, EPOLL_CTL_ADD, updatetimerfd, &ev));
    DINFO("Update timer file descriptor (fd=" << updatetimerfd << ") added to polling pool.");

    ev.data.fd = cloudtimerfd;
    EXIT_ON_ERROR(epoll_ctl(epollfd, EPOLL_CTL_ADD, cloudtimerfd, &ev));
    DINFO("Cloud timer file descriptor (fd=" << cloudtimerfd << ") added to polling pool.");

    ev.data.fd = fogtimerfd;
    EXIT_ON_ERROR(epoll_ctl(epollfd, EPOLL_CTL_ADD, fogtimerfd, &ev));
    DINFO("Fog timer file descriptor (fd=" << fogtimerfd << ") added to polling pool.");

    for(int ind = 0; ind < peers.size(); ind++)
    {
        ev.data.fd = encodeIndex(ind);
        EXIT_ON_ERROR(epoll_ctl(epollfd, EPOLL_CTL_ADD, peers[ind].sockfd, &ev));
        DINFO("Socket file descriptor (fd=" << peers[ind].sockfd << ") added to polling pool for the peer " << peers[ind].signature);
    }

    DINFO("Binding to UDP port...");
    EXIT_ON_ERROR(bind(udpfd, reinterpret_cast<sockaddr*>(&addrudp), sizeof(addrudp)));
    
    // epoll fd list: UDP fd, Update Timer fd, Fog Queue Timer fd, Cloud Queue Timer fd and a TCP fd for each peer.
    epollFdCount = 4 + peers.size();
    
    DINFO("Initialization is done.");
}

void FogNode::run()
{
    unique_ptr<epoll_event[]> events(new epoll_event[epollFdCount]);
    
    DINFO("Starting peer update timer...");
    setTimer(updatetimerfd, 0, 50, updateInterval, 0);
    
    queueDelay = 0;
    while(true)
    {
        int nfds;
        
        EXIT_ON_ERROR(nfds = epoll_wait(epollfd, events.get(), epollFdCount, -1));
        
        for(auto i = 0; i < nfds; i++)
        {
            auto ev = events[i].events;
            auto fd = events[i].data.fd;
            if(ev & (EPOLLERR | EPOLLHUP)) {
                DERROR("Problem in file descriptor (fd=" << fd << ")", -1);
            }
            if(!(ev & EPOLLIN)) {
                DERROR("File descriptor (fd=" << fd << ") returned with no input.", -1);
            }
            if(fd == udpfd) {
                handleIOTMessage();
            } else if(fd == updatetimerfd) {
                readTimer(updatetimerfd);
                updatePeers();
            } else if(fd == fogtimerfd) {
                readTimer(fogtimerfd);
                popFromQueue(false);
            } else if(fd == cloudtimerfd) {
                readTimer(cloudtimerfd);
                popFromQueue(true);
            } else {
                int peerInd = decodeIndex(fd);
                if(peers[peerInd].sockfd >= 0) {
                    handlePeerMessage(peers[peerInd]);
                }
            }
        }
    }
}

void FogNode::handleIOTMessage()
{
    int recvbytes;
    do {
        char buff[RECV_BUFFER_SIZE];
        char hostname[256];
        recvbytes = recvfrom(udpfd, buff, RECV_BUFFER_SIZE, MSG_DONTWAIT, nullptr, nullptr);
        if(recvbytes >= 0 || !((errno & EWOULDBLOCK) || (errno & EAGAIN))) {
            EXIT_ON_ERROR(recvbytes);

            IOTRequestPacketWrapper pw;
            IOTRequestPacket& packet = pw.packet;
            stringstream ss(string(buff, recvbytes));
            ss.ignore(2) >> packet.seq_num;
            ss.ignore(3) >> packet.req_type;
            ss.ignore(4) >> packet.forw_lim;
            ss.ignore(4).get(hostname, sizeof(hostname), ' ');
            packet.src.ip.all = get_ip_addr(hostname);
            ss.ignore(3) >> packet.src.port;
            packet.visited_cnt = 0;
            DINFO("Received Request{" << packet.seq_num << "} from IoT node: " << string(buff, recvbytes) << " (" << packet.req_type << ", " << packet.forw_lim << ", " << packet.src << ", " << packet.visited_cnt << ")");

            handleRequest(pw);
        }
    } while(recvbytes > 0);
}

void FogNode::updatePeers()
{
    PeerUpdatePacket updatePacket { PeerPacketHeader { PEER_UPDATE_PACKET_TYPE }, static_cast<uint32_t>(queueDelay) };
    for(auto& peer : peers)
    {
        DINFO("Sending update to peer " << peer.signature);
        EXIT_ON_ERROR(send(peer.sockfd, &updatePacket, sizeof(updatePacket), 0));
    }
}

void FogNode::handlePeerMessage(PeerRecord& peer)
{
    char buff[RECV_BUFFER_SIZE];
    int recvbytes;
    do
    {
        recvbytes = recv(peer.sockfd, buff, RECV_BUFFER_SIZE, MSG_DONTWAIT);
        if(recvbytes >= 0 || !((errno & EWOULDBLOCK) || (errno & EAGAIN))) {
            EXIT_ON_ERROR(recvbytes);
            peer.buffer.insert(peer.buffer.end(), buff, buff+recvbytes);
            DINFO("Received message (" << recvbytes << " bytes) from peer " << peer.signature);
            /*for(int i = 0; i < recvbytes; i ++)
            {
                DRAW(setfill('0') << setw(2) << setbase(16) << (int)*(unsigned char*)(buff+i) << " " << setbase(10));
            }
            DRAW(endl);*/
        }
    } while(recvbytes > 0);
    
    if(recvbytes == 0) {
        DINFO(peer.signature << " has disconnected. Deleting its fd from epoll.");
        EXIT_ON_ERROR(epoll_ctl(epollfd, EPOLL_CTL_DEL, peer.sockfd, nullptr));
        close(peer.sockfd);
        peer.sockfd = -1;
    }
    
    bool possiblyAvailable;
    do
    {
        auto& curPacketType = peer.curPacketType;
        auto& curRequest = peer.curRequest;
        auto& buffer = peer.buffer;
        
        possiblyAvailable = false;
        if(curPacketType == 0) {
            if(buffer.size() >= sizeof(PeerPacketHeader::packet_type))
            {
                decltype(PeerPacketHeader::packet_type) packet_type;
                peer.readFromBuffer(0, sizeof(PeerPacketHeader::packet_type), &packet_type);
                curPacketType = packet_type;
                possiblyAvailable = true;
            }
        } else if(curPacketType == PEER_UPDATE_PACKET_TYPE) {
            if(buffer.size() >= sizeof(PeerUpdatePacket) - sizeof(PeerPacketHeader::packet_type)) {
                PeerUpdatePacket packet = {};
                peer.readFromBuffer(sizeof(PeerPacketHeader::packet_type), sizeof(packet), &packet);
                DINFO("Update packet received from peer " << peer.signature << " with delay " << packet.queue_delay);
                peer.queueDelay = packet.queue_delay;
                
                sort(peersByQueueDelay.begin(), peersByQueueDelay.end(), PeerRecord::CompareByQueueDelayRef);
                
                curPacketType = 0;
                possiblyAvailable = true;
            }
        } else if(curPacketType == PEER_FORWARD_PACKET_TYPE) {
            if(curRequest == nullptr) {
                if(buffer.size() >= sizeof(PeerForwardPacket) - sizeof(PeerPacketHeader::packet_type)) {
                    PeerForwardPacket packet = {};
                    peer.readFromBuffer(sizeof(PeerPacketHeader::packet_type), sizeof(packet), &packet);
                    DINFO("Forward packet received from peer " << peer.signature << ". (Request{" << packet.payload.seq_num << "})");

                    curRequest = shared_ptr<IOTRequestPacketWrapper>(new IOTRequestPacketWrapper);
                    curRequest->packet = packet.payload;
                    possiblyAvailable = true;
                }
            } else {
                auto& curPacket = curRequest->packet;
                if(buffer.size() >= sizeof(NodeSignature) * curPacket.visited_cnt) {
                    auto& visitedNodes = curRequest->visitedNodes;
                    for(int i = 0; i < curPacket.visited_cnt; i++)
                    {
                        NodeSignature newSignature;
                        peer.readFromBuffer(0, sizeof(NodeSignature), &newSignature);
                        visitedNodes.push_back(newSignature);
                    }
                    for(int i = 0; i < visitedNodes.size(); i++)
                    {
                        DINFO("- Request{" << curRequest->packet.seq_num << "} has been marked as visited " << visitedNodes[i]);
                    }
                    handleRequest(*curRequest);
                    
                    curRequest = nullptr;
                    curPacketType = 0;
                    possiblyAvailable = true;
                }
            }
        } else {
            DERROR("Unkown packet type.", -1);
        }
    } while(possiblyAvailable);
}

void FogNode::handleRequest(IOTRequestPacketWrapper& pw)
{
    DINFO("Request{" << pw.packet.seq_num << "} is being processed... (delay: " << queueDelay << ", required: " << pw.packet.req_type << ", maxResponseTime: " << maxResponseTime << ")");
    if(queueDelay + pw.packet.req_type <= maxResponseTime) {
        pushToQueue(pw, false);
    } else if(pw.packet.forw_lim == 0) {
        DINFO("Request{" << pw.packet.seq_num << "} cannot be forwarded any more.");
        pushToQueue(pw, true);
    } else {
        forwardPacket(pw);
    }
}

void FogNode::popFromQueue(bool isCloud)
{
    IOTRequestPacketWrapper pw;
    if(isCloud) {
        pw = cloudQueue.front();
        cloudQueue.pop();
        DINFO("Request{" << pw.packet.seq_num << "} has been completed in the cloud.");
        if(cloudQueue.size() > 0) {
            handleNextRequestInQueue(true);
        }
    } else {
        pw = fogQueue.front();
        fogQueue.pop();
        DINFO("Request{" << pw.packet.seq_num << "} has been completed in the fog.");
        if(fogQueue.size() > 0) {
            handleNextRequestInQueue(false);
        }
        queueDelay -= pw.packet.req_type;
    }

    #ifdef DEBUG
    stringstream os;
    _debug_print_timestamp(os);
    os << " Request{" << pw.packet.seq_num << "}, FL:" << pw.packet.forw_lim << ", P: ";
    for(auto& visitedSignature : pw.visitedNodes)
    {
        os << visitedSignature << "->";
    }
    os << signature;
    if(isCloud) {
        os << " (cloud)";
    }
    
    sockaddr_in destaddr = { AF_INET, htons(pw.packet.src.port), { pw.packet.src.ip.all } };
    EXIT_ON_ERROR(sendto(udpfd, os.str().data(), os.str().length(), 0, reinterpret_cast<sockaddr*>(&destaddr), sizeof(destaddr)));
    #endif
}

void FogNode::pushToQueue(IOTRequestPacketWrapper& pw, bool isCloud)
{
    if(isCloud) {
        DINFO("Request{" << pw.packet.seq_num << "} is being added to the cloud queue...");
        cloudQueue.push(pw);
        if(cloudQueue.size() == 1) {
            handleNextRequestInQueue(true);
        }
    } else {
        DINFO("Request{" << pw.packet.seq_num << "} is being added to the fog queue...");
        fogQueue.push(pw);
        if(fogQueue.size() == 1) {
            handleNextRequestInQueue(false);
        }
        queueDelay += pw.packet.req_type;
    }
}

void FogNode::forwardPacket(IOTRequestPacketWrapper& pw)
{
    DINFO("Request{" << pw.packet.seq_num << "} is being forwarded...");
    
    auto& visitedNodes = pw.visitedNodes;
    
    PeerRecord* bestPeer = nullptr;
    bool bestPeerExists = false;
    for(auto itBest = peersByQueueDelay.begin(); itBest != peersByQueueDelay.end(); itBest++)
    {
        bestPeerExists = true;
        
        for(auto it = visitedNodes.begin(); bestPeerExists && it != visitedNodes.end(); it++) {
            DINFO("Candidate: " << itBest->get().signature << ", comparing to " << *it << "...");
            if(*it == itBest->get().signature) {
                DINFO("Nope!");
                bestPeerExists = false;
            }
        }
        
        if(bestPeerExists) {
            bestPeer = &(itBest->get());
            break;
        }
    }
    
    if(!bestPeerExists) {
        DINFO("Request{" << pw.packet.seq_num << "} has already visited all possible neighbors...");
        pushToQueue(pw, true);
    } else {
        PeerForwardPacket fwPacket = { { PEER_FORWARD_PACKET_TYPE}, pw.packet };
        
        fwPacket.payload.forw_lim--;
        fwPacket.payload.visited_cnt++;
        EXIT_ON_ERROR(send(bestPeer->sockfd, &fwPacket, sizeof(fwPacket), 0));
        
        visitedNodes.push_back(signature);
        EXIT_ON_ERROR(send(bestPeer->sockfd, &visitedNodes[0], sizeof(NodeSignature) * visitedNodes.size(), 0));
        DINFO("Request{" << pw.packet.seq_num << "} has been forwarded to " << bestPeer->signature);
    }
}

void FogNode::handleNextRequestInQueue(bool isCloud)
{
    if(isCloud) {
        IOTRequestPacketWrapper& pw = cloudQueue.front();
        int processMs = pw.packet.req_type * 1000 / 100;
        setTimer(cloudtimerfd, processMs / 1000, processMs % 1000);
        DINFO("Request{" << pw.packet.seq_num << "} is being processed by the cloud...");
    } else {
        IOTRequestPacketWrapper& pw = fogQueue.front();
        setTimer(fogtimerfd, pw.packet.req_type);
        DINFO("Request{" << pw.packet.seq_num << "} is being processed by the fog...");
    }
}

int FogNode::encodeIndex(int ind)
{
    return -ind-1;
}

int FogNode::decodeIndex(int val)
{
    return -val-1;
}

uint64_t FogNode::readTimer(int timerfd)
{
    uint64_t times;
    EXIT_ON_ERROR(read(timerfd, &times, sizeof(times)));
    DINFO("Timer (fd=" << timerfd << ") is up " << times << " times.");
    return times;
}

void FogNode::setTimer(int timerfd, int initialSecs, int initialMs, int intervalSecs, int intervalMs)
{
    DINFO("Setting timer (fd=" << timerfd << "): initial(" << initialSecs << "s, " << initialMs << "ms), interval(" << intervalSecs << "s, " << intervalMs << "ms)");
    EXIT_ON_ERROR(timerfd_settime(timerfd, 0, shared_ptr<itimerspec>(new itimerspec { { intervalSecs, intervalMs * 1000000 }, { initialSecs, initialMs * 1000000 } }).get(), nullptr));
}


