/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2002, 2003 Decillion Pty(Ltd)
 Copyright (C) 2005, 2006, 2008, 2009 StatPro Italia srl
 Copyright (C) 2009, 2015 Ferdinando Ametrano
 Copyright (C) 2015 Paolo Mazzocchi

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

/*! \file discountcurve.hpp
    \brief interpolated discount factor structure
*/

#ifndef quantlib_discount_curve_hpp
#define quantlib_discount_curve_hpp

#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/interpolatedcurve.hpp>
#include <ql/math/interpolations/loginterpolation.hpp>
#include <utility>

namespace QuantLib {

    //! YieldTermStructure based on interpolation of discount factors
    /*! \ingroup yieldtermstructures */
    template <class Interpolator>
    class InterpolatedDiscountCurve
        : public YieldTermStructure,
          protected InterpolatedCurve<Interpolator> {
      public:
        InterpolatedDiscountCurve(
            const std::vector<Date>& dates,
            const std::vector<DiscountFactor>& dfs,
            const DayCounter& dayCounter,
            const Calendar& cal = Calendar(),
            const std::vector<Handle<Quote>>& jumps = {},
            const std::vector<Date>& jumpDates = {},
            const Interpolator& interpolator = {},
            const Extrapolation extrapolation = Extrapolation::ContinuousForward,
            const bool excludeTimeZeroFromInterpolation = false);
        InterpolatedDiscountCurve(
            const std::vector<Date>& dates,
            const std::vector<DiscountFactor>& dfs,
            const DayCounter& dayCounter,
            const Calendar& calendar,
            const Interpolator& interpolator,
            const Extrapolation extrapolation = Extrapolation::ContinuousForward,
            const bool excludeTimeZeroFromInterpolation = false);
        InterpolatedDiscountCurve(
            const std::vector<Date>& dates,
            const std::vector<DiscountFactor>& dfs,
            const DayCounter& dayCounter,
            const Interpolator& interpolator,
            const Extrapolation extrapolation = Extrapolation::ContinuousForward,
            const bool excludeTimeZeroFromInterpolation = false);
        //! \name TermStructure interface
        //@{
        Date maxDate() const override;
        //@}
        //! \name other inspectors
        //@{
        const std::vector<Time>& times() const;
        const std::vector<Date>& dates() const;
        const std::vector<Real>& data() const;
        const std::vector<DiscountFactor>& discounts() const;
        std::vector<std::pair<Date, Real> > nodes() const;
        //@}

      protected:
        explicit InterpolatedDiscountCurve(
            const DayCounter&,
            const Interpolator& interpolator = {},
            const Extrapolation extrapolation = Extrapolation::ContinuousForward,
            const bool excludeTimeZeroFromInterpolation = false);
        InterpolatedDiscountCurve(
            const Date& referenceDate,
            const DayCounter&,
            const std::vector<Handle<Quote>>& jumps = {},
            const std::vector<Date>& jumpDates = {},
            const Interpolator& interpolator = {},
            const Extrapolation extrapolation = Extrapolation::ContinuousForward,
            const bool excludeTimeZeroFromInterpolation = false);
        InterpolatedDiscountCurve(
            Natural settlementDays,
            const Calendar&,
            const DayCounter&,
            const std::vector<Handle<Quote>>& jumps = {},
            const std::vector<Date>& jumpDates = {},
            const Interpolator& interpolator = {},
            const Extrapolation extrapolation = Extrapolation::ContinuousForward,
            const bool excludeTimeZeroFromInterpolation = false);

        //! \name YieldTermStructure implementation
        //@{
        DiscountFactor discountImpl(Time) const override;
        //@}
        Extrapolation extrapolation_;
        bool excludeTimeZeroFromInterpolation_;
        mutable std::vector<Date> dates_;
      private:
        void initialize();
    };

