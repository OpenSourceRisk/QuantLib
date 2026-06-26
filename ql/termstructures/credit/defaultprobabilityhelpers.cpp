/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008, 2009 Jose Aparicio
 Copyright (C) 2008 Chris Kenyon
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2008 StatPro Italia srl
 Copyright (C) 2017 Quaternion Risk Management Ltd
 Copyright (C) 2023 Andrea Pellegatta

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

#include <ql/pricingengines/credit/isdacdsengine.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/termstructures/credit/defaultprobabilityhelpers.hpp>
#include <ql/utilities/null_deleter.hpp>
#include <utility>
#include <iostream>

namespace QuantLib {

    CdsHelper::CdsHelper(const std::variant<Rate, Handle<Quote>>& quote,
                         const Period& tenor,
                         Integer settlementDays,
                         Calendar calendar,
                         Frequency frequency,
                         BusinessDayConvention paymentConvention,
                         DateGeneration::Rule rule,
                         DayCounter dayCounter,
                         Real recoveryRate,
                         const Handle<YieldTermStructure>& discountCurve,
                         bool settlesAccrual,
                         bool paysAtDefaultTime,
                         const Date& startDate,
                         DayCounter lastPeriodDayCounter,
                         bool rebatesAccrual,
                         CreditDefaultSwap::PricingModel model)
    : RelativeDateDefaultProbabilityHelper(quote), tenor_(tenor), settlementDays_(settlementDays),
      calendar_(std::move(calendar)), frequency_(frequency), paymentConvention_(paymentConvention),
      rule_(rule), dayCounter_(std::move(dayCounter)), recoveryRate_(recoveryRate),
      discountCurve_(discountCurve), settlesAccrual_(settlesAccrual),
      //paysAtDefaultTime_(paysAtDefaultTime),
      protectionPaymentTime_(paysAtDefaultTime ?
                             CreditDefaultSwap::ProtectionPaymentTime::atDefault :
                             CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd),
      lastPeriodDC_(std::move(lastPeriodDayCounter)),
      rebatesAccrual_(rebatesAccrual), model_(model), startDate_(startDate) {

        CdsHelper::initializeDates();

        registerWith(discountCurve);
    }

    CdsHelper::CdsHelper(const std::variant<Rate, Handle<Quote>>& quote,
                         const Period& tenor,
                         Integer settlementDays,
                         const Calendar& calendar,
                         Frequency frequency,
                         BusinessDayConvention paymentConvention,
                         DateGeneration::Rule rule,
                         const DayCounter& dayCounter,
                         Real recoveryRate,
                         const Handle<YieldTermStructure>& discountCurve,
                         bool settlesAccrual,
                         CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                         const Date& startDate,
                         const DayCounter& lastPeriodDayCounter,
                         bool rebatesAccrual,
                         CreditDefaultSwap::PricingModel model)
    : RelativeDateDefaultProbabilityHelper(quote), tenor_(tenor), settlementDays_(settlementDays),
      calendar_(std::move(calendar)), frequency_(frequency), paymentConvention_(paymentConvention),
      rule_(rule), dayCounter_(std::move(dayCounter)), recoveryRate_(recoveryRate),
      discountCurve_(discountCurve), settlesAccrual_(settlesAccrual),
      protectionPaymentTime_(protectionPaymentTime), lastPeriodDC_(std::move(lastPeriodDayCounter)),
      rebatesAccrual_(rebatesAccrual), model_(model), startDate_(startDate) {

        initializeDates();

        registerWith(discountCurve);
    }

    CdsHelper::CdsHelper(const std::variant<Rate, Handle<Quote>>& quote,
                         Schedule schedule,
                         const DayCounter& dayCounter,
                         Real recoveryRate,
                         const Handle<YieldTermStructure>& discountCurve,
                         CreditDefaultSwap::PricingModel model,
                         bool settlesAccrual,
                         CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                         const DayCounter& lastPeriodDayCounter,
                         bool rebatesAccrual)
    : RelativeDateDefaultProbabilityHelper(quote), settlementDays_(0),
      dayCounter_(std::move(dayCounter)), recoveryRate_(recoveryRate),
      discountCurve_(discountCurve), settlesAccrual_(settlesAccrual),
      protectionPaymentTime_(protectionPaymentTime), lastPeriodDC_(std::move(lastPeriodDayCounter)),
      rebatesAccrual_(rebatesAccrual), model_(model), schedule_(schedule) {

        protectionStart_ = schedule[0];
        schedule_ = removeCDSPeriodsBeforeStartDate(schedule_, protectionStart_);
        earliestDate_ = schedule_.dates().front();
        latestDate_ = schedule_.calendar().adjust(schedule_.dates().back(),
                                                  schedule_.businessDayConvention());
        if (model_ == CreditDefaultSwap::ISDA)
            ++latestDate_;

        registerWith(discountCurve);
    }

    void CdsHelper::setTermStructure(DefaultProbabilityTermStructure* ts) {
        RelativeDateDefaultProbabilityHelper::setTermStructure(ts);

        probability_.linkTo(
            ext::shared_ptr<DefaultProbabilityTermStructure>(ts, null_deleter()),
            false);

        resetEngine();
    }

