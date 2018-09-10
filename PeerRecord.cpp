#include "PeerRecord.h"

void PeerRecord::readFromBuffer(int start, int end, void *buf)
{
    auto cbuf = static_cast<char*>(buf);
    while(start < end)
    {
        auto b = buffer.front();
        cbuf[start] = b;
        buffer.pop_front();
        start++;
    }
    /*
    copy_n(buffer.begin(), end - start, );
    buffer.erase(buffer.begin(), buffer.begin() + (end - start));
    */
}

bool PeerRecord::CompareBySockFd(const PeerRecord& lhs, const PeerRecord& rhs)
{
    return lhs.sockfd < rhs.sockfd;
}

bool PeerRecord::CompareBySockFdRef(reference_wrapper<PeerRecord> lhs, reference_wrapper<PeerRecord> rhs)
{
    return CompareBySockFd(lhs.get(), rhs.get());
}

bool PeerRecord::CompareBySignature(const PeerRecord& lhs, const PeerRecord& rhs)
{
    return lhs.signature < rhs.signature;
}

bool PeerRecord::CompareBySignatureRef(reference_wrapper<PeerRecord> lhs, reference_wrapper<PeerRecord> rhs)
{
    return CompareBySignature(lhs.get(), rhs.get());
}

bool PeerRecord::CompareByQueueDelay(const PeerRecord& lhs, const PeerRecord& rhs)
{
    return lhs.queueDelay < rhs.queueDelay;
}

bool PeerRecord::CompareByQueueDelayRef(reference_wrapper<PeerRecord> lhs, reference_wrapper<PeerRecord> rhs)
{
    return CompareByQueueDelay(lhs.get(), rhs.get());
}

