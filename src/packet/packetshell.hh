/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#ifndef PACKETSHELL_HH
#define PACKETSHELL_HH

#include <string>

#include "netdevice.hh"
#include "nat.hh"
#include "util.hh"
#include "address.hh"
#include "dns_proxy.hh"
#include "event_loop.hh"
#include "socketpair.hh"


// * Using Mahimahi's RecordShell, you can completely record and store real HTTP
// * content (such as Web pages) to disk. Recorded sites can then be repeatably 
// * replayed using ReplayShell. ReplayShell accurately models a Web application's 
// * multi-server structure by locally mirroring a page's server distribution.

// * Mahimahi's network emulation tools can be used to emulate many different 
// * link conditions for replaying recorded HTTP traffic. Mahimahi supports 
// * emulating fixed propagation delays (DelayShell), fixed and variable link 
// * rates (LinkShell), stochastic packet loss (LossShell). LinkShell also 
// * supports a variety of queueing disciplines such as DropTail, DropHead, 
// * and active queue management schemes like CoDel. Each of Mahimahi's 
// * network emulation tools can be arbitrarily nested within one another 
// * providing more experimental flexibility.

// *              渡轮队列类型 : linkqueue, lossqueue...
template <class FerryQueueType>
class PacketShell
{
private:
    char ** const user_environment_;
    std::pair<Address, Address> egress_ingress;
    Address nameserver_;
    // * shell 有一个egress
    TunDevice egress_tun_;
    DNSProxy dns_outside_;
    NAT nat_rule_ {};

    std::pair<UnixDomainSocket, UnixDomainSocket> pipe_;

    // * main loop
    EventLoop event_loop_;

    const Address & egress_addr( void ) { return egress_ingress.first; }
    const Address & ingress_addr( void ) { return egress_ingress.second; }

    // * 渡轮: 从一个tun设备读取，运送到另一个tun设备(sibling)
    // * PacketShell 不拥有Ferry, 它的uplink拥有ferry
    class Ferry : public EventLoop
    {
    public:
        int loop( FerryQueueType & ferry_queue, FileDescriptor & tun, FileDescriptor & sibling );
    };

    Address get_mahimahi_base( void ) const;

public:
    PacketShell( const std::string & device_prefix, char ** const user_environment );

    template <typename... Targs>
    void start_uplink( const std::string & shell_prefix,
                       const std::vector< std::string > & command,
                       Targs&&... Fargs );

    template <typename... Targs>
    void start_downlink( Targs&&... Fargs );

    // * begin loop
    int wait_for_exit( void );

    // * 禁用复制赋值
    PacketShell( const PacketShell & other ) = delete;
    PacketShell & operator=( const PacketShell & other ) = delete;
};

#endif
