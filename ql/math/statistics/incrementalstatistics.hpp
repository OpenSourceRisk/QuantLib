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

/*! \file incrementalstatistics.hpp
    \brief statistics tool based on incremental accumulation
           in the meantime, this is just a wrapper to the boost
           accumulator library, kept for backward compatibility
*/

#ifndef quantlib_incremental_statistics_hpp
#define quantlib_incremental_statistics_hpp

#include <ql/utilities/null.hpp>
#include <ql/errors.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++11-extensions"
#endif
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/sum.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/weighted_mean.hpp>
#include <boost/accumulators/statistics/weighted_variance.hpp>
#include <boost/accumulators/statistics/weighted_skewness.hpp>
#include <boost/accumulators/statistics/weighted_kurtosis.hpp>
#include <boost/accumulators/statistics/weighted_moment.hpp>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace QuantLib {

    //! Statistics tool based on incremental accumulation
    /*! It can accumulate a set of data and return statistics (e.g: mean,
        variance, skewness, kurtosis, error estimation, etc.).
        This class is a wrapper to the boost accumulator library.
    */

    class IncrementalStatistics {
      public:
        typedef double value_type; // PC AD TODO, allow for Real
        IncrementalStatistics();
        //! \name Inspectors
        //@{
        //! number of samples collected
        Size samples() const;

        //! sum of data weights
        value_type weightSum() const;

        /*! returns the mean, defined as
            \f[ \langle x \rangle = \frac{\sum w_i x_i}{\sum w_i}. \f]
        */
        value_type mean() const;

        /*! returns the variance, defined as
            \f[ \frac{N}{N-1} \left\langle \left(
                x-\langle x \rangle \right)^2 \right\rangle. \f]
        */
        value_type variance() const;

        /*! returns the standard deviation \f$ \sigma \f$, defined as the
            square root of the variance.
        */
        value_type standardDeviation() const;

        /*! returns the error estimate \f$ \epsilon \f$, defined as the
            square root of the ratio of the variance to the number of
            samples.
        */
        value_type errorEstimate() const;

        /*! returns the skewness, defined as
            \f[ \frac{N^2}{(N-1)(N-2)} \frac{\left\langle \left(
                x-\langle x \rangle \right)^3 \right\rangle}{\sigma^3}. \f]
            The above evaluates to 0 for a Gaussian distribution.
        */
        value_type skewness() const;

        /*! returns the excess kurtosis, defined as
            \f[ \frac{N^2(N+1)}{(N-1)(N-2)(N-3)}
                \frac{\left\langle \left(x-\langle x \rangle \right)^4
                \right\rangle}{\sigma^4} - \frac{3(N-1)^2}{(N-2)(N-3)}. \f]
            The above evaluates to 0 for a Gaussian distribution.
        */
        value_type kurtosis() const;

        /*! returns the minimum sample value */
        value_type min() const;

        /*! returns the maximum sample value */
        value_type max() const;

        //! number of negative samples collected
        Size downsideSamples() const;

        //! sum of data weights for negative samples
        value_type downsideWeightSum() const;

        /*! returns the downside variance, defined as
            \f[ \frac{N}{N-1} \times \frac{ \sum_{i=1}^{N}
                \theta \times x_i^{2}}{ \sum_{i=1}^{N} w_i} \f],
            where \f$ \theta \f$ = 0 if x > 0 and
            \f$ \theta \f$ =1 if x <0
        */
        value_type downsideVariance() const;

        /*! returns the downside deviation, defined as the
            square root of the downside variance.
        */
        value_type downsideDeviation() const;

        //@}

        //! \name Modifiers
        //@{
        //! adds a datum to the set, possibly with a weight
        /*! \pre weight must be positive or null */
        void add(value_type value, value_type weight = 1.0);
        //! adds a sequence of data to the set, with default weight
        template <class DataIterator>
        void addSequence(DataIterator begin, DataIterator end) {
            for (;begin!=end;++begin)
                add(VALUE(*begin)); // TODO PC AD
        }
        //! adds a sequence of data to the set, each with its weight
        /*! \pre weights must be positive or null */
        template <class DataIterator, class WeightIterator>
        void addSequence(DataIterator begin, DataIterator end,
                         WeightIterator wbegin) {
            for (;begin!=end;++begin,++wbegin)
                add(VALUE(*begin), VALUE(*wbegin)); // TODO PC AD
        }
        //! resets the data to a null set
        void reset();
        //@}
     private:
       typedef boost::accumulators::accumulator_set<
           value_type,
           boost::accumulators::stats<
               boost::accumulators::tag::count, boost::accumulators::tag::min,
               boost::accumulators::tag::max,
               boost::accumulators::tag::weighted_mean,
               boost::accumulators::tag::weighted_variance,
               boost::accumulators::tag::weighted_skewness,
               boost::accumulators::tag::weighted_kurtosis,
               boost::accumulators::tag::sum_of_weights>,
           value_type> accumulator_set;
        accumulator_set acc_;
        typedef boost::accumulators::accumulator_set<
            value_type, boost::accumulators::stats<
                      boost::accumulators::tag::count,
                      boost::accumulators::tag::weighted_moment<2>,
                      boost::accumulators::tag::sum_of_weights>,
            value_type> downside_accumulator_set;
        downside_accumulator_set downsideAcc_;
    };

}


#endif
