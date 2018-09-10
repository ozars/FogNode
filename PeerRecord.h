#ifndef _6390_PROJECT_PEER_RECORD_H_
#define _6390_PROJECT_PEER_RECORD_H_

#include "Packets.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>

using namespace std;

class PeerRecord
{
    public:
        NodeSignature signature;
        int sockfd;
        int queueDelay;
        
        int curPacketType;
        shared_ptr<IOTRequestPacketWrapper> curRequest;
        deque<char> buffer;
        
        void readFromBuffer(int start, int end, void *buf);

        static bool CompareBySockFd(const PeerRecord& lhs, const PeerRecord& rhs);
        static bool CompareBySockFdRef(reference_wrapper<PeerRecord> lhs, reference_wrapper<PeerRecord> rhs);
        
        static bool CompareBySignature(const PeerRecord& lhs, const PeerRecord& rhs);
        static bool CompareBySignatureRef(reference_wrapper<PeerRecord> lhs, reference_wrapper<PeerRecord> rhs);
        
        static bool CompareByQueueDelay(const PeerRecord& lhs, const PeerRecord& rhs);
        static bool CompareByQueueDelayRef(reference_wrapper<PeerRecord> lhs, reference_wrapper<PeerRecord> rhs);
};

#endif /* _6390_PROJECT_PEER_RECORD_H_ */
