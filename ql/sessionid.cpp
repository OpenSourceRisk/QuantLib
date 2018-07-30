/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
*/

// provide a session id if QL_ENABLE_SESSIONS is defined

#include <ql/qldefines.hpp>
#include <ql/types.hpp>
#include <thread>

#include <iostream>

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {
    Integer sessionId() { return std::hash<std::thread::id>{}(std::this_thread::get_id()); };
} // namespace QuantLib
#endif