    //! Term structure based on log-linear interpolation of discount factors
    /*! Log-linear interpolation guarantees piecewise-constant forward
        rates.

        \ingroup yieldtermstructures
    */
    typedef InterpolatedDiscountCurve<LogLinear> DiscountCurve;


    // inline definitions

    template <class T>
    inline Date InterpolatedDiscountCurve<T>::maxDate() const {
        if (this->maxDate_ != Date())
            return this->maxDate_;
        return dates_.back();
    }

    template <class T>
    inline const std::vector<Time>&
    InterpolatedDiscountCurve<T>::times() const {
        return this->times_;
    }

    template <class T>
    inline const std::vector<Date>&
    InterpolatedDiscountCurve<T>::dates() const {
        return dates_;
    }

    template <class T>
    inline const std::vector<Real>&
    InterpolatedDiscountCurve<T>::data() const {
        return this->data_;
    }

    template <class T>
    inline const std::vector<DiscountFactor>&
    InterpolatedDiscountCurve<T>::discounts() const {
        return this->data_;
    }

    template <class T>
    inline std::vector<std::pair<Date, Real> >
    InterpolatedDiscountCurve<T>::nodes() const {
        std::vector<std::pair<Date, Real> > results(dates_.size());
        for (Size i=0; i<dates_.size(); ++i)
            results[i] = std::make_pair(dates_[i], this->data_[i]);
        return results;
    }

    #ifndef __DOXYGEN__

    // template definitions

    template <class T>
    DiscountFactor InterpolatedDiscountCurve<T>::discountImpl(Time t) const {
        if (t <= this->times_.back()) {
            if (excludeTimeZeroFromInterpolation_) {
                if (t <= 0.0)
                    return 1.0;
                if (t < this->times_[1]) {
                    // flat zero rate between 0 and first positive time
                    return std::pow(this->data_[1], t / this->times_[1]);
                }
            }
            return this->interpolation_(t, true);
        }

        Time tMax = this->times_.back();
        DiscountFactor dMax = this->data_.back();

        // flat fwd extrapolation
        if (extrapolation_ == YieldTermStructure::Extrapolation::ContinuousForward) {
            Rate instFwdMax = -this->interpolation_.derivative(tMax) / dMax;
            return dMax * std::exp(-instFwdMax * (t - tMax));
        } else if (extrapolation_ == YieldTermStructure::Extrapolation::DiscreteForward) {
            Time tMax_m = this->timeFromReference(dates_.back() - 1);
            DiscountFactor dMax_m = this->interpolation_(tMax_m);
            return dMax * std::pow(dMax / dMax_m, (t - tMax) / (tMax - tMax_m));
        } else {
            QL_FAIL("extrapolation method not handled.");
        }
    }

    template <class T>
    InterpolatedDiscountCurve<T>::InterpolatedDiscountCurve(
        const DayCounter& dayCounter,
        const T& interpolator,
        const Extrapolation extrapolation,
        const bool excludeTimeZeroFromInterpolation)
    : YieldTermStructure(dayCounter),
      InterpolatedCurve<T>(interpolator, excludeTimeZeroFromInterpolation ? 1 : 0),
      extrapolation_(extrapolation),
      excludeTimeZeroFromInterpolation_(excludeTimeZeroFromInterpolation) {}

    template <class T>
    InterpolatedDiscountCurve<T>::InterpolatedDiscountCurve(
        const Date& referenceDate,
        const DayCounter& dayCounter,
        const std::vector<Handle<Quote>>& jumps,
        const std::vector<Date>& jumpDates,
        const T& interpolator,
        const Extrapolation extrapolation,
        const bool excludeTimeZeroFromInterpolation)
    : YieldTermStructure(referenceDate, Calendar(), dayCounter, jumps, jumpDates),
      InterpolatedCurve<T>(interpolator, excludeTimeZeroFromInterpolation ? 1 : 0),
      extrapolation_(extrapolation),
      excludeTimeZeroFromInterpolation_(excludeTimeZeroFromInterpolation) {}

