#ifndef _6390_PROJECT_PACKETS_H_
#define _6390_PROJECT_PACKETS_H_

#include <cstdint>

#include <ostream>
#include <string>
#include <vector>

using namespace std;

union IPAddr
{
    uint32_t all;
    uint8_t bytes[4];
};

struct NodeSignature
{
    IPAddr ip;
    uint16_t port;
    
    bool operator==(const NodeSignature& rhs)
    {
        return ip.all == rhs.ip.all && port == rhs.port;
    }

    NodeSignature& operator=(const NodeSignature& rhs)
    {
        ip.all = rhs.ip.all;
        port = rhs.port;
        return *this;
    }
    
    friend bool operator<(const NodeSignature& lhs, const NodeSignature& rhs)
    {
        return lhs.ip.all < rhs.ip.all || (lhs.ip.all == rhs.ip.all && lhs.port < rhs.port);
    }

    friend ostream& operator<<(ostream& os, const NodeSignature& signature)
    {
        os << (
            to_string(signature.ip.bytes[0]) + "." +
            to_string(signature.ip.bytes[1]) + "." +
            to_string(signature.ip.bytes[2]) + "." +
            to_string(signature.ip.bytes[3]) + ":" +
            to_string(signature.port)
        );
        return os;
    }
};

struct IOTRequestPacket
{
    uint32_t seq_num;
    uint16_t req_type;
    uint16_t forw_lim;
    NodeSignature src;
    uint16_t visited_cnt;
};


class IOTRequestPacketWrapper
{
    public:
        IOTRequestPacketWrapper() {}
        IOTRequestPacket packet;
        vector<NodeSignature> visitedNodes;
};

struct PeerPacketHeader
{
    uint16_t packet_type;
};

#define PEER_UPDATE_PACKET_TYPE  (1)
#define PEER_FORWARD_PACKET_TYPE (2)

struct PeerUpdatePacket
{
    PeerPacketHeader header;
    uint32_t queue_delay;
};

struct PeerForwardPacket
{
    PeerPacketHeader header;
    IOTRequestPacket payload;
};

#endif /* _6390_PROJECT_PACKETS_H_ */

