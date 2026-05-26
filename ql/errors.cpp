/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003, 2004, 2005 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/errors.hpp>
#include <stdexcept>

namespace {

    #if defined(_MSC_VER)
    // allow Visual Studio integration
    std::string format_error(
                       #ifdef QL_ERROR_LINES
                       const std::string& file, long line,
                       #else
                       const std::string&, long,
                       #endif
                       #ifdef QL_ERROR_FUNCTIONS
                       const std::string& function,
                       #else
                       const std::string&,
                       #endif
                       const std::string& message) {
        std::ostringstream msg;
        #ifdef QL_ERROR_FUNCTIONS
        if (function != "(unknown)")
            msg << function << ": ";
        #endif
        #ifdef QL_ERROR_LINES
        msg << "\n  " << file << "(" << line << "): \n";
        #endif
        msg << message;
        return msg.str();
    }
    #else
    // use gcc format (e.g. for integration with Emacs)
    std::string format_error(const std::string& file, long line,
                             const std::string& function,
                             const std::string& message) {
        std::ostringstream msg;
        #ifdef QL_ERROR_LINES
        msg << "\n" << file << ":" << line << ": ";
        #endif
        #ifdef QL_ERROR_FUNCTIONS
        if (function != "(unknown)")
            msg << "In function `" << function << "': \n";
        #endif
        msg << message;
        return msg.str();
    }
    #endif

}

namespace boost {

    // must be defined by the user
    void assertion_failed(char const * expr, char const * function,
                          char const * file, long line) {
        throw std::runtime_error(format_error(file, line, function,
                                              "Boost assertion failed: " +
                                              std::string(expr)));
    }

    void assertion_failed_msg(char const * expr, char const * msg,
                              char const * function, char const * file,
                              long line) {
        throw std::runtime_error(format_error(file, line, function,
                                              "Boost assertion failed: " +
                                              std::string(expr) + ": " +
                                              std::string(msg)));
    }

}

namespace QuantLib {

    Error::Error(const std::string& file, long line,
                 const std::string& function,
                 const std::string& message) {
        message_ = ext::make_shared<std::string>(
            format_error(file, line, function, message));
    }

    const char* Error::what() const noexcept { return message_->c_str(); }
}

#if defined(__GNUC__) or defined(__clang__)
bool qlStoreStacktrace = false;
thread_local boost::stacktrace::stacktrace qlLastStacktrace;
extern "C" {
typedef void (*cxa_throw_type)(void*, std::type_info*, void (*)(void*));
void __cxa_throw(void* thrown_exception, std::type_info* tinfo, void (*dest)(void*)) {
    if (qlStoreStacktrace) {
        qlLastStacktrace = boost::stacktrace::stacktrace();
    }
    static cxa_throw_type real_cxa_throw =
        reinterpret_cast<cxa_throw_type>(dlsym(RTLD_NEXT, "__cxa_throw"));
    real_cxa_throw(thrown_exception, tinfo, dest);
    __builtin_unreachable();
}
}
#endif
