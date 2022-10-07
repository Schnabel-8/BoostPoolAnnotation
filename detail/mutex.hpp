// Copyright (C) 2000 Stephen Cleary
// Copyright (C) 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org for updates, documentation, and revision history.

#ifndef BOOST_POOL_MUTEX_HPP
#define BOOST_POOL_MUTEX_HPP

#include <boost/config.hpp>

namespace boost{ namespace details{ namespace pool{

//不加锁
class null_mutex
{
//禁止拷贝
private:

    null_mutex(const null_mutex &);
    void operator=(const null_mutex &);

public:
//只用到了这三个接口
    null_mutex() {}

    static void lock() {}
    static void unlock() {}
};

}}} // namespace boost::details::pool

//根据define定义default_mutex
//
#if !defined(BOOST_HAS_THREADS) || defined(BOOST_NO_MT) || defined(BOOST_POOL_NO_MT)

namespace boost{ namespace details{ namespace pool{

typedef null_mutex default_mutex;

}}} // namespace boost::details::pool


//适应不同的C++版本
//如果是C++11以后，直接使用STL的mutex
#elif !defined(BOOST_NO_CXX11_HDR_MUTEX)


#include <mutex>

namespace boost{ namespace details{ namespace pool{

typedef std::mutex default_mutex;

}}} // namespace boost::details::pool


//如果是多线程
//使用pthread
#elif defined(BOOST_HAS_PTHREADS)



#include <boost/assert.hpp>
#include <pthread.h>

namespace boost{ namespace details{ namespace pool{

class pt_mutex
{
private:

    pthread_mutex_t m_;

    pt_mutex(pt_mutex const &);
    pt_mutex & operator=(pt_mutex const &);

public:

    pt_mutex()
    {
        BOOST_VERIFY( pthread_mutex_init( &m_, 0 ) == 0 );
    }

    ~pt_mutex()
    {
        BOOST_VERIFY( pthread_mutex_destroy( &m_ ) == 0 );
    }

    void lock()
    {
        BOOST_VERIFY( pthread_mutex_lock( &m_ ) == 0 );
    }

    void unlock()
    {
        BOOST_VERIFY( pthread_mutex_unlock( &m_ ) == 0 );
    }
};

typedef pt_mutex default_mutex;

}}} // namespace boost::details::pool



//还可以使用windows_api
#elif defined(BOOST_HAS_WINTHREADS) || defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)




#include <boost/winapi/critical_section.hpp>

namespace boost{ namespace details{ namespace pool{

class cs_mutex
{
private:

    boost::winapi::CRITICAL_SECTION_ cs_;

    cs_mutex(cs_mutex const &);
    cs_mutex & operator=(cs_mutex const &);

public:

    cs_mutex()
    {
        boost::winapi::InitializeCriticalSection( &cs_ );
    }

    ~cs_mutex()
    {
        boost::winapi::DeleteCriticalSection( &cs_ );
    }

    void lock()
    {
        boost::winapi::EnterCriticalSection( &cs_ );
    }

    void unlock()
    {
        boost::winapi::LeaveCriticalSection( &cs_ );
    }
};

typedef cs_mutex default_mutex;

}}} // namespace boost::details::pool

#else


//如果都不是就报错
// Use #define BOOST_DISABLE_THREADS to avoid this error
#  error Unrecognized threading platform

#endif

#endif // #ifndef BOOST_POOL_MUTEX_HPP
