/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <algorithm>

#include "event_loop.hh"
#include "exception.hh"

using namespace std;
using namespace PollerShortNames;

// ! ctor
EventLoop::EventLoop()
    : signals_( { SIGCHLD, SIGCONT, SIGHUP, SIGTERM, SIGQUIT, SIGINT } ),
      poller_(),
      child_processes_()
{
    signals_.set_as_mask(); /* block signals so we can later use signalfd to read them */
}

// 为了不让用户自己创建Poller::Action
void EventLoop::add_simple_input_handler( FileDescriptor & fd,
                                          const Poller::Action::CallbackType & callback )
{
    poller_.add_action( Poller::Action( fd, Direction::In, callback ) );
}

// 处理具体的信号, 用于传入 poller ,在poller线程被调用
Result EventLoop::handle_signal( const signalfd_siginfo & sig )
{
    switch ( sig.ssi_signo ) {
    case SIGCONT:
        /* resume child processes too */
        for ( auto & x : child_processes_ ) {
            x.second.resume(); // 向进程发送SIGCONT
        }
        break;

    // ? 这里在干啥
    case SIGCHLD: // 子进程结束
        if ( child_processes_.empty() ) {
            throw runtime_error( "received SIGCHLD without any managed children" );
        }

        /* find which children are waitable */
        /* we can't count on getting exactly one SIGCHLD per waitable event, so search */
        for ( auto & procpair : child_processes_ ) {
            ChildProcess & proc = procpair.second;
            if ( proc.terminated() or ( !proc.waitable() ) ) {
                continue; /* not the process we're looking for */
            }

            proc.wait( true ); /* get process's change of state.
                               true => throws exception if no change available */

            if ( proc.terminated() ) {
                if ( proc.exit_status() != 0 and proc.exit_status() != procpair.first ) {
                    proc.throw_exception();
                }

                /* quit if all children have quit */
                if ( all_of( child_processes_.begin(), child_processes_.end(),
                             [] ( pair<int, ChildProcess> & x ) { return x.second.terminated(); } ) ) {
                    return ResultType::Exit;
                }

                return proc.exit_status() == procpair.first ? ResultType::Continue : ResultType::Exit;
            } else if ( !proc.running() ) {
                /* suspend parent too */
                SystemCall( "raise", raise( SIGSTOP ) ); // raise支持给自己发信号
            }
        }

        break;

    case SIGHUP:
    case SIGTERM:
    case SIGQUIT:
    case SIGINT:
        return ResultType::Exit; //  从而让poller也退出, 从而让eventloop也退出
    default:
        throw runtime_error( "EventLoop: unknown signal" );
    }

    return ResultType::Continue; // 让poller 继续poll
}

 // ? poll超时时间也需要是个函数？为什么？
 // * 这里wait_time没有是用户函数过
int EventLoop::internal_loop( const std::function<int(void)> & wait_time )
{
    // RAII 手法 -> 实现暂时的非root -> ?为什么
    // ? 为什么要非root？ 
    // 调用mm-link的时候也要求非root，为什么
    TemporarilyUnprivileged tu;

    // ? 为什么要验证？
    /* verify that signal mask is intact */ 
    SignalMask current_mask = SignalMask::current_mask();

    if ( !( signals_ == current_mask ) ) {
        throw runtime_error( "EventLoop: signal mask has been altered" );
    }

    // ? signal fd ? 把信号也变成能poll的东西？
    SignalFD signal_fd( signals_ ); // 传入一个mask, 转换为一个fd

    /* we get signal -> main screen turn on */
    add_simple_input_handler( signal_fd.fd(),
                              [&] () { return handle_signal( signal_fd.read_signal() ); } );

    while ( true ) {
        const auto poll_result = poller_.poll( wait_time() );
        if ( poll_result.result == Poller::Result::Type::Exit ) { // 
            // * 当poller回调返回exit时，poller传递这个exit到eventloop
            // * 当fd错误时
            // * 当没有fd可以poll的时候
            return poll_result.exit_status;
        }
        // ！ 这里没有处理超时的情况
    }
}
