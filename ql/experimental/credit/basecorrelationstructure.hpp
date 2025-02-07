/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2014 Jose Aparicio

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

#ifndef quantlib_base_correl_structure_hpp
#define quantlib_base_correl_structure_hpp

#include <ql/quote.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/math/interpolations/bicubicsplineinterpolation.hpp>
#include <ql/experimental/credit/correlationstructure.hpp>
#include <ql/time/dategenerationrule.hpp>
#include <ql/time/schedule.hpp>
#include <boost/optional/optional.hpp>

namespace QuantLib {


    /*! Matrix based Base Correlation Term Structure\par
    Loss level versus time interpolated scalar copula type parametric 
    correlation term structure. Represents the correlation for the credit loss 
    level of a given portfolio at a given loss level and time.

    \todo The relation to a given basket is to be made explicit for bespoke 
    models to be implemented.
    \todo Consider moving to a matrix data structure. A matrix might make some
    computations heavy, template specialization on the dimension might be an
    alternative to having two classes, one for scalars and another for matrices.
    \todo Rethink all the data structure with a basket where current losses are 
    not zero.
    \todo In principle the 2D interpolator is left optional since there are 
    arbitrage issues on the interpolator type to be used. However one has to be
    careful when using non local interpolators like CubicSplines which have an
    effect on the past (calibrated) coupons of previous tenors.
    */
    template<class Interpolator2D_T>
    class BaseCorrelationTermStructure : public CorrelationTermStructure {
    public:
        /*
        @param correls Corresponds to: correls[iYear][iLoss]

        The Settlement date should in an ideal world coincide with the 
        (implicit) basket inception date and its default term structures 
        settlement dates.
        */
        BaseCorrelationTermStructure(
            Natural settlementDays,
            const Calendar& cal,
            BusinessDayConvention bdc,
            const std::vector<Period>& tenors,// sorted
            const std::vector<Real>& lossLevel,//sorted
            const std::vector<std::vector<Handle<Quote> > >& correls,
            const DayCounter& dc = DayCounter(),
            const Date& startDate = Date(),
            boost::optional<DateGeneration::Rule> rule = boost::none
            )
        : CorrelationTermStructure(settlementDays, cal, bdc, dc),
          correlHandles_(correls),
          nLosses_(lossLevel.size()),
          tenors_(tenors),
          lossLevel_(lossLevel) {
              checkTrancheTenors();

              // Reference date to which the tenors are applied to get the base correlation maturities.
              // Generally, for index tranches, the index start date would be supplied as the startDate ctor argument 
              // and a CDS date generation rule provided to arrive at the appropriate maturity.
              const Date& refDate = referenceDate();
              Date start = startDate == Date() ? refDate : startDate;
              Calendar cldr = calendar();
              BusinessDayConvention bdconv = businessDayConvention();

              for (Size i = 0; i < tenors_.size(); i++) {

                  Date d;
                  if (rule) {
                      d = start + tenors_[i];
                      if (*rule == DateGeneration::CDS2015 || *rule == DateGeneration::CDS ||
                          *rule == DateGeneration::OldCDS) {
                          d = cdsMaturity(start, tenors_[i], *rule);
                      }
                      Schedule schedule = MakeSchedule()
                          .from(start)
                          .to(d)
                          .withFrequency(Quarterly)
                          .withCalendar(cldr)
                          .withConvention(bdconv)
                          .withTerminationDateConvention(Unadjusted)
                          .withRule(*rule);
                      d = cldr.adjust(schedule.dates().back(), bdconv);
                  } else {
                      d = cldr.advance(start, tenors_[i], bdconv);
                  }

                  // only keep future dates
                  if(d <= refDate)
                      continue;

                  trancheDates_.push_back(d);
              }

              QL_REQUIRE(!trancheDates_.empty(),
                         "no tranche dates left after removing expired tenors");

              correlations_ = Matrix(correls.size(), trancheDates_.size(), 0.0);
              initializeTrancheTimes();

              checkInputs(correlations_.rows(), correlations_.columns());
              updateMatrix();
              registerWithMarketData();
              // call factory
              setupInterpolation();
        }
    private:
        virtual void setupInterpolation() ;
    public:
        Size correlationSize() const override {return 1;}
        //! Implicit correlation for the given loss interval.
        Real ImplicitCorrelation(Real, Real);

