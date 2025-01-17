/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <csignal>
#include <cstring>

#include "signalfd.hh"
#include "exception.hh"

using namespace std;

/* add given signals to the mask */
SignalMask::SignalMask( const initializer_list< int > signals )
    : mask_()
{
    SystemCall( "sigemptyset", sigemptyset( &mask_ ) );

    for ( const auto signal : signals ) {
        SystemCall( "sigaddset", sigaddset( &mask_, signal ) );
    }
}

/* get the current mask */
SignalMask SignalMask::current_mask( void )
{
    SignalMask mask = {};

    SystemCall( "sigprocmask", sigprocmask( SIG_BLOCK, nullptr, &mask.mask_ ) );

    return mask;
}

/* challenging to compare two sigset_t's for equality */
bool SignalMask::operator==( const SignalMask & other ) const
{
    for ( int signum = 0; signum < SIGRTMAX; signum++ ) {
        if ( sigismember( &mask_, signum ) != sigismember( &other.mask_, signum ) ) {
            return false;
        }
    }

    return true;
}

/* mask these signals from interrupting our process */
/* (because we'll use a signalfd instead to read them */
void SignalMask::set_as_mask( void ) const
{
    SystemCall( "sigprocmask", sigprocmask( SIG_SETMASK, &mask_, nullptr ) );
}

SignalFD::SignalFD( const SignalMask & signals )
    : fd_( SystemCall( "signalfd", signalfd( -1, &signals.mask(), 0 ) ) )
{
}

/* read one signal */
// 读取这个信号，好让poller不再被触发   
signalfd_siginfo SignalFD::read_signal( void )
{
    signalfd_siginfo delivered_signal;

    string delivered_signal_str = fd_.read( sizeof( signalfd_siginfo ) );

    if ( delivered_signal_str.size() != sizeof( signalfd_siginfo ) ) {
        throw runtime_error( "signalfd read size mismatch" );
    }

    memcpy( &delivered_signal, delivered_signal_str.data(), sizeof( signalfd_siginfo ) );

    return delivered_signal;
}