    void CdsHelper::update() {
        RelativeDateDefaultProbabilityHelper::update();
        resetEngine();
    }

    void CdsHelper::initializeDates() {

        protectionStart_ = evaluationDate_ + settlementDays_;
        if (schedule_.empty()) {
        
        
            Date startDate = startDate_ == Date() ? protectionStart_ : startDate_;
            if (rule_ != DateGeneration::CDS2015 && rule_ != DateGeneration::CDS) {
                startDate = calendar_.adjust(startDate, paymentConvention_);
            }

            Date endDate;
        
                if (rule_ == DateGeneration::CDS2015 || rule_ == DateGeneration::CDS || rule_ == DateGeneration::OldCDS) {
                    Date refDate = startDate_ == Date() ? evaluationDate_ : startDate_;
                    endDate = cdsMaturity(refDate, tenor_, rule_);
                } else {
                    // Keep the old logic here
                    Date refDate = startDate_ == Date() ? protectionStart_ : startDate_ + settlementDays_;
                    endDate = refDate + tenor_;
                }
        
            schedule_ =
                MakeSchedule().from(startDate)
                              .to(endDate)
                              .withFrequency(frequency_)
                              .withCalendar(calendar_)
                              .withConvention(paymentConvention_)
                              .withTerminationDateConvention(Unadjusted)
                              .withRule(rule_);
        }
        schedule_ = removeCDSPeriodsBeforeStartDate(schedule_, evaluationDate_ + 1);

        earliestDate_ = schedule_.dates().front();
        latestDate_   = schedule_.calendar().adjust(schedule_.dates().back(),
                                         schedule_.businessDayConvention());
        if (model_ == CreditDefaultSwap::ISDA)
            ++latestDate_;
    }

    // deprecated 
    SpreadCdsHelper::SpreadCdsHelper(
                              const std::variant<Rate, Handle<Quote>>& runningSpread,
                              const Period& tenor,
                              Integer settlementDays,
                              const Calendar& calendar,
                              Frequency frequency,
                              BusinessDayConvention paymentConvention,
                              DateGeneration::Rule rule,
                              const DayCounter& dayCounter,
                              Real recoveryRate,
                              const Handle<YieldTermStructure>& discountCurve,
                              bool settlesAccrual,
                              bool paysAtDefaultTime,
                              const Date& startDate,
                              const DayCounter& lastPeriodDayCounter,
                              bool rebatesAccrual,
                              CreditDefaultSwap::PricingModel model)
    : CdsHelper(runningSpread, tenor, settlementDays, calendar,
                frequency, paymentConvention, rule, dayCounter,
                recoveryRate, discountCurve, settlesAccrual, paysAtDefaultTime,
                startDate, lastPeriodDayCounter, rebatesAccrual, model) {}

    SpreadCdsHelper::SpreadCdsHelper(
                              const std::variant<Rate, Handle<Quote>>& runningSpread,
                              const Period& tenor,
                              Integer settlementDays,
                              const Calendar& calendar,
                              Frequency frequency,
                              BusinessDayConvention paymentConvention,
                              DateGeneration::Rule rule,
                              const DayCounter& dayCounter,
                              Real recoveryRate,
                              const Handle<YieldTermStructure>& discountCurve,
                              CreditDefaultSwap::PricingModel model,
                              bool settlesAccrual,
                              CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                              const Date& startDate,
                              const DayCounter& lastPeriodDayCounter,
                              bool rebatesAccrual)
    : CdsHelper(runningSpread, tenor, settlementDays, calendar,
                frequency, paymentConvention, rule, dayCounter,
                recoveryRate, discountCurve, settlesAccrual, protectionPaymentTime,
                startDate, lastPeriodDayCounter, rebatesAccrual, model) {}
    
    Real SpreadCdsHelper::impliedQuote() const {
        swap_->deepUpdate();
        return swap_->fairSpreadClean();
    }

    void SpreadCdsHelper::resetEngine() {
        swap_ = ext::make_shared<CreditDefaultSwap>(
            Protection::Buyer, 100.0, 0.01, schedule_, paymentConvention_,
            dayCounter_, settlesAccrual_, protectionPaymentTime_, protectionStart_,
            ext::shared_ptr<Claim>(), lastPeriodDC_, rebatesAccrual_, evaluationDate_);

        switch (model_) {
          case CreditDefaultSwap::ISDA:
            swap_->setPricingEngine(ext::make_shared<IsdaCdsEngine>(
                probability_, recoveryRate_, discountCurve_, false,
                IsdaCdsEngine::Taylor, IsdaCdsEngine::HalfDayBias,
                IsdaCdsEngine::Piecewise));
            break;
          case CreditDefaultSwap::Midpoint:
            swap_->setPricingEngine(ext::make_shared<MidPointCdsEngine>(
                probability_, recoveryRate_, discountCurve_));
            break;
          default:
            QL_FAIL("unknown CDS pricing model: " << model_);
        }
    }

