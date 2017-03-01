/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2003, 2004, 2005, 2006, 2007 StatPro Italia srl
 Copyright (C) 2015 CompatibL

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

/*! \file qldefines.hpp
    \brief Global definitions and compiler switches.
*/

#ifndef quantlib_defines_hpp
/* install-hook */
#define quantlib_defines_hpp

#ifdef _MSC_VER
/* Microsoft-specific, but needs to be defined before
   including <boost/config.hpp> which somehow includes
   <math.h> under VC++10
*/
#define _USE_MATH_DEFINES
#endif

#include <boost/config.hpp>
#include <boost/version.hpp>
#if BOOST_VERSION < 103900
    #error using an old version of Boost, please update.
#endif
#if !defined(BOOST_ENABLE_ASSERT_HANDLER)
    #define BOOST_ENABLE_ASSERT_HANDLER
#endif

// PC AD
#define USE_CPPAD 1

#ifdef USE_CPPAD
#include <cppad/cppad.hpp>
#include <complex>
#include <boost/type_traits.hpp>
#include <boost/mpl/apply.hpp>
#endif

/* This allows one to include a given file at this point by
   passing it as a compiler define (e.g., -DQL_INCLUDE_FIRST=foo.hpp).

   The idea is to provide a hook for defining QL_REAL and at the
   same time including any necessary headers for the new type.
*/
#define INCLUDE_FILE(F) INCLUDE_FILE__(F)
#define INCLUDE_FILE_(F) #F
#ifdef QL_INCLUDE_FIRST
#    include INCLUDE_FILE(QL_INCLUDE_FIRST)
#endif
#undef INCLUDE_FILE_
#undef INCLUDE_FILE

/* Eventually these might go into userconfig.hpp.
   For the time being, we hard code them here.
   They can be overridden by passing the #define to the compiler.
*/
#ifndef QL_INTEGER
#    define QL_INTEGER int
#endif

#ifndef QL_BIG_INTEGER
#    define QL_BIG_INTEGER long
#endif

#ifndef QL_REAL
#ifdef USE_CPPAD
#   define QL_REAL CppAD::AD<double>
#   define QL_FLOAT CppAD::AD<float>
#else
#   define QL_REAL double;
#   define QL_FLOAT double;
#endif
#endif

/*! \defgroup macros QuantLib macros

    Global definitions and a few macros which help porting the
    code to different compilers.

    @{
*/

#if (defined(_DEBUG) || defined(DEBUG))
    #define QL_DEBUG
#endif

#if   defined(HAVE_CONFIG_H)    // Dynamically created by configure
   #include <ql/config.hpp>
/* Use BOOST_MSVC instead of _MSC_VER since some other vendors (Metrowerks,
   for example) also #define _MSC_VER
*/
#elif defined(BOOST_MSVC)       // Microsoft Visual C++
   #include <ql/config.msvc.hpp>
#elif defined(__MINGW32__)      // Minimalistic GNU for Windows
   #include <ql/config.mingw.hpp>
#elif defined(__SUNPRO_CC)      // Sun Studio
   #include <ql/config.sun.hpp>
#else                           // We hope that the compiler follows ANSI
   #include <ql/config.ansi.hpp>
#endif


// extra debug checks
#ifdef QL_DEBUG
    #ifndef QL_EXTRA_SAFETY_CHECKS
        #define QL_EXTRA_SAFETY_CHECKS
    #endif
#endif

#ifdef QL_ENABLE_THREAD_SAFE_OBSERVER_PATTERN
    #if BOOST_VERSION < 105800
        #error Boost version 1.58 or higher is required for the thread-safe observer pattern
    #endif
#endif

#ifdef QL_ENABLE_PARALLEL_UNIT_TEST_RUNNER
    #if BOOST_VERSION < 105900
        #error Boost version 1.59 or higher is required for the parallel unit test runner
    #endif
#endif

// ensure that needed math constants are defined
#include <ql/mathconstants.hpp>


// import global functions into std namespace
#if defined(BOOST_NO_STDC_NAMESPACE)
    #include <cmath>
    namespace std {
        using ::sqrt; using ::abs; using ::fabs;
        using ::exp; using ::log; using ::pow;
        using ::sin; using ::cos; using ::asin; using ::acos;
        using ::sinh; using ::cosh;
        using ::floor; using ::fmod; using ::modf;
    }
#endif


/*! \defgroup limitMacros Numeric limits

    Some compilers do not give an implementation of
    <code>\<limits\></code> yet.  For the code to be portable
    these macros should be used instead of the corresponding method of
    <code>std::numeric_limits</code> or the corresponding macro
    defined in <code><limits.h></code>.

    @{
*/
/*! \def QL_MIN_INTEGER
    Defines the value of the largest representable negative integer value
*/
/*! \def QL_MAX_INTEGER
    Defines the value of the largest representable integer value
*/
/*! \def QL_MIN_REAL
    Defines the value of the largest representable negative
    floating-point value
*/
/*! \def QL_MIN_POSITIVE_REAL
    Defines the value of the smallest representable positive double value
*/
/*! \def QL_MAX_REAL
    Defines the value of the largest representable floating-point value
*/
/*! \def QL_EPSILON
    Defines the machine precision for operations over doubles
*/
#include <boost/limits.hpp>
// limits used as such
#define QL_MIN_INTEGER         ((std::numeric_limits<QL_INTEGER>::min)())
#define QL_MAX_INTEGER         ((std::numeric_limits<QL_INTEGER>::max)())

