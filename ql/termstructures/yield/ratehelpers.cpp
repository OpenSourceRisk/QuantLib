/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 StatPro Italia srl
 Copyright (C) 2007, 2008, 2009, 2015 Ferdinando Ametrano
 Copyright (C) 2007, 2009 Roland Lichters
 Copyright (C) 2015 Maddalena Zanzi
 Copyright (C) 2015 Paolo Mazzocchi
 Copyright (C) 2018 Matthias Lungwitz

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

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/couponpricer.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/currency.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/simplifynotificationgraph.hpp>
#include <ql/optional.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quote.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/time/asx.hpp>
#include <ql/time/calendars/jointcalendar.hpp>
#include <ql/time/imm.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <utility>

namespace QuantLib {

    namespace {

        void CheckDate(const Date& date, const Futures::Type type) {
            switch (type) {
              case Futures::IMM:
                QL_REQUIRE(IMM::isIMMdate(date, false), date << " is not a valid IMM date");
                break;
              case Futures::ASX:
                QL_REQUIRE(ASX::isASXdate(date, false), date << " is not a valid ASX date");
                break;
              case Futures::Custom:
                break;
              default:
                QL_FAIL("unknown futures type (" << type << ')');
            }
        }

        Time DetermineYearFraction(const Date& earliestDate,
                                   const Date& maturityDate,
                                   const DayCounter& dayCounter) {
            return dayCounter.yearFraction(earliestDate, maturityDate,
                                           earliestDate, maturityDate);
        }

    } // namespace

    FuturesRateHelper::FuturesRateHelper(const std::variant<Real, Handle<Quote>>& price,
                                         const Date& iborStartDate,
                                         Natural lengthInMonths,
                                         const Calendar& calendar,
                                         BusinessDayConvention convention,
                                         bool endOfMonth,
                                         const DayCounter& dayCounter,
                                         const std::variant<Real, Handle<Quote>>& convAdj,
                                         Futures::Type type,
                                         Pillar::Choice pillarChoice,
                                         const Date& customPillarDate)
    : RateHelper(price), convAdj_(handleFromVariant(convAdj)), pillarChoice_(pillarChoice) {
        CheckDate(iborStartDate, type);

        earliestDate_ = iborStartDate;
        maturityDate_ =
            calendar.advance(iborStartDate, lengthInMonths * Months, convention, endOfMonth);
        yearFraction_ = DetermineYearFraction(earliestDate_, maturityDate_, dayCounter);
        latestRelevantDate_ = maturityDate_;
        pillarDate_ = customPillarDate;

        dayCounter_ = dayCounter;

        registerWith(convAdj_);
        setPillarDate();
    }

    FuturesRateHelper::FuturesRateHelper(const std::variant<Real, Handle<Quote>>& price,
                                         const Date& iborStartDate,
                                         const Date& iborEndDate,
                                         const DayCounter& dayCounter,
                                         const std::variant<Real, Handle<Quote>>& convAdj,
                                         Futures::Type type,
                                         Pillar::Choice pillarChoice,
                                         const Date& customPillarDate)
    : RateHelper(price), convAdj_(handleFromVariant(convAdj)), pillarChoice_(pillarChoice) {
        CheckDate(iborStartDate, type);

        const auto determineMaturityDate =
            [&iborStartDate, &iborEndDate](const auto nextDateCalculator) -> Date {
                Date maturityDate;
                if (iborEndDate == Date()) {
                    // advance 3 months
                    maturityDate = nextDateCalculator(iborStartDate);
                    maturityDate = nextDateCalculator(maturityDate);
                    maturityDate = nextDateCalculator(maturityDate);
                } else {
                    QL_REQUIRE(iborEndDate > iborStartDate,
                               "end date (" << iborEndDate << ") must be greater than start date ("
                                            << iborStartDate << ')');
                    maturityDate = iborEndDate;
                }
                return maturityDate;
            };

        switch (type) {
          case Futures::IMM:
            maturityDate_ = determineMaturityDate(
                [](const Date date) -> Date { return IMM::nextDate(date, false); });
            break;
          case Futures::ASX:
            maturityDate_ = determineMaturityDate(
                [](const Date date) -> Date { return ASX::nextDate(date, false); });
            break;
          case Futures::Custom:
            maturityDate_ = iborEndDate;
            break;
          default:
            QL_FAIL("unsupported futures type (" << type << ')');
        }
        earliestDate_ = iborStartDate;
        yearFraction_ = DetermineYearFraction(earliestDate_, maturityDate_, dayCounter);
        latestRelevantDate_ = maturityDate_;
        pillarDate_ = customPillarDate;

        dayCounter_ = dayCounter;

        registerWith(convAdj_);
        setPillarDate();
    }

    FuturesRateHelper::FuturesRateHelper(const std::variant<Real, Handle<Quote>>& price,
                                         const Date& iborStartDate,
                                         const ext::shared_ptr<IborIndex>& index,
                                         const std::variant<Real, Handle<Quote>>& convAdj,
                                         Futures::Type type,
                                         Pillar::Choice pillarChoice,
                                         const Date& customPillarDate)
    : RateHelper(price), convAdj_(handleFromVariant(convAdj)), pillarChoice_(pillarChoice) {
        CheckDate(iborStartDate, type);

        earliestDate_ = iborStartDate;
        const Calendar& cal = index->fixingCalendar();
        maturityDate_ =
            cal.advance(iborStartDate, index->tenor(), index->businessDayConvention());
        yearFraction_ = DetermineYearFraction(earliestDate_, maturityDate_, index->dayCounter());
        latestRelevantDate_ = maturityDate_;
        pillarDate_ = customPillarDate;

        dayCounter_ = index->dayCounter();

        registerWith(convAdj_);
        setPillarDate();
    }