    template <class T>
    InterpolatedDiscountCurve<T>::InterpolatedDiscountCurve(
        Natural settlementDays,
        const Calendar& calendar,
        const DayCounter& dayCounter,
        const std::vector<Handle<Quote>>& jumps,
        const std::vector<Date>& jumpDates,
        const T& interpolator,
        const Extrapolation extrapolation,
        const bool excludeTimeZeroFromInterpolation)
    : YieldTermStructure(settlementDays, calendar, dayCounter, jumps, jumpDates),
      InterpolatedCurve<T>(interpolator, excludeTimeZeroFromInterpolation ? 1 : 0),
      extrapolation_(extrapolation),
      excludeTimeZeroFromInterpolation_(excludeTimeZeroFromInterpolation) {}

    template <class T>
    InterpolatedDiscountCurve<T>::InterpolatedDiscountCurve(
        const std::vector<Date>& dates,
        const std::vector<DiscountFactor>& discounts,
        const DayCounter& dayCounter,
        const Calendar& calendar,
        const std::vector<Handle<Quote>>& jumps,
        const std::vector<Date>& jumpDates,
        const T& interpolator,
        const Extrapolation extrapolation,
        const bool excludeTimeZeroFromInterpolation)
    : YieldTermStructure(dates.at(0), calendar, dayCounter, jumps, jumpDates),
      InterpolatedCurve<T>(
          std::vector<Time>(), discounts, interpolator, excludeTimeZeroFromInterpolation ? 1 : 0),
      extrapolation_(extrapolation),
      excludeTimeZeroFromInterpolation_(excludeTimeZeroFromInterpolation), dates_(dates) {
        initialize();
    }

    template <class T>
    InterpolatedDiscountCurve<T>::InterpolatedDiscountCurve(
        const std::vector<Date>& dates,
        const std::vector<DiscountFactor>& discounts,
        const DayCounter& dayCounter,
        const Calendar& calendar,
        const T& interpolator,
        const Extrapolation extrapolation,
        const bool excludeTimeZeroFromInterpolation)
    : YieldTermStructure(dates.at(0), calendar, dayCounter),
      InterpolatedCurve<T>(
          std::vector<Time>(), discounts, interpolator, excludeTimeZeroFromInterpolation ? 1 : 0),
      extrapolation_(extrapolation),
      excludeTimeZeroFromInterpolation_(excludeTimeZeroFromInterpolation), dates_(dates) {
        initialize();
    }

    template <class T>
    InterpolatedDiscountCurve<T>::InterpolatedDiscountCurve(
        const std::vector<Date>& dates,
        const std::vector<DiscountFactor>& discounts,
        const DayCounter& dayCounter,
        const T& interpolator,
        const Extrapolation extrapolation,
        const bool excludeTimeZeroFromInterpolation)
    : YieldTermStructure(dates.at(0), Calendar(), dayCounter),
      InterpolatedCurve<T>(
          std::vector<Time>(), discounts, interpolator, excludeTimeZeroFromInterpolation ? 1 : 0),
      extrapolation_(extrapolation),
      excludeTimeZeroFromInterpolation_(excludeTimeZeroFromInterpolation), dates_(dates) {
        initialize();
    }

#endif

    template <class T>
    void InterpolatedDiscountCurve<T>::initialize()
    {
        QL_REQUIRE(dates_.size() >= T::requiredPoints,
                   "not enough input dates given");
        QL_REQUIRE(this->data_.size() == dates_.size(),
                   "dates/data count mismatch");
        QL_REQUIRE(this->data_[0] == 1.0,
                   "the first discount must be == 1.0 "
                   "to flag the corresponding date as reference date");
        for (Size i=1; i<dates_.size(); ++i) {
            QL_REQUIRE(this->data_[i] > 0.0, "negative discount");
        }

        this->setupTimes(dates_, dates_[0], dayCounter());
        this->setupInterpolation();
        this->interpolation_.update();
    }

}

#endif