    // deprecated 
    UpfrontCdsHelper::UpfrontCdsHelper(
                              const std::variant<Rate, Handle<Quote>>& upfront,
                              Rate runningSpread,
                              const Period& tenor,
                              Integer settlementDays,
                              const Calendar& calendar,
                              Frequency frequency,
                              BusinessDayConvention paymentConvention,
                              DateGeneration::Rule rule,
                              const DayCounter& dayCounter,
                              Real recoveryRate,
                              const Handle<YieldTermStructure>& discountCurve,
                              Natural upfrontSettlementDays,
                              bool settlesAccrual,
                              bool paysAtDefaultTime,
                              const Date& startDate,
                              const DayCounter& lastPeriodDayCounter,
                              bool rebatesAccrual,
                              CreditDefaultSwap::PricingModel model)
    : CdsHelper(upfront, tenor, settlementDays, calendar,
                frequency, paymentConvention, rule, dayCounter,
                recoveryRate, discountCurve, settlesAccrual, paysAtDefaultTime,
                startDate, lastPeriodDayCounter, rebatesAccrual, model),
      upfrontSettlementDays_(upfrontSettlementDays),
      upfrontDate_(upfrontDate()),
      runningSpread_(runningSpread) {}

    UpfrontCdsHelper::UpfrontCdsHelper(
                              const std::variant<Rate, Handle<Quote>>& upfront,
                              Rate runningSpread,
                              const Period& tenor,
                              Integer settlementDays,
                              const Calendar& calendar,
                              Frequency frequency,
                              BusinessDayConvention paymentConvention,
                              DateGeneration::Rule rule,
                              const DayCounter& dayCounter,
                              Real recoveryRate,
                              const Handle<YieldTermStructure>& discountCurve,
                              CreditDefaultSwap::PricingModel model,
                              Natural upfrontSettlementDays,
                              bool settlesAccrual,
                              CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                              const Date& startDate,
                              const DayCounter& lastPeriodDayCounter,
                              bool rebatesAccrual)
    : CdsHelper(upfront, tenor, settlementDays, calendar,
                frequency, paymentConvention, rule, dayCounter,
                recoveryRate, discountCurve, settlesAccrual, protectionPaymentTime,
                startDate, lastPeriodDayCounter, rebatesAccrual, model),
      upfrontSettlementDays_(upfrontSettlementDays), runningSpread_(runningSpread) {
        UpfrontCdsHelper::initializeDates();
    }

    UpfrontCdsHelper::UpfrontCdsHelper(
        const std::variant<Rate, Handle<Quote>>& upfront,
        Rate runningSpread,
        Schedule schedule,
        DayCounter dayCounter,
        Real recoveryRate,
        const Handle<YieldTermStructure>& discountCurve,
        CreditDefaultSwap::PricingModel model,
        Integer upfrontSettlementDays,
        bool settlesAccrual,
        CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
        const DayCounter& lastPeriodDayCounter,
        bool rebatesAccrual)
    : CdsHelper(upfront,
                schedule,
                dayCounter,
                recoveryRate,
                discountCurve,
                model,
                settlesAccrual,
                protectionPaymentTime,
                lastPeriodDayCounter,
                rebatesAccrual),
      upfrontSettlementDays_(3), runningSpread_(runningSpread) {
        upfrontDate_ = schedule_.calendar().advance(evaluationDate_, upfrontSettlementDays_, Days, paymentConvention_);
    }

    Date UpfrontCdsHelper::upfrontDate() {
        return calendar_.advance(evaluationDate_, upfrontSettlementDays_, Days, paymentConvention_);
    }

    void UpfrontCdsHelper::resetEngine() {
        swap_ = ext::make_shared<CreditDefaultSwap>(
            Protection::Buyer, 100.0, 0.01, runningSpread_, schedule_,
            schedule_.businessDayConvention(), dayCounter_, settlesAccrual_,
            protectionPaymentTime_, protectionStart_, upfrontDate_,
            ext::shared_ptr<Claim>(), lastPeriodDC_, rebatesAccrual_,
            evaluationDate_);
        
        switch (model_) {
          case CreditDefaultSwap::ISDA:
            swap_->setPricingEngine(ext::make_shared<IsdaCdsEngine>(
                probability_, recoveryRate_, discountCurve_, false,
                IsdaCdsEngine::Taylor, IsdaCdsEngine::HalfDayBias,
                IsdaCdsEngine::Piecewise));
            break;
          case CreditDefaultSwap::Midpoint:
            swap_->setPricingEngine(ext::make_shared<MidPointCdsEngine>(
                probability_, recoveryRate_, discountCurve_, true));
            break;
          default:
            QL_FAIL("unknown CDS pricing model: " << model_);
        }
    }

    Real UpfrontCdsHelper::impliedQuote() const {
        SavedSettings backup;
        Settings::instance().includeTodaysCashFlows() = true;
        swap_->deepUpdate();
        return swap_->fairUpfront();
    }

}
