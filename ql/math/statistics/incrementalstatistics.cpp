/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2015 Peter Caspers

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

#include <ql/math/statistics/incrementalstatistics.hpp>
#include <iomanip>

namespace QuantLib {

    IncrementalStatistics::IncrementalStatistics() {
        reset();
    }

    Size IncrementalStatistics::samples() const {
        return boost::accumulators::extract_result<
            boost::accumulators::tag::count>(acc_);
    }

    IncrementalStatistics::value_type IncrementalStatistics::weightSum() const {
        return boost::accumulators::extract_result<
            boost::accumulators::tag::sum_of_weights>(acc_);
    }

    IncrementalStatistics::value_type IncrementalStatistics::mean() const {
        QL_REQUIRE(weightSum() > 0.0, "sampleWeight_= 0, unsufficient");
        return boost::accumulators::extract_result<
            boost::accumulators::tag::weighted_mean>(acc_);
    }

    IncrementalStatistics::value_type IncrementalStatistics::variance() const {
        QL_REQUIRE(weightSum() > 0.0, "sampleWeight_= 0, unsufficient");
        QL_REQUIRE(samples() > 1, "sample number <= 1, unsufficient");
        IncrementalStatistics::value_type n = static_cast<IncrementalStatistics::value_type>(samples());
        return n / (n - 1.0) *
               boost::accumulators::extract_result<
                   boost::accumulators::tag::weighted_variance>(acc_);
    }

    IncrementalStatistics::value_type IncrementalStatistics::standardDeviation() const {
        return sqrt(variance());
    }

    IncrementalStatistics::value_type IncrementalStatistics::errorEstimate() const {
        return sqrt(variance() / (samples()));
    }

    IncrementalStatistics::value_type IncrementalStatistics::skewness() const {
        QL_REQUIRE(samples() > 2, "sample number <= 2, unsufficient");
        IncrementalStatistics::value_type n = static_cast<IncrementalStatistics::value_type>(samples());
        IncrementalStatistics::value_type r1 = n / (n - 2.0);
        IncrementalStatistics::value_type r2 = (n - 1.0) / (n - 2.0);
        return sqrt(r1 * r2) * 
               boost::accumulators::extract_result<
                   boost::accumulators::tag::weighted_skewness>(acc_);
    }

    IncrementalStatistics::value_type IncrementalStatistics::kurtosis() const {
        QL_REQUIRE(samples() > 3,
                   "sample number <= 3, unsufficient");
        boost::accumulators::extract_result<
            boost::accumulators::tag::weighted_kurtosis>(acc_);
        IncrementalStatistics::value_type n = static_cast<IncrementalStatistics::value_type>(samples());
        IncrementalStatistics::value_type r1 = (n - 1.0) / (n - 2.0);
        IncrementalStatistics::value_type r2 = (n + 1.0) / (n - 3.0);
        IncrementalStatistics::value_type r3 = (n - 1.0) / (n - 3.0);
        return ((3.0 + boost::accumulators::extract_result<
                           boost::accumulators::tag::weighted_kurtosis>(acc_)) *
                    r2 -
                3.0 * r3) *
               r1;
    }

    IncrementalStatistics::value_type IncrementalStatistics::min() const {
        QL_REQUIRE(samples() > 0, "empty sample set");
        return boost::accumulators::extract_result<
            boost::accumulators::tag::min>(acc_);
    }

    IncrementalStatistics::value_type IncrementalStatistics::max() const {
        QL_REQUIRE(samples() > 0, "empty sample set");
        return boost::accumulators::extract_result<
            boost::accumulators::tag::max>(acc_);
    }

    Size IncrementalStatistics::downsideSamples() const {
        return boost::accumulators::extract_result<
            boost::accumulators::tag::count>(downsideAcc_);
    }

    IncrementalStatistics::value_type IncrementalStatistics::downsideWeightSum() const {
        return boost::accumulators::extract_result<
            boost::accumulators::tag::sum_of_weights>(downsideAcc_);
    }

    IncrementalStatistics::value_type IncrementalStatistics::downsideVariance() const {
        QL_REQUIRE(downsideWeightSum() > 0.0, "sampleWeight_= 0, unsufficient");
        QL_REQUIRE(downsideSamples() > 1, "sample number <= 1, unsufficient");
        IncrementalStatistics::value_type n = static_cast<IncrementalStatistics::value_type>(downsideSamples());
        IncrementalStatistics::value_type r1 = n / (n - 1.0);
        return r1 *
               boost::accumulators::extract_result<
                   boost::accumulators::tag::moment<2> >(downsideAcc_);
    }

    IncrementalStatistics::value_type IncrementalStatistics::downsideDeviation() const {
        return sqrt(downsideVariance());
    }

    void IncrementalStatistics::add(IncrementalStatistics::value_type value, IncrementalStatistics::value_type valueWeight) {
        QL_REQUIRE(valueWeight >= 0.0, "negative weight (" << valueWeight
                                                           << ") not allowed");
        acc_(value, boost::accumulators::weight = valueWeight);
        if(value < 0.0)
            downsideAcc_(value, boost::accumulators::weight = valueWeight);
    }

    void IncrementalStatistics::reset() {
        acc_ = accumulator_set();
        downsideAcc_ = downside_accumulator_set();
    }

}
