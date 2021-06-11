/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*
 Copyright (C) 2004, 2005, 2007 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file singleton.hpp
    \brief basic support for the singleton pattern
           QRM: add thread safe singleton init for QL_ENABLE_SEESION
*/

#ifndef quantlib_singleton_hpp
#define quantlib_singleton_hpp

#include <ql/qldefines.hpp>

#ifdef QL_ENABLE_SINGLETON_THREAD_SAFE_INIT
    #if defined(QL_ENABLE_SESSIONS)
        #ifdef BOOST_MSVC
            #pragma message(\
                "Thread-safe singleton initialization not supported "  \
                "when sessions are enabled.")
        #else
            #warning \
                Thread-safe singleton initialization not supported \
                when sessions are enabled.
        #endif
    #else
        #include <boost/atomic.hpp>
        #if !defined(BOOST_ATOMIC_ADDRESS_LOCK_FREE)
            #ifdef BOOST_MSVC
                #pragma message(\
                    "Thread-safe singleton initialization "  \
                    "may degrade performances.")
            #else
                #warning \
                    Thread-safe singleton initialization \
                    may degrade performances.
            #endif
        #endif
        #define QL_SINGLETON_THREAD_SAFE_INIT
    #endif
#endif

#if defined(QL_SINGLETON_THREAD_SAFE_INIT)
#include <boost/thread/mutex.hpp>
#endif

#if defined(QL_ENABLE_SESSIONS)
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#endif

#include <ql/types.hpp>
#include <ql/shared_ptr.hpp>
#if defined(QL_PATCH_MSVC)
    #pragma managed(push, off)
#endif
#include <boost/noncopyable.hpp>
#if defined(QL_PATCH_MSVC)
    #pragma managed(pop)
#endif
#include <map>


#if (_MANAGED == 1) || (_M_CEE == 1)
// One of the Visual C++ /clr modes. In this case, the global instance
// map must be declared as a static data member of the class.
#define QL_MANAGED 1
#else
// Every other configuration. The map can be declared as a static
// variable inside the creation method.
#define QL_MANAGED 0
#endif

namespace QuantLib {

    #if defined(QL_ENABLE_SESSIONS)
    // definition must be provided by the user
    Integer sessionId();
    #endif

    // this is required on VC++ when CLR support is enabled
    #if defined(QL_PATCH_MSVC)
        #pragma managed(push, off)
    #endif

    //! Basic support for the singleton pattern.
    /*! The typical use of this class is:
        \code
        class Foo : public Singleton<Foo> {
            friend class Singleton<Foo>;
          private:
            Foo() {}
          public:
            ...
        };
        \endcode
        which, albeit sub-optimal, frees one from the concerns of
        creating and managing the unique instance and can serve later
        as a single implemementation point should synchronization
        features be added.

        \ingroup patterns
    */

    struct B_True {
        static bool value() { return true; }
    };

    struct B_False {
        static bool value() { return false; }
    };

    template <class T, class B = B_False>
    class Singleton : private boost::noncopyable {
      private:
    #if (QL_MANAGED == 1) && !defined(QL_SINGLETON_THREAD_SAFE_INIT)
        static std::map<Integer, ext::shared_ptr<T> > instances_;
    #endif

    #if defined(QL_SINGLETON_THREAD_SAFE_INIT)
        static boost::atomic<T*> instance_;
    #endif
    #if defined(QL_SINGLETON_THREAD_SAFE_INIT)
        static boost::mutex mutex_;
    #endif
    #if defined(QL_ENABLE_SESSIONS)
        static boost::shared_mutex mutex_;
    #endif

      public:
        //! access to the unique instance
        static T& instance();
      protected:
      Singleton() {}
    };

    // static member definitions

    #if (QL_MANAGED == 1) && !defined(QL_SINGLETON_THREAD_SAFE_INIT)
      template <class T, class B>
      std::map<Integer, ext::shared_ptr<T> > Singleton<T, B>::instances_;
    #endif

    #if defined(QL_SINGLETON_THREAD_SAFE_INIT)
    template <class T, class B> boost::atomic<T*> Singleton<T, B>::instance_;
    #endif
    #if defined(QL_SINGLETON_THREAD_SAFE_INIT)
    template <class T, class B> boost::mutex Singleton<T, B>::mutex_;
    #endif
    #if defined(QL_ENABLE_SESSIONS)
    template <class T, class B>
    boost::shared_mutex Singleton<T, B>::mutex_;
    #endif

    // template definitions

    template <class T, class B>
    T& Singleton<T, B>::instance() {

        #if (QL_MANAGED == 0) && !defined(QL_SINGLETON_THREAD_SAFE_INIT)
        static std::map<Integer, ext::shared_ptr<T> > instances_;
        #endif

        // thread safe double checked locking pattern with atomic memory calls
        #if defined(QL_SINGLETON_THREAD_SAFE_INIT)

        T* instance =  instance_.load(boost::memory_order_consume);

        if (!instance) {
            boost::mutex::scoped_lock guard(mutex_);
            instance = instance_.load(boost::memory_order_consume);
            if (!instance) {
                instance = new T();
                instance_.store(instance, boost::memory_order_release);
            }
        }

        #else

        #if defined(QL_ENABLE_SESSIONS)
        // thread safe
        Integer id = B::value() ? 0 : sessionId();
        const std::map<Integer, ext::shared_ptr<T> >& i = instances_;
        boost::upgrade_lock<boost::shared_mutex> sharedLock(mutex_);
        typename std::map<Integer, ext::shared_ptr<T> >::const_iterator instance = i.find(id);
        if(instance != i.end())
            return *instance->second;
        else
        {
            boost::upgrade_to_unique_lock<boost::shared_mutex> uniqueLock(sharedLock);
            ext::shared_ptr<T> tmp(new T);
            instances_[id] = tmp;
            return *tmp;
        }
        #else
        ext::shared_ptr<T>& instance = instances_[0];
        if (!instance)
            instance = ext::shared_ptr<T>(new T);
        return *instance;
        #endif

        #endif
    }

    // reverts the change above
    #if defined(QL_PATCH_MSVC)
        #pragma managed(pop)
    #endif

}

#undef QL_MANAGED

#endif