#ifdef USE_CPPAD
namespace std {
    template<class T> struct numeric_limits<CppAD::AD<T>> {
        static const CppAD::AD<T> max() { return std::numeric_limits<T>::max(); }
        static const CppAD::AD<T> min() { return std::numeric_limits<T>::min(); }
        static const CppAD::AD<T> epsilon() { return std::numeric_limits<T>::epsilon(); }
        static const CppAD::AD<T> infinity() { return std::numeric_limits<T>::infinity(); }
    };
}
#endif

#define QL_MIN_REAL           -((std::numeric_limits<QL_REAL>::max)())
#define QL_MAX_REAL            ((std::numeric_limits<QL_REAL>::max)())
#define QL_MIN_POSITIVE_REAL   ((std::numeric_limits<QL_REAL>::min)())
#define QL_EPSILON             ((std::numeric_limits<QL_REAL>::epsilon)())
// specific values---these should fit into any Integer or Real
#define QL_NULL_INTEGER        ((std::numeric_limits<int>::max)())
#define QL_NULL_REAL           ((std::numeric_limits<float>::max)())
/*! @} */

/*! @}  */


// emit warning when using deprecated features
#if defined(BOOST_MSVC)       // Microsoft Visual C++
#define QL_DEPRECATED __declspec(deprecated)
#elif defined(__GNUC__) || defined(__clang__)
#define QL_DEPRECATED __attribute__((deprecated))
#else
// we don't know how to enable it, just define the macro away
#define QL_DEPRECATED
#endif

#ifdef USE_CPPAD

namespace CppAD {

template <class T> inline
const T max(const  T& x, const T& y) {
    return CondExpGt(x, y, x, y);
}

template <class T> inline
const T min(const T& x, const T& y) {
    return CondExpLt(x, y, x, y);
}

template <class T> inline bool isinf(const CppAD::AD<T>& x) {
    return std::isinf(Value(x));
}

template <class T> inline bool isnan(const CppAD::AD<T>& x) {
    return std::isnan(Value(x));
}

template <class T> inline bool isfinite(const CppAD::AD<T>& x) {
    return !std::isinf(Value(x));
}

template <class T> inline bool signbit(const CppAD::AD<T>& x) {
    return x < 0.0;
}

template <class T> inline CppAD::AD<T> copysign(const CppAD::AD<T>& x, const CppAD::AD<T>& y) {
    return abs(x) * sign(y);
}

template <class T> inline CppAD::AD<T> hypot(const CppAD::AD<T>& x, const CppAD::AD<T>& y) {
    return sqrt(x * x + y * y);
}

template <class T> inline CppAD::AD<T> fmax(const CppAD::AD<T>& x, const CppAD::AD<T>& y) {
    return max(x,y);
}

template <class T> inline CppAD::AD<T> logb(const CppAD::AD<T>& x) {
    return log(abs(x)) / log(2.0); // FLT_RADIX == 2 ?
}

template <class T> inline T Value(const T& x) {
    return x;
}

template <class T> inline std::complex<T> Value(const std::complex<CppAD::AD<T>>& x) {
    return std::complex<T>(CppAD::Value(x.real()), CppAD::Value(x.imag()));
}

// breaks AD variables
template <class T> inline std::complex<CppAD::AD<T>> operator/(const std::complex<CppAD::AD<T>>& x, const std::complex<CppAD::AD<T>>& y) {
    std::complex<T> z = CppAD::Value(x) / CppAD::Value(y);
    return std::complex<CppAD::AD<T>>(z.real(), z.imag());
}

// breaks AD variables
template <class T> inline CppAD::AD<T> modf(CppAD::AD<T> x, CppAD::AD<T>* y) {
    T tmp;
    T res = std::modf(CppAD::Value(x), &tmp);
    *y = CppAD::AD<T>(tmp);
    return res;
}

} // namespace CppAD

namespace boost {
template <class T> struct is_float<CppAD::AD<T>> { static const bool value = true; };
template <class T> struct is_arithmetic<CppAD::AD<T>> { static const bool value = true; };

namespace math {
template <class T> inline bool isnan(const CppAD::AD<T>& x) { return std::isnan(Value(x)); }
template <class T> inline bool isinf(const CppAD::AD<T>& x) { return std::isinf(Value(x)); }
} // math

namespace mpl {
    template <typename U, typename Base> struct apply1<U, CppAD::AD<Base>> : apply1<U, Base> {};
} // mpl

} // boost

using CppAD::min; using CppAD::max; using CppAD::isinf; using CppAD::isnan; using CppAD::copysign;
using CppAD::fmax; using CppAD::operator/; using CppAD::modf;

// last resort if function calls are ambiguous
namespace QL_FCT_EXPL {
using CppAD::min; using CppAD::max; using CppAD::isinf; using CppAD::isnan; using CppAD::copysign;
using CppAD::fmax; using CppAD::operator/; using CppAD::modf;
}

#define VALUE(x) (CppAD::Value(x))

#elif // not USE_CPPAD

#define VALUE(x) (x)

// last resort if function calls are ambiguous
namespace QL_FCT_EXPL {
using std::min; using std::max; using std::isinf; using std::isnan; using std::copysign;
using std::fmax; using std::operator/; using std::modf;
}

#endif // USE_CPPAD

#endif
