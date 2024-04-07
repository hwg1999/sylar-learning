#pragma once

#include "sylar/fiber.h"
#include "sylar/thread.h"
#include <atomic>
#include <cstddef>
#include <functional>
#include <list>
#include <memory>
#include <vector>

namespace sylar {

class Scheduler
{
public:
  using SPtr = std::shared_ptr<Scheduler>;
  using MutexType = Mutex;

  Scheduler( std::size_t threads = 1, bool use_caller = true, const std::string& name = "" );
  virtual ~Scheduler();

  const std::string& getName() const { return m_name; }

  static Scheduler* GetThis();
  static Fiber* GetMainFiber();

  void start();
  void stop();

  template<typename FiberOrCb>
  void schedule( FiberOrCb fc, int thread = -1 )
  {
    bool need_tickle { false };
    {
      MutexType::Lock lock { m_mutex };
      need_tickle = scheduleNoLock( fc, thread );
    }

    if ( need_tickle ) {
      tickle();
    }
  }

  template<typename InputIterator>
  void schedule( InputIterator begin, InputIterator end )
  {
    bool need_tickle { false };
    {
      MutexType::Lock lock { m_mutex };
      while ( begin != end ) {
        need_tickle = scheduleNoLock( &*begin ) || need_tickle;
      }

      if ( need_tickle ) {
        tickle();
      }
    }
  }

protected:
  virtual void tickle();
  void run();
  virtual bool stopping();
  virtual void idle();

  void setThis();

private:
  template<typename FiberOrCb>
  bool scheduleNoLock( FiberOrCb fc, int thread )
  {
    bool need_tickle { m_fibers.empty() };
    FiberAndThread ft { fc, thread };
    if ( ft.fiber || ft.cb ) {
      m_fibers.push_back( ft );
    }
    return need_tickle;
  }

private:
  struct FiberAndThread
  {
    Fiber::SPtr fiber;
    std::function<void()> cb;
    int thread;

    FiberAndThread( Fiber::SPtr f, int thr ) : fiber( f ), thread( thr ) {}
    FiberAndThread( Fiber::SPtr* f, int thr ) : thread( thr ) { fiber.swap( *f ); }
    FiberAndThread( std::function<void()> f, int thr ) : cb( f ), thread( thr ) {}
    FiberAndThread( std::function<void()>* f, int thr ) : thread( thr ) { cb.swap( *f ); }
    FiberAndThread() : thread( -1 ) {}

    void reset()
    {
      fiber = nullptr;
      cb = nullptr;
      thread = -1;
    }
  };

private:
  MutexType m_mutex;
  std::vector<Thread::SPtr> m_threads;
  std::list<FiberAndThread> m_fibers;
  Fiber::SPtr m_rootFiber;
  std::string m_name;

protected:
  std::vector<int> m_threadIds;
  std::size_t m_threadCount { 0 };
  std::atomic<std::size_t> m_activeThreadCount { 0 };
  std::atomic<std::size_t> m_idleThreadCount { 0 };
  bool m_stopping { true };
  bool m_autoStop { false };
  int m_rootThread { 0 };
};

}