    void FuturesRateHelper::setPillarDate() {
        switch (pillarChoice_) {
          case Pillar::MaturityDate:
            pillarDate_ = maturityDate_;
            break;
          case Pillar::LastRelevantDate:
            pillarDate_ = latestRelevantDate_;
            break;
          case Pillar::StartDate:
            pillarDate_ = earliestDate_;
            break;
          case Pillar::CustomDate:
            // pillarDate_ already assigned at construction time
            QL_REQUIRE(pillarDate_ >= earliestDate_,
                "pillar date (" << pillarDate_ << ") must be later "
                "than or equal to the instrument's earliest date (" <<
                earliestDate_ << ")");
            QL_REQUIRE(pillarDate_ <= latestRelevantDate_,
                "pillar date (" << pillarDate_ << ") must be before "
                "or equal to the instrument's latest relevant date (" <<
                latestRelevantDate_ << ")");
            break;
          default:
            QL_FAIL("unknown Pillar::Choice(" << Integer(pillarChoice_) << ")");
        }
        latestDate_ = pillarDate_; // backward compatibility
    }

    Real FuturesRateHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_ != nullptr, "term structure not set");
        Rate forwardRate = (termStructure_->discount(earliestDate_) /
            termStructure_->discount(maturityDate_) - 1.0) / yearFraction_;
        // Convexity, as FRA/futures adjustment, has been used in the
        // past to take into account futures margining vs FRA.
        // Therefore, there's no requirement for it to be non-negative.
        Rate futureRate = forwardRate + convexityAdjustment();
        return 100.0 * (1.0 - futureRate);
    }

    Real FuturesRateHelper::convexityAdjustment() const {
        return convAdj_.empty() ? 0.0 : convAdj_->value();
    }

    void FuturesRateHelper::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<FuturesRateHelper>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }

    DepositRateHelper::DepositRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                         const Period& tenor,
                                         Natural fixingDays,
                                         const Calendar& calendar,
                                         BusinessDayConvention convention,
                                         bool endOfMonth,
                                         const DayCounter& dayCounter,
                                         Pillar::Choice pillarChoice,
                                         const Date& customPillarDate)
    : RelativeDateRateHelper(rate), pillarChoice_(pillarChoice) {
        iborIndex_ = ext::make_shared<IborIndex>("no-fix", // never take fixing into account
                      tenor, fixingDays,
                      Currency(), calendar, convention,
                      endOfMonth, dayCounter, termStructureHandle_);
        pillarDate_ = customPillarDate;
        DepositRateHelper::initializeDates();
    }

    DepositRateHelper::DepositRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                         const ext::shared_ptr<IborIndex>& i,
                                         Pillar::Choice pillarChoice,
                                         const Date& customPillarDate)
    : RelativeDateRateHelper(rate), pillarChoice_(pillarChoice) {
        iborIndex_ = i->clone(termStructureHandle_);
        pillarDate_ = customPillarDate;
        DepositRateHelper::initializeDates();
    }

    DepositRateHelper::DepositRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                         Date fixingDate,
                                         const ext::shared_ptr<IborIndex>& i,
                                         Pillar::Choice pillarChoice,
                                         const Date& customPillarDate)
    : RelativeDateRateHelper(rate, false), fixingDate_(fixingDate), pillarChoice_(pillarChoice) {
        iborIndex_ = i->clone(termStructureHandle_);
        pillarDate_ = customPillarDate;
        DepositRateHelper::initializeDates();
    }

    Real DepositRateHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_ != nullptr, "term structure not set");
        // the forecast fixing flag is set to true because
        // we do not want to take fixing into account
        return iborIndex_->fixing(fixingDate_, true);
    }

    void DepositRateHelper::setTermStructure(YieldTermStructure* t) {
        // do not set the relinkable handle as an observer -
        // force recalculation when needed---the index is not lazy
        bool observer = false;

        ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
        termStructureHandle_.linkTo(temp, observer);

        RelativeDateRateHelper::setTermStructure(t);
    }

    void DepositRateHelper::initializeDates() {
        if (updateDates_) {
            // if the evaluation date is not a business day
            // then move to the next business day
            Date referenceDate = iborIndex_->fixingCalendar().adjust(evaluationDate_);
            earliestDate_ = iborIndex_->valueDate(referenceDate);
            fixingDate_ = iborIndex_->fixingDate(earliestDate_);
        } else {
            earliestDate_ = iborIndex_->valueDate(fixingDate_);
        }
        latestRelevantDate_ = maturityDate_ = iborIndex_->maturityDate(earliestDate_);
        switch (pillarChoice_) {
          case Pillar::MaturityDate:
            pillarDate_ = maturityDate_;
            break;
          case Pillar::LastRelevantDate:
            pillarDate_ = latestRelevantDate_;
            break;
          case Pillar::StartDate:
            pillarDate_ = earliestDate_;
            break;
          case Pillar::CustomDate:
            // pillarDate_ already assigned at construction time
            QL_REQUIRE(pillarDate_ >= earliestDate_,
                       "pillar date (" << pillarDate_ << ") must be later "
                       "than or equal to the instrument's earliest date (" <<
                       earliestDate_ << ")");
            QL_REQUIRE(pillarDate_ <= latestRelevantDate_,
                       "pillar date (" << pillarDate_ << ") must be before "
                       "or equal to the instrument's latest relevant date (" <<
                       latestRelevantDate_ << ")");
            break;
          default:
            QL_FAIL("unknown Pillar::Choice(" << Integer(pillarChoice_) << ")");
        }

        latestDate_ = pillarDate_; // backward compatibility
    }

    ext::shared_ptr<IborCoupon> DepositRateHelper::iborCoupon() const {
        auto coupon = ext::make_shared<IborCoupon>(maturityDate_, 1.0, earliestDate_, maturityDate_,
                                                   iborIndex_->fixingDays(), iborIndex_);
        coupon->setPricer(ext::make_shared<BlackIborCouponPricer>(
            Handle<OptionletVolatilityStructure>(),
            BlackIborCouponPricer::TimingAdjustment::Black76,
            Handle<Quote>(ext::make_shared<SimpleQuote>(1.0)), true));
        return coupon;
    }

    void DepositRateHelper::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<DepositRateHelper>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }


    FraRateHelper::FraRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                 Natural monthsToStart,
                                 Natural monthsToEnd,
                                 Natural fixingDays,
                                 const Calendar& calendar,
                                 BusinessDayConvention convention,
                                 bool endOfMonth,
                                 const DayCounter& dayCounter,
                                 Pillar::Choice pillarChoice,
                                 Date customPillarDate,
                                 bool useIndexedCoupon)
    : FraRateHelper(rate, monthsToStart*Months, monthsToEnd-monthsToStart, fixingDays, calendar,
        convention, endOfMonth, dayCounter, pillarChoice, customPillarDate, useIndexedCoupon) {
        QL_REQUIRE(monthsToEnd>monthsToStart,
                   "monthsToEnd (" << monthsToEnd <<
                   ") must be grater than monthsToStart (" << monthsToStart <<
                   ")");
    }

    FraRateHelper::FraRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                 Natural monthsToStart,
                                 const ext::shared_ptr<IborIndex>& i,
                                 Pillar::Choice pillarChoice,
                                 Date customPillarDate,
                                 bool useIndexedCoupon)
    : FraRateHelper(rate, monthsToStart*Months, i, pillarChoice, customPillarDate, useIndexedCoupon)
    {}

    FraRateHelper::FraRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                 Period periodToStart,
                                 Natural lengthInMonths,
                                 Natural fixingDays,
                                 const Calendar& calendar,
                                 BusinessDayConvention convention,
                                 bool endOfMonth,
                                 const DayCounter& dayCounter,
                                 Pillar::Choice pillarChoice,
                                 Date customPillarDate,
                                 bool useIndexedCoupon)
    : RelativeDateRateHelper(rate), periodToStart_(periodToStart),
      pillarChoice_(pillarChoice), useIndexedCoupon_(useIndexedCoupon) {
        // no way to take fixing into account,
        // even if we would like to for FRA over today
        iborIndex_ = ext::make_shared<IborIndex>("no-fix", // correct family name would be needed
                      lengthInMonths*Months,
                      fixingDays,
                      Currency(), calendar, convention,
                      endOfMonth, dayCounter, termStructureHandle_);
        pillarDate_ = customPillarDate;
        FraRateHelper::initializeDates();
    }

    FraRateHelper::FraRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                 Period periodToStart,
                                 const ext::shared_ptr<IborIndex>& i,
                                 Pillar::Choice pillarChoice,
                                 Date customPillarDate,
                                 bool useIndexedCoupon)
    : RelativeDateRateHelper(rate), periodToStart_(periodToStart),
      pillarChoice_(pillarChoice), useIndexedCoupon_(useIndexedCoupon) {
        // take fixing into account
        iborIndex_ = i->clone(termStructureHandle_);
        // We want to be notified of changes of fixings, but we don't
        // want notifications from termStructureHandle_ (they would
        // interfere with bootstrapping.)
        iborIndex_->unregisterWith(termStructureHandle_);
        registerWith(iborIndex_);
        pillarDate_ = customPillarDate;
        FraRateHelper::initializeDates();
    }

    FraRateHelper::FraRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                 Natural immOffsetStart,
                                 Natural immOffsetEnd,
                                 const ext::shared_ptr<IborIndex>& i,
                                 Pillar::Choice pillarChoice,
                                 Date customPillarDate,
                                 bool useIndexedCoupon)
    : RelativeDateRateHelper(rate), immOffsetStart_(immOffsetStart), immOffsetEnd_(immOffsetEnd),
      pillarChoice_(pillarChoice), useIndexedCoupon_(useIndexedCoupon) {
        // take fixing into account
        iborIndex_ = i->clone(termStructureHandle_);
        // see above
        iborIndex_->unregisterWith(termStructureHandle_);
        registerWith(iborIndex_);
        pillarDate_ = customPillarDate;
        FraRateHelper::initializeDates();
    }

    FraRateHelper::FraRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                 Date startDate,
                                 Date endDate,
                                 const ext::shared_ptr<IborIndex>& i,
                                 Pillar::Choice pillarChoice,
                                 Date customPillarDate,
                                 bool useIndexedCoupon)
    : RelativeDateRateHelper(rate, false), pillarChoice_(pillarChoice),
      useIndexedCoupon_(useIndexedCoupon) {
        // take fixing into account
        iborIndex_ = i->clone(termStructureHandle_);
        // see above
        iborIndex_->unregisterWith(termStructureHandle_);
        registerWith(iborIndex_);
        earliestDate_ = startDate;
        maturityDate_ = endDate;
        pillarDate_ = customPillarDate;
        FraRateHelper::initializeDates();
    }

    Real FraRateHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_ != nullptr, "term structure not set");
        if (useIndexedCoupon_)
            return iborIndex_->fixing(fixingDate_, true);
        else
            return (termStructure_->discount(earliestDate_) /
                        termStructure_->discount(maturityDate_) -
                    1.0) /
                   spanningTime_;
    }

    void FraRateHelper::setTermStructure(YieldTermStructure* t) {
        // do not set the relinkable handle as an observer -
        // force recalculation when needed---the index is not lazy
        bool observer = false;

        ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
        termStructureHandle_.linkTo(temp, observer);

        RelativeDateRateHelper::setTermStructure(t);
    }

    namespace {
        Date nthImmDate(const Date& asof, const Size n) {
            Date imm = asof;
            for (Size i = 0; i < n; ++i) {
                imm = IMM::nextDate(imm, true);
            }
            return imm;
        }
    }

    void FraRateHelper::initializeDates() {
        if (updateDates_) {
            // if the evaluation date is not a business day
            // then move to the next business day
            Date referenceDate =
                iborIndex_->fixingCalendar().adjust(evaluationDate_);
            Date spotDate = iborIndex_->fixingCalendar().advance(
                referenceDate, iborIndex_->fixingDays()*Days);
            if (periodToStart_) { // NOLINT(readability-implicit-bool-conversion)
                earliestDate_ = iborIndex_->fixingCalendar().advance(
                    spotDate, *periodToStart_, iborIndex_->businessDayConvention(),
                    iborIndex_->endOfMonth());
                // maturity date is calculated from spot date
                maturityDate_ = iborIndex_->fixingCalendar().advance(
                    spotDate, *periodToStart_ + iborIndex_->tenor(), iborIndex_->businessDayConvention(),
                    iborIndex_->endOfMonth());

            } else if ((immOffsetStart_) && (immOffsetEnd_)) { // NOLINT(readability-implicit-bool-conversion)
                earliestDate_ = iborIndex_->fixingCalendar().adjust(nthImmDate(spotDate, *immOffsetStart_));
                maturityDate_ = iborIndex_->fixingCalendar().adjust(nthImmDate(spotDate, *immOffsetEnd_));
            } else {
                QL_FAIL("neither periodToStart nor immOffsetStart/End given");
            }
        }

        if (useIndexedCoupon_)
            // latest relevant date is calculated from earliestDate_
            latestRelevantDate_ = iborIndex_->maturityDate(earliestDate_);
        else {
            latestRelevantDate_ = maturityDate_;
            spanningTime_ = iborIndex_->dayCounter().yearFraction(earliestDate_, maturityDate_);
        }

        switch (pillarChoice_) {
          case Pillar::MaturityDate:
            pillarDate_ = maturityDate_;
            break;
          case Pillar::LastRelevantDate:
            pillarDate_ = latestRelevantDate_;
            break;
          case Pillar::StartDate:
            pillarDate_ = earliestDate_;
            break;
          case Pillar::CustomDate:
            // pillarDate_ already assigned at construction time
            QL_REQUIRE(pillarDate_ >= earliestDate_,
                       "pillar date (" << pillarDate_ << ") must be later "
                       "than or equal to the instrument's earliest date (" <<
                       earliestDate_ << ")");
            QL_REQUIRE(pillarDate_ <= latestRelevantDate_,
                       "pillar date (" << pillarDate_ << ") must be before "
                       "or equal to the instrument's latest relevant date (" <<
                       latestRelevantDate_ << ")");
            break;
          default:
            QL_FAIL("unknown Pillar::Choice(" << Integer(pillarChoice_) << ")");
        }

        latestDate_ = pillarDate_; // backward compatibility

        fixingDate_ = iborIndex_->fixingDate(earliestDate_);
    }

    void FraRateHelper::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<FraRateHelper>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }

    ext::shared_ptr<IborCoupon> FraRateHelper::iborCoupon() const {
        auto coupon = ext::make_shared<IborCoupon>(maturityDate_, 1.0, earliestDate_, maturityDate_,
                                                   iborIndex_->fixingDays(), iborIndex_);
        coupon->setPricer(ext::make_shared<BlackIborCouponPricer>(
            Handle<OptionletVolatilityStructure>(),
            BlackIborCouponPricer::TimingAdjustment::Black76,
            Handle<Quote>(ext::make_shared<SimpleQuote>(1.0)), useIndexedCoupon_));
        return coupon;
    }

    SwapRateHelper::SwapRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                   const ext::shared_ptr<SwapIndex>& swapIndex,
                                   Handle<Quote> spread,
                                   const Period& fwdStart,
                                   Handle<YieldTermStructure> discount,
                                   Pillar::Choice pillarChoice,
                                   Date customPillarDate,
                                   bool endOfMonth,
                                   const ext::optional<bool>& useIndexedCoupons)
    : SwapRateHelper(rate, swapIndex->tenor(), swapIndex->fixingCalendar(),
        swapIndex->fixedLegTenor().frequency(), swapIndex->fixedLegConvention(),
        swapIndex->dayCounter(), swapIndex->iborIndex(), std::move(spread), fwdStart,
        std::move(discount), Null<Natural>(), pillarChoice, customPillarDate, endOfMonth,
        useIndexedCoupons) {}

    SwapRateHelper::SwapRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                   const Period& tenor,
                                   Calendar calendar,
                                   Frequency fixedFrequency,
                                   BusinessDayConvention fixedConvention,
                                   DayCounter fixedDayCount,
                                   const ext::shared_ptr<IborIndex>& iborIndex,
                                   Handle<Quote> spread,
                                   const Period& fwdStart,
                                   Handle<YieldTermStructure> discount,
                                   Natural settlementDays,
                                   Pillar::Choice pillarChoice,
                                   Date customPillarDate,
                                   bool endOfMonth,
                                   const ext::optional<bool>& useIndexedCoupons,
                                   const ext::optional<BusinessDayConvention>& floatConvention)
    : RelativeDateRateHelper(rate), settlementDays_(settlementDays), tenor_(tenor),
      pillarChoice_(pillarChoice), calendar_(std::move(calendar)),
      fixedConvention_(fixedConvention), fixedFrequency_(fixedFrequency),
      fixedDayCount_(std::move(fixedDayCount)), spread_(std::move(spread)), endOfMonth_(endOfMonth),
      fwdStart_(fwdStart), discountHandle_(std::move(discount)),
      useIndexedCoupons_(useIndexedCoupons), floatConvention_(floatConvention) {
        initialize(iborIndex, customPillarDate);
    }

    SwapRateHelper::SwapRateHelper(const std::variant<Rate, Handle<Quote>>& rate,
                                   const Date& startDate,
                                   const Date& endDate,
                                   Calendar calendar,
                                   Frequency fixedFrequency,
                                   BusinessDayConvention fixedConvention,
                                   DayCounter fixedDayCount,
                                   const ext::shared_ptr<IborIndex>& iborIndex,
                                   Handle<Quote> spread,
                                   Handle<YieldTermStructure> discount,
                                   Pillar::Choice pillarChoice,
                                   Date customPillarDate,
                                   bool endOfMonth,
                                   const ext::optional<bool>& useIndexedCoupons,
                                   const ext::optional<BusinessDayConvention>& floatConvention)
    : RelativeDateRateHelper(rate, false), startDate_(startDate), endDate_(endDate),
      pillarChoice_(pillarChoice), calendar_(std::move(calendar)),
      fixedConvention_(fixedConvention), fixedFrequency_(fixedFrequency),
      fixedDayCount_(std::move(fixedDayCount)), spread_(std::move(spread)), endOfMonth_(endOfMonth),
      discountHandle_(std::move(discount)), useIndexedCoupons_(useIndexedCoupons),
      floatConvention_(floatConvention) {
        QL_REQUIRE(fixedFrequency != Once,
            "fixedFrequency == Once is not supported when passing explicit "
            "startDate and endDate");
        initialize(iborIndex, customPillarDate);
    }

    void SwapRateHelper::initialize(const ext::shared_ptr<IborIndex>& iborIndex,
                                    Date customPillarDate) {
        // take fixing into account
        iborIndex_ = iborIndex->clone(termStructureHandle_);
        // We want to be notified of changes of fixings, but we don't
        // want notifications from termStructureHandle_ (they would
        // interfere with bootstrapping.)
        iborIndex_->unregisterWith(termStructureHandle_);

        registerWith(iborIndex_);
        registerWith(spread_);
        registerWith(discountHandle_);

        pillarDate_ = customPillarDate;
        SwapRateHelper::initializeDates();
    }

    void SwapRateHelper::initializeDates() {

        // 1. do not pass the spread here, as it might be a Quote
        //    i.e. it can dynamically change
        // 2. input discount curve Handle might be empty now but it could
        //    be assigned a curve later; use a RelinkableHandle here
        auto tmp =
            MakeVanillaSwap(tenor_, iborIndex_,
                            quote().empty() || !quote()->isValid() ? 0.0 : quote()->value(),
                            fwdStart_)
                .withEffectiveDate(startDate_)
                .withTerminationDate(endDate_)
                .withDiscountingTermStructure(discountRelinkableHandle_)
                .withFixedLegDayCount(fixedDayCount_)
                .withFixedLegTenor(fixedFrequency_ == Once ? tenor_ : Period(fixedFrequency_))
                .withFixedLegConvention(fixedConvention_)
                .withFixedLegTerminationDateConvention(fixedConvention_)
                .withFixedLegCalendar(calendar_)
                .withFixedLegEndOfMonth(endOfMonth_)
                .withFloatingLegCalendar(calendar_)
                .withFloatingLegEndOfMonth(endOfMonth_)
                .withIndexedCoupons(useIndexedCoupons_);
        if (floatConvention_) {
            tmp.withFloatingLegConvention(*floatConvention_)
               .withFloatingLegTerminationDateConvention(*floatConvention_);
        }
        // only set settlementDays when no explicit start date, to avoid conflict
        if (startDate_ == Date() && settlementDays_ != Null<Natural>())
            tmp.withSettlementDays(settlementDays_);
        swap_ = tmp;

        simplifyNotificationGraph(*swap_, true);

        earliestDate_ = swap_->startDate();
        maturityDate_ = swap_->maturityDate();

        ext::shared_ptr<IborCoupon> lastCoupon =
            ext::dynamic_pointer_cast<IborCoupon>(swap_->floatingLeg().back());
        latestRelevantDate_ = std::max(maturityDate_, lastCoupon->fixingEndDate());

        switch (pillarChoice_) {
          case Pillar::MaturityDate:
            pillarDate_ = maturityDate_;
            break;
          case Pillar::LastRelevantDate:
            pillarDate_ = latestRelevantDate_;
            break;
          case Pillar::StartDate:
            pillarDate_ = earliestDate_;
            break;
          case Pillar::CustomDate:
            // pillarDate_ already assigned at construction time
            QL_REQUIRE(pillarDate_ >= earliestDate_,
                "pillar date (" << pillarDate_ << ") must be later "
                "than or equal to the instrument's earliest date (" <<
                earliestDate_ << ")");
            QL_REQUIRE(pillarDate_ <= latestRelevantDate_,
                "pillar date (" << pillarDate_ << ") must be before "
                "or equal to the instrument's latest relevant date (" <<
                latestRelevantDate_ << ")");
            break;
          default:
            QL_FAIL("unknown Pillar::Choice(" << Integer(pillarChoice_) << ")");
        }

        latestDate_ = pillarDate_; // backward compatibility

    }

    void SwapRateHelper::setTermStructure(YieldTermStructure* t) {
        // do not set the relinkable handle as an observer -
        // force recalculation when needed
        bool observer = false;

        ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
        termStructureHandle_.linkTo(temp, observer);

        if (discountHandle_.empty())
            discountRelinkableHandle_.linkTo(temp, observer);
        else
            discountRelinkableHandle_.linkTo(*discountHandle_, observer);

        RelativeDateRateHelper::setTermStructure(t);
    }

    Real SwapRateHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_ != nullptr, "term structure not set");
        // we didn't register as observers - force calculation
        swap_->deepUpdate();
        // weak implementation... to be improved
        static const Spread basisPoint = 1.0e-4;
        Real floatingLegNPV = swap_->floatingLegNPV();
        Spread spread = spread_.empty() ? 0.0 : spread_->value();
        Real spreadNPV = swap_->floatingLegBPS()/basisPoint*spread;
        Real totNPV = - (floatingLegNPV+spreadNPV);
        Real result = totNPV/(swap_->fixedLegBPS()/basisPoint);
        return result;
    }

    void SwapRateHelper::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<SwapRateHelper>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }

    BMASwapRateHelper::BMASwapRateHelper(const Handle<Quote>& indexFraction,
                                         const Period& tenor,
                                         Natural bmaSettlementDays,
                                         const Calendar& calendar,
                                         // bma leg
                                         const Period& bmaPeriod,
                                         BusinessDayConvention bmaConvention,
                                         const DayCounter& bmaDayCount,
                                         const ext::shared_ptr<BMAIndex>& bmaIndex,
                                         // ibor / ois leg
                                         const ext::shared_ptr<IborIndex>& index,
                                         // external discount
                                         const Handle<YieldTermStructure>& discountingCurve,
                                         Pillar::Choice pillarChoice,
                                         const Date& customPillarDate)
    : RelativeDateRateHelper(indexFraction), tenor_(tenor), bmaSettlementDays_(bmaSettlementDays),
      bmaCalendar_(calendar), bmaPeriod_(bmaPeriod), bmaConvention_(bmaConvention),
      bmaDayCount_(bmaDayCount), bmaIndex_(bmaIndex), bmaPaymentCalendar_(calendar),
      bmaPaymentConvention_(Following), bmaPaymentLag_(0), indexSettlementDays_(bmaSettlementDays),
      indexPaymentPeriod_(bmaPeriod), indexConvention_(index->businessDayConvention()),
      index_(index), indexPaymentCalendar_(index->fixingCalendar()),
      indexPaymentConvention_(Following), indexPaymentLag_(0), overnightLockoutDays_(0),
      discountingCurve_(discountingCurve), pillarChoice_(pillarChoice),
      customPillarDate_(customPillarDate) {
        registerWith(index_);
        registerWith(bmaIndex_);
        BMASwapRateHelper::initializeDates();
    }

    BMASwapRateHelper::BMASwapRateHelper(const Handle<Quote>& indexFraction,
                                         const Period& tenor,
                                         // bma leg
                                         Natural bmaSettlementDays,
                                         const Calendar& bmaCalendar,
                                         const Period& bmaPeriod,
                                         BusinessDayConvention bmaConvention,
                                         const DayCounter& bmaDayCount,
                                         const ext::shared_ptr<BMAIndex>& bmaIndex,
                                         const Calendar& bmaPaymentCalendar,
                                         BusinessDayConvention bmaPaymentConvention,
                                         Natural bmaPaymentLag,
                                         // ibor / ois leg
                                         Natural indexSettlementDays,
                                         const Period& indexPaymentPeriod,
                                         BusinessDayConvention indexConvention,
                                         const ext::shared_ptr<IborIndex>& index,
                                         const Calendar& indexPaymentCalendar,
                                         BusinessDayConvention indexPaymentConvention,
                                         Natural indexPaymentLag,
                                         Natural overnightLockoutDays,
                                         // external discount
                                         const Handle<YieldTermStructure>& discountingCurve,
                                         Pillar::Choice pillarChoice,
                                         const Date& customPillarDate)
    : RelativeDateRateHelper(indexFraction), tenor_(tenor), bmaSettlementDays_(bmaSettlementDays),
      bmaCalendar_(bmaCalendar), bmaPeriod_(bmaPeriod), bmaConvention_(bmaConvention),
      bmaDayCount_(bmaDayCount), bmaIndex_(bmaIndex), bmaPaymentCalendar_(bmaPaymentCalendar),
      bmaPaymentConvention_(Following), bmaPaymentLag_(0),
      indexSettlementDays_(indexSettlementDays), indexPaymentPeriod_(indexPaymentPeriod),
      indexConvention_(indexConvention), index_(index), indexPaymentCalendar_(indexPaymentCalendar),
      indexPaymentConvention_(indexPaymentConvention), indexPaymentLag_(indexPaymentLag),
      overnightLockoutDays_(overnightLockoutDays), discountingCurve_(discountingCurve),
      pillarChoice_(pillarChoice), customPillarDate_(customPillarDate) {
        registerWith(index_);
        registerWith(bmaIndex_);
        BMASwapRateHelper::initializeDates();
    }

    void BMASwapRateHelper::initializeDates() {

        // if the evaluation date is not a business day
        // then move to the next business day
        JointCalendar jc(bmaCalendar_, index_->fixingCalendar());
        Date referenceDate = jc.adjust(evaluationDate_);

        Date bmaStart = bmaCalendar_.advance(referenceDate, bmaSettlementDays_ * Days, Following);
        Date bmaMaturity = bmaStart + tenor_;

        Date indexStart = index_->fixingCalendar().advance(
            referenceDate, indexSettlementDays_ * Days, Following);
        Date indexMaturity = indexStart + tenor_;

        bool isOis = QuantLib::ext::dynamic_pointer_cast<OvernightIndex>(index_) != nullptr;

        bool indexEndOfMonth = isOis && index_->fixingCalendar().isEndOfMonth(indexStart);
        if (indexEndOfMonth)
            indexMaturity =
                index_->fixingCalendar().advance(indexStart, tenor_, ModifiedFollowing, true);

        earliestDate_ = std::min(bmaStart, indexStart);

        // dummy BMA index with curve/swap arguments
        ext::shared_ptr<BMAIndex> clonedIndex =
            QuantLib::ext::make_shared<BMAIndex>(termStructureHandle_);

        Schedule bmaSchedule = MakeSchedule()
                                   .from(bmaStart)
                                   .to(bmaMaturity)
                                   .withTenor(bmaPeriod_)
                                   .withCalendar(bmaCalendar_)
                                   .withConvention(bmaConvention_)
                                   .backwards();

        Schedule indexSchedule = MakeSchedule()
                                     .from(indexStart)
                                     .to(indexMaturity)
                                     .withTenor(indexPaymentPeriod_)
                                     .withCalendar(index_->fixingCalendar())
                                     .withConvention(indexConvention_)
                                     .endOfMonth(indexEndOfMonth)
                                     .backwards();

        swap_ = ext::make_shared<BMASwap>(
            Swap::Payer, 100.0, indexSchedule,
            quote().empty() || !quote()->isValid() ? 0.75 : quote()->value(), 0.0, index_,
            index_->dayCounter(), bmaSchedule, clonedIndex, bmaDayCount_, indexPaymentCalendar_,
            indexPaymentConvention_, indexPaymentLag_, bmaPaymentCalendar_, bmaPaymentConvention_,
            bmaPaymentLag_, overnightLockoutDays_, true);

        swap_->setPricingEngine(QuantLib::ext::make_shared<DiscountingSwapEngine>(
            !discountingCurve_.empty() ? discountingCurve_ : index_->forwardingTermStructure()));

        maturityDate_ = swap_->maturityDate();

        Date d = bmaCalendar_.adjust(CashFlows::maturityDate(swap_->bmaLeg()), Following);
        Weekday w = d.weekday();
        Date nextWednesday = (w >= 4) ?
            d + (11 - w) * Days :
            d + (4 - w) * Days;
        latestRelevantDate_ =
            clonedIndex->valueDate(clonedIndex->fixingCalendar().adjust(nextWednesday));

        if (auto ibor = ext::dynamic_pointer_cast<IborCoupon>(swap_->indexLeg().back());
            ibor != nullptr)
            latestRelevantDate_ = std::max(latestRelevantDate_, ibor->fixingEndDate());

        switch (pillarChoice_) {
          case Pillar::MaturityDate:
            pillarDate_ = maturityDate_;
            break;
          case Pillar::LastRelevantDate:
            pillarDate_ = latestRelevantDate_;
            break;
          case Pillar::StartDate:
            pillarDate_ = earliestDate_;
            break;
          case Pillar::CustomDate:
            pillarDate_ = customPillarDate_;
            QL_REQUIRE(pillarDate_ >= earliestDate_,
                "pillar date (" << pillarDate_ << ") must be later "
                "than or equal to the instrument's earliest date (" <<
                earliestDate_ << ")");
            QL_REQUIRE(pillarDate_ <= latestRelevantDate_,
                "pillar date (" << pillarDate_ << ") must be before "
                "or equal to the instrument's latest relevant date (" <<
                latestRelevantDate_ << ")");
            break;
          default:
            QL_FAIL("unknown Pillar::Choice(" << Integer(pillarChoice_) << ")");
        }

        latestDate_ = pillarDate_; // backward compatibility

    }

    RelinkableHandle<YieldTermStructure> BMASwapRateHelper::termStructureHandle() const {
        return termStructureHandle_;
    }

    void BMASwapRateHelper::setTermStructure(YieldTermStructure* t) {
        // do not set the relinkable handle as an observer -
        // force recalculation when needed
        bool observer = false;

        ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
        termStructureHandle_.linkTo(temp, observer);

        RelativeDateRateHelper::setTermStructure(t);
    }

    Real BMASwapRateHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_ != nullptr, "term structure not set");
        // we didn't register as observers - force calculation
        swap_->deepUpdate();
        return swap_->fairIndexFraction();
    }

    void BMASwapRateHelper::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<BMASwapRateHelper>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }

    FxSwapRateHelper::FxSwapRateHelper(const Handle<Quote>& fwdPoint,
                                       Handle<Quote> spotFx,
                                       const Period& tenor,
                                       Natural fixingDays,
                                       Calendar calendar,
                                       BusinessDayConvention convention,
                                       bool endOfMonth,
                                       bool isFxBaseCurrencyCollateralCurrency,
                                       Handle<YieldTermStructure> coll,
                                       Calendar tradingCalendar,
                                       Pillar::Choice pillarChoice,
                                       const Date& customPillarDate)
    : RelativeDateRateHelper(fwdPoint), spot_(std::move(spotFx)), tenor_(tenor),
      fixingDays_(fixingDays), cal_(std::move(calendar)), conv_(convention), eom_(endOfMonth),
      isFxBaseCurrencyCollateralCurrency_(isFxBaseCurrencyCollateralCurrency),
      collHandle_(std::move(coll)), tradingCalendar_(std::move(tradingCalendar)),
      pillarChoice_(pillarChoice) {
        registerWith(spot_);
        registerWith(collHandle_);

        if (tradingCalendar_.empty())
            jointCalendar_ = cal_;
        else
            jointCalendar_ = JointCalendar(tradingCalendar_, cal_,
                                           JoinHolidays);
        pillarDate_ = customPillarDate;
        FxSwapRateHelper::initializeDates();
    }

    FxSwapRateHelper::FxSwapRateHelper(const Handle<Quote>& fwdPoint,
                                       Handle<Quote> spotFx,
                                       const Date& startDate,
                                       const Date& endDate,
                                       bool isFxBaseCurrencyCollateralCurrency,
                                       Handle<YieldTermStructure> coll,
                                       Pillar::Choice pillarChoice,
                                       const Date& customPillarDate)
    : RelativeDateRateHelper(fwdPoint, false), spot_(std::move(spotFx)),
      isFxBaseCurrencyCollateralCurrency_(isFxBaseCurrencyCollateralCurrency),
      collHandle_(std::move(coll)), pillarChoice_(pillarChoice) {
        registerWith(spot_);
        registerWith(collHandle_);
        earliestDate_ = startDate;
        latestRelevantDate_ = maturityDate_ = endDate;
        pillarDate_ = customPillarDate;
        setPillarDate();
    }

    void FxSwapRateHelper::initializeDates() {
        if (!updateDates_) return;
        // if the evaluation date is not a business day
        // then move to the next business day
        Date refDate = cal_.adjust(evaluationDate_);
        earliestDate_ = cal_.advance(refDate, fixingDays_*Days);

        if (!tradingCalendar_.empty()) {
            // check if fx trade can be settled in US, if not, adjust it
            earliestDate_ = jointCalendar_.adjust(earliestDate_);
            latestRelevantDate_ = maturityDate_ =
                jointCalendar_.advance(earliestDate_, tenor_, conv_, eom_);
        } else {
            latestRelevantDate_ = maturityDate_ = cal_.advance(earliestDate_, tenor_, conv_, eom_);
        }
        setPillarDate();
    }

    void FxSwapRateHelper::setPillarDate() {
        switch (pillarChoice_) {
          case Pillar::MaturityDate:
            pillarDate_ = maturityDate_;
            break;
          case Pillar::LastRelevantDate:
            pillarDate_ = latestRelevantDate_;
            break;
          case Pillar::StartDate:
            pillarDate_ = earliestDate_;
            break;
          case Pillar::CustomDate:
            // pillarDate_ already assigned at construction time
            QL_REQUIRE(pillarDate_ >= earliestDate_,
                "pillar date (" << pillarDate_ << ") must be later "
                "than or equal to the instrument's earliest date (" <<
                earliestDate_ << ")");
            QL_REQUIRE(pillarDate_ <= latestRelevantDate_,
                "pillar date (" << pillarDate_ << ") must be before "
                "or equal to the instrument's latest relevant date (" <<
                latestRelevantDate_ << ")");
            break;
          default:
            QL_FAIL("unknown Pillar::Choice(" << Integer(pillarChoice_) << ")");
        }
        latestDate_ = pillarDate_; // backward compatibility
    }

    Real FxSwapRateHelper::impliedQuote() const {
        QL_REQUIRE(termStructure_ != nullptr, "term structure not set");

        QL_REQUIRE(!collHandle_.empty(), "collateral term structure not set");

        DiscountFactor d1 = collHandle_->discount(earliestDate_);
        DiscountFactor d2 = collHandle_->discount(latestDate_);
        Real collRatio = d1 / d2;
        d1 = termStructureHandle_->discount(earliestDate_);
        d2 = termStructureHandle_->discount(latestDate_);
        Real ratio = d1 / d2;
        Real spot = spot_->value();
        if (isFxBaseCurrencyCollateralCurrency_) {
            return (ratio/collRatio-1)*spot;
        } else {
            return (collRatio/ratio-1)*spot;
        }
    }

    void FxSwapRateHelper::setTermStructure(YieldTermStructure* t) {
        // do not set the relinkable handle as an observer -
        // force recalculation when needed
        bool observer = false;

        ext::shared_ptr<YieldTermStructure> temp(t, null_deleter());
        termStructureHandle_.linkTo(temp, observer);

        collRelinkableHandle_.linkTo(*collHandle_, observer);

        RelativeDateRateHelper::setTermStructure(t);
    }

    void FxSwapRateHelper::accept(AcyclicVisitor& v) {
        auto* v1 = dynamic_cast<Visitor<FxSwapRateHelper>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            RateHelper::accept(v);
    }

}
