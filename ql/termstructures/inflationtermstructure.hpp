/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007, 2009 Chris Kenyon

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

/*! \file inflationtermstructure.hpp
    \brief Base classes for inflation term structures.
*/

#ifndef quantlib_inflation_termstructure_hpp
#define quantlib_inflation_termstructure_hpp

#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/inflation/seasonality.hpp>

namespace QuantLib {

    class InflationIndex;

    //! Interface for inflation term structures.
    /*! \ingroup inflationtermstructures */
    class InflationTermStructure : public TermStructure {
      public:
        //! \name Constructors
        //@{
        InflationTermStructure(Rate baseRate,
                               const Period& observationLag,
                               Frequency frequency,
                               const DayCounter& dayCounter = DayCounter(),
                               ext::shared_ptr<Seasonality> seasonality = {});
        InflationTermStructure(const Date& referenceDate,
                               Rate baseRate,
                               const Period& observationLag,
                               Frequency frequency,
                               const Calendar& calendar = Calendar(),
                               const DayCounter& dayCounter = DayCounter(),
                               ext::shared_ptr<Seasonality> seasonality = {});
        InflationTermStructure(Natural settlementDays,
                               const Calendar& calendar,
                               Rate baseRate,
                               const Period& observationLag,
                               Frequency frequency,
                               const DayCounter& dayCounter = DayCounter(),
                               ext::shared_ptr<Seasonality> seasonality = {});
        //@}

        //! \name Inflation interface
        //@{
        //! The TS observes with a lag that is usually different from the
        //! availability lag of the index.  An inflation rate is given,
        //! by default, for the maturity requested assuming this lag.
        virtual Period observationLag() const;
        virtual Frequency frequency() const;
        virtual Rate baseRate() const;

        //! minimum (base) date
        /*! Important in inflation since it starts before nominal
            reference date.  Changes depending whether index is
            interpolated or not.  When interpolated the base date
            is just observation lag before nominal.  When not
            interpolated it is the beginning of the relevant period
            (hence it is easy to create interpolated fixings from
             a not-interpolated curve because interpolation, usually,
             of fixings is forward looking).
        */
        virtual Date baseDate() const = 0;
        //@}

        //! Functions to set and get seasonality.
        /*! Calling setSeasonality with no arguments means unsetting
            as the default is used to choose unsetting.
        */
        virtual void setSeasonality(const ext::shared_ptr<Seasonality>& seasonality = {});
        ext::shared_ptr<Seasonality> seasonality() const;
        bool hasSeasonality() const;

      protected:

        // This next part is required for piecewise- constructors
        // because, for inflation, they need more than just the
        // instruments to build the term structure, since the rate at
        // time 0-lag is non-zero, since we deal (effectively) with
        // "forwards".
        virtual void setBaseRate(const Rate &r) { baseRate_ = r; }


        // range-checking
        void checkRange(const Date&,
                        bool extrapolate) const;
        void checkRange(Time t,
                        bool extrapolate) const;

        ext::shared_ptr<Seasonality> seasonality_;
        Period observationLag_;
        Frequency frequency_;
        mutable Rate baseRate_;
    };

    //! Interface for zero inflation term structures.
    // Child classes use templates but do not want that exposed to
    // general users.
    class ZeroInflationTermStructure : public InflationTermStructure {
      public:
        //! \name Constructors
        //@{
        ZeroInflationTermStructure(const DayCounter& dayCounter,
                                   Rate baseZeroRate,
                                   const Period& lag,
                                   Frequency frequency,
                                   const ext::shared_ptr<Seasonality> &seasonality = {});

        ZeroInflationTermStructure(const Date& referenceDate,
                                   const Calendar& calendar,
                                   const DayCounter& dayCounter,
                                   Rate baseZeroRate,
                                   const Period& lag,
                                   Frequency frequency,
                                   const ext::shared_ptr<Seasonality>& seasonality = {});

        ZeroInflationTermStructure(Natural settlementDays,
                                   const Calendar& calendar,
                                   const DayCounter& dayCounter,
                                   Rate baseZeroRate,
                                   const Period& lag,
                                   Frequency frequency,
                                   const ext::shared_ptr<Seasonality> &seasonality = {});
        //@}

