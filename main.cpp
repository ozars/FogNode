#include "Debug.h"
#include "FogNode.h"
#include "Packets.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

int main(int argc, char *argv[])
{
    if(argc < 6 || argc % 2) {
        DINFO("Wrong arguments.");
        return 1;
    }
    int peersCount = (argc - 6) / 2;
    int maxResponseTime, updateInterval;
    unsigned short udpPort;
    NodeSignature signature;
    vector<PeerRecord> peers;

    stringstream(string(argv[1])) >> maxResponseTime;
    stringstream(string(argv[2])) >> updateInterval;
    signature.ip.all = get_ip_addr(argv[3]);
    stringstream(string(argv[4])) >> udpPort;
    stringstream(string(argv[5])) >> signature.port;
    for(auto i = 0; i < peersCount; i++)
    {
        NodeSignature peerSignature;
        peerSignature.ip.all = get_ip_addr(argv[6+2*i]);
        stringstream(string(argv[6+2*i+1])) >> peerSignature.port;
        peers.push_back(PeerRecord { peerSignature, -1 });
    }

#ifdef DEBUG
    DINFO("Starting fog node " << signature << " [udp/" << udpPort << "]" << ", maxResponseTime: " << maxResponseTime << ", updateInterval: " << updateInterval);
    for(auto& p : peers)
    {
        DINFO("- Peer: " << p.signature);
    }
#endif

    FogNode fogNode(signature, updateInterval, maxResponseTime, udpPort, peers);
    fogNode.initialize();
    fogNode.run();

    return 0;
}