        void checkTrancheTenors() const ;
        void checkLosses() const;
        void initializeTrancheTimes() const;
        void checkInputs(Size volRows, Size volsColumns) const;
        void registerWithMarketData();

        void update() override;
        void updateMatrix() const;

        // TermStructure interface
        Date maxDate() const override {
            return trancheDates_.back();
        }
        Real correlation(const Date& d, Real lossLevel, 
            bool extrapolate = false) const 
        {
            return correlation(timeFromReference(d), lossLevel, extrapolate);
        }
        Real correlation(Time t, Real lossLevel, 
            bool extrapolate = false) const 
        {
            return interpolation_(t, lossLevel, true);
        }
    private:
        std::vector<std::vector<Handle<Quote> > > correlHandles_;
        mutable Matrix correlations_;
        Interpolation2D interpolation_;
        Size nLosses_;
        std::vector<Period> tenors_;
        mutable std::vector<Real> lossLevel_;
        mutable std::vector<Date> trancheDates_;
        mutable std::vector<Time> trancheTimes_;
    };

    // ----------------------------------------------------------------------

    template <class I2D_T>
    void BaseCorrelationTermStructure<I2D_T>::checkTrancheTenors() const {
        QL_REQUIRE(tenors_[0]>0*Days,
                   "first tranche tenor is negative (" <<
                   tenors_[0] << ")");
        for (Size i=1; i<tenors_.size(); ++i)
            QL_REQUIRE(tenors_[i]>tenors_[i-1],
                       "non increasing tranche tenor: " << io::ordinal(i) <<
                       " is " << tenors_[i-1] << ", " << io::ordinal(i+1) <<
                       " is " << tenors_[i]);
    }

    template <class I2D_T>
    void BaseCorrelationTermStructure<I2D_T>::checkLosses() const {
        QL_REQUIRE(lossLevel_[0]>0.,
                   "first loss level is negative (" <<
                   lossLevel_[0] << ")");
        QL_REQUIRE(lossLevel_[0] <= 1.,
            "First loss level larger than 100% (" << lossLevel_[0] <<")");
        for (Size i=1; i<nLosses_; ++i) {
            QL_REQUIRE(lossLevel_[i]>lossLevel_[i-1],
                       "non increasing losses: " << io::ordinal(i) <<
                       " is " << lossLevel_[i-1] << ", " << io::ordinal(i+1) <<
                       " is " << lossLevel_[i]);
        QL_REQUIRE(lossLevel_[i] <= 1.,
            "Loss level " << i << " larger than 100% (" << lossLevel_[i] <<")");
        }
    }

    template <class I2D_T>
    void BaseCorrelationTermStructure<I2D_T>::initializeTrancheTimes() const {
        trancheTimes_.resize(trancheDates_.size());
        for (Size i=0; i<trancheDates_.size(); ++i)
            trancheTimes_[i] = timeFromReference(trancheDates_[i]);
    }

    template <class I2D_T>
    void BaseCorrelationTermStructure<I2D_T>::checkInputs(Size volRows,
                                               Size volsColumns) const {
        QL_REQUIRE(nLosses_==volRows,
                   "mismatch between number of loss levels (" <<
                   nLosses_ << ") and number of rows (" << volRows <<
                   ") in the correl matrix");
        QL_REQUIRE(tenors_.size()==volsColumns,
                   "mismatch between number of tranche tenors (" <<
                   tenors_.size() << ") and number of columns (" <<
                   volsColumns << ") in the correl matrix");
    }

    template <class I2D_T>
    void BaseCorrelationTermStructure<I2D_T>::registerWithMarketData()
    {
        for (Size i=0; i<correlHandles_.size(); ++i)
            for (Size j=0; j<correlHandles_.front().size(); ++j)
                registerWith(correlHandles_[i][j]);
    }

    template <class I2D_T>
    void BaseCorrelationTermStructure<I2D_T>::update() {
        updateMatrix();
        TermStructure::update();
    }

    template <class I2D_T>
    void BaseCorrelationTermStructure<I2D_T>::updateMatrix() const {
        // we might have removed expired tenors
        Size tenorStartIndex = correlHandles_.front().size() - correlations_.columns();
        for (Size i=0; i<correlations_.rows(); ++i)
            for (Size j=0; j<correlations_.columns(); ++j)
                correlations_[i][j] = correlHandles_[i][tenorStartIndex+j]->value();

    }

}

#endif