        //! \name Inspectors
        //@{
        //! zero-coupon inflation rate.
        /*! Essentially the fair rate for a zero-coupon inflation swap
            (by definition), i.e. the zero term structure uses yearly
            compounding, which is assumed for ZCIIS instrument quotes.

            \note by default you get the same as lag and interpolation
            as the term structure.
            If you want to get predictions of RPI/CPI/etc then use an
            index.
        */
        Rate zeroRate(const Date &d, const Period& instObsLag = Period(-1,Days),
                      bool forceLinearInterpolation = false,
                      bool extrapolate = false) const;
        //! zero-coupon inflation rate.
        /*! \warning Since inflation is highly linked to dates (lags,
                     interpolation, months for seasonality, etc) this
                     method cannot account for all effects.  If you
                     call it, You'll have to manage lag, seasonality
                     etc. yourself.
        */
        Rate zeroRate(Time t,
                      bool extrapolate = false) const;
        //@}
      protected:
        //! to be defined in derived classes
        virtual Rate zeroRateImpl(Time t) const = 0;
    };


    //! Base class for year-on-year inflation term structures.
    class YoYInflationTermStructure : public InflationTermStructure {
      public:
        //! \name Constructors
        //@{
        YoYInflationTermStructure(const DayCounter& dayCounter,
                                  Rate baseYoYRate,
                                  const Period& lag,
                                  Frequency frequency,
                                  bool indexIsInterpolated,
                                  const ext::shared_ptr<Seasonality> &seasonality = {});

        YoYInflationTermStructure(const Date& referenceDate,
                                  const Calendar& calendar,
                                  const DayCounter& dayCounter,
                                  Rate baseYoYRate,
                                  const Period& lag,
                                  Frequency frequency,
                                  bool indexIsInterpolated,
                                  const ext::shared_ptr<Seasonality> &seasonality = {});

        YoYInflationTermStructure(Natural settlementDays,
                                  const Calendar& calendar,
                                  const DayCounter& dayCounter,
                                  Rate baseYoYRate,
                                  const Period& lag,
                                  Frequency frequency,
                                  bool indexIsInterpolated,
                                  const ext::shared_ptr<Seasonality> &seasonality = {});
        //@}

        //! \name Inspectors
        //@{
        //! year-on-year inflation rate.
        /*! The forceLinearInterpolation parameter is relative to the
            frequency of the TS.

            \note this is not the year-on-year swap (YYIIS) rate.
        */
        Rate yoyRate(const Date &d, const Period& instObsLag = Period(-1,Days),
                     bool forceLinearInterpolation = false,
                     bool extrapolate = false) const;
        //! year-on-year inflation rate.
        /*! \warning Since inflation is highly linked to dates (lags,
                     interpolation, months for seasonality, etc) this
                     method cannot account for all effects.  If you
                     call it, You'll have to manage lag, seasonality
                     etc. yourself.
        */
        Rate yoyRate(Time t,
                     bool extrapolate = false) const;
        //@}

        virtual bool indexIsInterpolated() const;
      protected:
        //! to be defined in derived classes
        virtual Rate yoyRateImpl(Time time) const = 0;
      private:
        bool indexIsInterpolated_;
    };


    //! utility function giving the inflation period for a given date
    std::pair<Date,Date> inflationPeriod(const Date &,
                                         Frequency);

    //! utility function giving the time between two dates depending on
    //! index frequency and interpolation, and a day counter
    Time inflationYearFraction(Frequency ,
                               bool indexIsInterpolated,
                               const DayCounter &,
                               const Date &, const Date &);


    // inline

    inline Period InflationTermStructure::observationLag() const {
        return observationLag_;
    }

    inline Frequency InflationTermStructure::frequency() const {
        return frequency_;
    }

    inline Rate InflationTermStructure::baseRate() const {
        return baseRate_;
    }

    inline ext::shared_ptr<Seasonality> InflationTermStructure::seasonality() const {
        return seasonality_;
    }

    inline bool InflationTermStructure::hasSeasonality() const {
        return static_cast<bool>(seasonality_);
    }

    inline bool YoYInflationTermStructure::indexIsInterpolated() const {
        return indexIsInterpolated_;
    }

}

#endif
