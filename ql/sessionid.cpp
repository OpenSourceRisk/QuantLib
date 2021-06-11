/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

// provide a session id if QL_ENABLE_SESSIONS is defined

#include <ql/qldefines.hpp>
#include <ql/types.hpp>
#include <thread>

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {
Integer sessionId() { return std::hash<std::thread::id>{}(std::this_thread::get_id()); };
} // namespace QuantLib
#endif
