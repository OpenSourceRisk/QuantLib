/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008, 2009 Jose Aparicio
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2008 StatPro Italia srl

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

#include <ql/instruments/creditdefaultswap.hpp>
#include <ql/instruments/claim.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>
#include <ql/pricingengines/credit/isdacdsengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/schedule.hpp>
#include <ql/optional.hpp>
#include <utility>

namespace QuantLib {

    CreditDefaultSwap::CreditDefaultSwap(Protection::Side side,
                                         Real notional,
                                         Rate spread,
                                         const Schedule& schedule,
                                         BusinessDayConvention convention,
                                         const DayCounter& dayCounter,
                                         bool settlesAccrual,
                                         bool paysAtDefaultTime,
                                         const Date& protectionStart,
                                         const ext::shared_ptr<Claim>& claim,
                                         const DayCounter& lastPeriodDayCounter,
                                         bool rebatesAccrual,
                                         const Date& tradeDate,
                                         Natural cashSettlementDays)
    : side_(side), notional_(notional), upfront_(ext::nullopt), runningSpread_(spread),
      schedule_(schedule), paymentConvention_(convention), settlesAccrual_(settlesAccrual),
      paysAtDefaultTime_(paysAtDefaultTime),
      protectionPaymentTime_(settlesAccrual ? atDefault : atPeriodEnd), claim_(std::move(claim)),
      protectionStart_(protectionStart == Date() ? schedule[0] : protectionStart),
      tradeDate_(tradeDate), cashSettlementDays_(cashSettlementDays),
      rebatesAccrual_(rebatesAccrual), dayCounter_(dayCounter),
      lastPeriodDayCounter_(lastPeriodDayCounter){

        init();
    }

    CreditDefaultSwap::CreditDefaultSwap(Protection::Side side,
                                         Real notional,
                                         Rate upfront,
                                         Rate runningSpread,
                                         const Schedule& schedule,
                                         BusinessDayConvention convention,
                                         const DayCounter& dayCounter,
                                         bool settlesAccrual,
                                         bool paysAtDefaultTime,
                                         const Date& protectionStart,
                                         const Date& upfrontDate,
                                         const ext::shared_ptr<Claim>& claim,
                                         const DayCounter& lastPeriodDayCounter,
                                         bool rebatesAccrual,
                                         const Date& tradeDate,
                                         Natural cashSettlementDays)
    : side_(side), notional_(notional), upfront_(upfront), runningSpread_(runningSpread),
      schedule_(schedule), paymentConvention_(convention), settlesAccrual_(settlesAccrual),
      paysAtDefaultTime_(paysAtDefaultTime),
      protectionPaymentTime_(settlesAccrual ? atDefault : atPeriodEnd), claim_(claim),
      protectionStart_(protectionStart == Date() ? schedule[0] : protectionStart),
      tradeDate_(tradeDate), cashSettlementDays_(cashSettlementDays),
      rebatesAccrual_(rebatesAccrual), dayCounter_(dayCounter),
      lastPeriodDayCounter_(lastPeriodDayCounter){

        init(upfrontDate);
    }

    CreditDefaultSwap::CreditDefaultSwap(Protection::Side side,
                                         Real notional,
                                         Rate spread,
                                         const Schedule& schedule,
                                         BusinessDayConvention convention,
                                         const DayCounter& dayCounter,
                                         bool settlesAccrual,
                                         ProtectionPaymentTime protectionPaymentTime,
                                         const Date& protectionStart,
                                         const ext::shared_ptr<Claim>& claim,
                                         const DayCounter& lastPeriodDayCounter,
                                         bool rebatesAccrual,
                                         const Date& tradeDate,
                                         Natural cashSettlementDays)
    : side_(side), notional_(notional), upfront_(ext::nullopt), runningSpread_(spread),
      schedule_(schedule), paymentConvention_(convention), settlesAccrual_(settlesAccrual),
      paysAtDefaultTime_(protectionPaymentTime == atDefault ? true : false),
      protectionPaymentTime_(protectionPaymentTime), claim_(claim),
      protectionStart_(protectionStart == Date() ? schedule[0] : protectionStart),
      tradeDate_(tradeDate), cashSettlementDays_(cashSettlementDays),
      rebatesAccrual_(rebatesAccrual),dayCounter_(dayCounter),
      lastPeriodDayCounter_(lastPeriodDayCounter) {

        init();
    }

    CreditDefaultSwap::CreditDefaultSwap(Protection::Side side,
                                         Real notional,
                                         Rate upfront,
                                         Rate runningSpread,
                                         const Schedule& schedule,
                                         BusinessDayConvention convention,
                                         const DayCounter& dayCounter,
                                         bool settlesAccrual,
                                         ProtectionPaymentTime protectionPaymentTime,
                                         const Date& protectionStart,
                                         const Date& upfrontDate,
                                         const ext::shared_ptr<Claim>& claim,
                                         const DayCounter& lastPeriodDayCounter,
                                         bool rebatesAccrual,
                                         const Date& tradeDate,
                                         Natural cashSettlementDays)
    : side_(side), notional_(notional), upfront_(upfront), runningSpread_(runningSpread),
      schedule_(schedule), paymentConvention_(convention), settlesAccrual_(settlesAccrual),
      paysAtDefaultTime_(protectionPaymentTime == atDefault ? true : false),
      protectionPaymentTime_(protectionPaymentTime), claim_(claim),
      protectionStart_(protectionStart == Date() ? schedule[0] : protectionStart),
      tradeDate_(tradeDate), cashSettlementDays_(cashSettlementDays),
      rebatesAccrual_(rebatesAccrual),dayCounter_(dayCounter),
      lastPeriodDayCounter_(lastPeriodDayCounter) {

        init(upfrontDate);
    }

    CreditDefaultSwap::CreditDefaultSwap(Protection::Side side,
                                         Real notional,
                                         const Leg& amortized_leg,
                                         Rate spread,
                                         const Schedule& schedule,
                                         BusinessDayConvention convention,
                                         const DayCounter& dayCounter,
                                         bool settlesAccrual,
                                         ProtectionPaymentTime protectionPaymentTime,
                                         const Date& protectionStart,
                                         const ext::shared_ptr<Claim>& claim,
                                         const DayCounter& lastPeriodDayCounter,
                                         bool rebatesAccrual,
                                         const Date& tradeDate,
                                         Natural cashSettlementDays)
    : side_(side), notional_(notional), upfront_(ext::nullopt), runningSpread_(spread),
      schedule_(schedule), paymentConvention_(convention), settlesAccrual_(settlesAccrual),
      paysAtDefaultTime_(protectionPaymentTime == atDefault ? true : false),
      protectionPaymentTime_(protectionPaymentTime), claim_(claim), leg_(amortized_leg),
      protectionStart_(protectionStart == Date() ? schedule[0] : protectionStart),
      tradeDate_(tradeDate), cashSettlementDays_(cashSettlementDays),
      rebatesAccrual_(rebatesAccrual), dayCounter_(dayCounter),
      lastPeriodDayCounter_(lastPeriodDayCounter) {

        init();
    }

    CreditDefaultSwap::CreditDefaultSwap(Protection::Side side, Real notional, const Leg& amortized_leg, Rate upfront,
                                         Rate runningSpread, const Schedule& schedule, BusinessDayConvention convention,
                                         const DayCounter& dayCounter, bool settlesAccrual,
                                         ProtectionPaymentTime protectionPaymentTime, const Date& protectionStart,
                                         const Date& upfrontDate, const ext::shared_ptr<Claim>& claim,
                                         const DayCounter& lastPeriodDayCounter, bool rebatesAccrual,
                                         const Date& tradeDate, Natural cashSettlementDays)
        : side_(side), notional_(notional), upfront_(upfront), runningSpread_(runningSpread), schedule_(schedule),
          paymentConvention_(convention), settlesAccrual_(settlesAccrual),
          paysAtDefaultTime_(protectionPaymentTime == atDefault ? true : false),
          protectionPaymentTime_(protectionPaymentTime), claim_(claim), leg_(amortized_leg),
          protectionStart_(protectionStart == Date() ? schedule[0] : protectionStart), tradeDate_(tradeDate),
          cashSettlementDays_(cashSettlementDays), rebatesAccrual_(rebatesAccrual),
          dayCounter_(dayCounter), lastPeriodDayCounter_(lastPeriodDayCounter){

        init(upfrontDate);
    }

    void CreditDefaultSwap::init(const Date& upfrontDate) {

        QL_REQUIRE(!schedule_.empty(), "CreditDefaultSwap needs a non-empty schedule.");

        postBigBang_ = false;
        if (schedule_.hasRule()) {
            DateGeneration::Rule rule = schedule_.rule();
            postBigBang_ = rule == DateGeneration::CDS || rule == DateGeneration::CDS2015 || rule == DateGeneration::EighthBusinessDay;
        }
        
        if (!postBigBang_) {
            QL_REQUIRE(protectionStart_ <= schedule_[0], "CreditDefaultSwap: protection can not start after accrual");
        }
        

        // For CDS, the standard day counter is Actual/360 and the final period coupon accrual includes the maturity date.
        // If the main day counter is Act/360 and no lastPeriodDayCounter_ is given, default to Act/360 including last.
        effectiveLastPeriodDayCounter_ =
            lastPeriodDayCounter_.empty() ?
                (dayCounter_ == Actual360() ? Actual360(true) : dayCounter_) :
                lastPeriodDayCounter_;

        // If the leg_ has not already been populated via amortised leg ctor, populate it.
        if (leg_.empty()) {
            leg_ = FixedRateLeg(schedule_)
                .withNotionals(notional_)
                .withCouponRates(runningSpread_, dayCounter_)
                .withPaymentAdjustment(paymentConvention_)
                .withLastPeriodDayCounter(effectiveLastPeriodDayCounter_);
        }
        
        // Deduce the trade date if not given.
        if (tradeDate_ == Date()) {
            if (postBigBang_) {
                tradeDate_ = protectionStart_;
            } else {
                tradeDate_ = protectionStart_ - 1;
            }
        }
        
        // Deduce the cash settlement date if not given.
        effectiveUpfrontDate_ = upfrontDate;
        if (effectiveUpfrontDate_ == Date()) {
            effectiveUpfrontDate_ = schedule_.calendar().advance(tradeDate_, cashSettlementDays_,
                                                                 Days, paymentConvention_);
        }
        QL_REQUIRE(effectiveUpfrontDate_ >= protectionStart_,
                   "The cash settlement date must not be before the protection start date.");

        // Create the upfront payment, if one is provided.
        Real upfrontAmount = 0.0;
        if (upfront_)
            upfrontAmount = *upfront_ * notional_;
        upfrontPayment_ = ext::make_shared<SimpleCashFlow>(upfrontAmount, effectiveUpfrontDate_);

        // Set the maturity date.
        maturity_ = schedule_.dates().back();
        
        if (!claim_)
            claim_ = ext::make_shared<FaceValueClaim>();
        registerWith(claim_);
    }

    void CreditDefaultSwap::performCalculations() const {
        // Deal with the accrual rebate. We use the standard conventions for accrual calculation introduced with the
        // CDS Big Bang in 2009
        auto calculateAccrualDate =[](const Date& d, const Leg& leg, const DayCounter& dayCounter, const DayCounter& lastPeriodDayCounter)->Date {
            Date nextPaymentDate = CashFlows::nextCashFlowDate(leg, true, d);
            Date accrualDate = d + (dayCounter.includeLastDay() ? 0 : 1);
            if (nextPaymentDate == leg.back()->date()) {
                accrualDate = d + (lastPeriodDayCounter.includeLastDay() ? 0 : 1);
            }
            return accrualDate;
        };
        if (rebatesAccrual_ && postBigBang_) {
            Date accrualDate =
                calculateAccrualDate(tradeDate_, leg_, dayCounter_, effectiveLastPeriodDayCounter_);
            accrualRebate_ = ext::make_shared<SimpleCashFlow>(
                CashFlows::accruedAmount(leg_, tradeDate_ + 1 == leg_.back()->date(), accrualDate),
                effectiveUpfrontDate_);
            Date current = std::max((Date)Settings::instance().evaluationDate(), tradeDate_);
            Date currentAccrualDate = calculateAccrualDate(current, leg_, dayCounter_, effectiveLastPeriodDayCounter_);
            accrualRebateCurrent_ = ext::make_shared<SimpleCashFlow>(
                CashFlows::accruedAmount(leg_, true, currentAccrualDate),
                schedule_.calendar().advance(current, cashSettlementDays_, Days,
                                             paymentConvention_));
        }
        Instrument::performCalculations();
    }

    Protection::Side CreditDefaultSwap::side() const {
        return side_;
    }

    Real CreditDefaultSwap::notional() const {
        return notional_;
    }

    Rate CreditDefaultSwap::runningSpread() const {
        return runningSpread_;
    }

    ext::optional<Rate> CreditDefaultSwap::upfront() const {
        return upfront_;
    }

    bool CreditDefaultSwap::settlesAccrual() const {
        return settlesAccrual_;
    }

    bool CreditDefaultSwap::paysAtDefaultTime() const {
        return paysAtDefaultTime_;
    }

    CreditDefaultSwap::ProtectionPaymentTime CreditDefaultSwap::protectionPaymentTime() const {
        return protectionPaymentTime_;
    }

    const Leg& CreditDefaultSwap::coupons() const {
        return leg_;
    }


    bool CreditDefaultSwap::isExpired() const {
        for (Leg::const_reverse_iterator i = leg_.rbegin();
                                         i != leg_.rend(); ++i) {
            if (!(*i)->hasOccurred())
                return false;
        }
        return true;
    }

    void CreditDefaultSwap::setupExpired() const {
        Instrument::setupExpired();
        fairSpread_ = fairSpreadClean_ = fairSpreadDirty_ = fairUpfront_ = 0.0;
        couponLegBPS_ = upfrontBPS_ = 0.0;
        couponLegNPV_ = defaultLegNPV_ = upfrontNPV_ = 0.0;
    }

    void CreditDefaultSwap::setupArguments(
                                       PricingEngine::arguments* args) const {
        CreditDefaultSwap::arguments* arguments =
            dynamic_cast<CreditDefaultSwap::arguments*>(args);
        QL_REQUIRE(arguments != 0, "wrong argument type");

        arguments->side = side_;
        arguments->notional = notional_;
        arguments->leg = leg_;
        arguments->upfrontPayment = upfrontPayment_;
        arguments->accrualRebate = accrualRebate_;
        arguments->accrualRebateCurrent = accrualRebateCurrent_;
        arguments->settlesAccrual = settlesAccrual_;
        arguments->paysAtDefaultTime = paysAtDefaultTime_;
        arguments->protectionPaymentTime = protectionPaymentTime_;
        arguments->claim = claim_;
        arguments->upfront = upfront_;
        arguments->spread = runningSpread_;
        arguments->protectionStart = protectionStart_;
        arguments->maturity = maturity_;
    }


    void CreditDefaultSwap::fetchResults(
                                      const PricingEngine::results* r) const {
        Instrument::fetchResults(r);

        const CreditDefaultSwap::results* results =
            dynamic_cast<const CreditDefaultSwap::results*>(r);
        QL_REQUIRE(results != 0, "wrong result type");

        fairSpread_ = results->fairSpread;
        fairSpreadDirty_ = results->fairSpreadDirty;
        fairSpreadClean_ = results->fairSpreadClean;
        fairUpfront_ = results->fairUpfront;
        couponLegBPS_ = results->couponLegBPS;
        couponLegNPV_ = results->couponLegNPV;
        defaultLegNPV_ = results->defaultLegNPV;
        upfrontNPV_ = results->upfrontNPV;
        upfrontBPS_ = results->upfrontBPS;
        accrualRebateNPV_ = results->accrualRebateNPV;
        accrualRebateNPVCurrent_ = results->accrualRebateNPVCurrent;
    }

    Rate CreditDefaultSwap::fairUpfront() const {
        calculate();
        QL_REQUIRE(fairUpfront_ != Null<Rate>(),
                   "fair upfront not available");
        return fairUpfront_;
    }

    Rate CreditDefaultSwap::fairSpreadDirty() const {
        calculate();
        QL_REQUIRE(fairSpreadDirty_ != Null<Rate>(), "fair spread (dirtry) not available");
        return fairSpreadDirty_;
    }

    Rate CreditDefaultSwap::fairSpreadClean() const {
        calculate();
        QL_REQUIRE(fairSpreadClean_ != Null<Rate>(), "fair spread (clean) not available");
        return fairSpreadClean_;
    }

    Rate CreditDefaultSwap::fairSpread() const {
        calculate();
        QL_REQUIRE(fairSpread_ != Null<Rate>(), "fair spread not available");
        return fairSpread_;
    }

    Real CreditDefaultSwap::couponLegBPS() const {
        calculate();
        QL_REQUIRE(couponLegBPS_ != Null<Rate>(),
                   "coupon-leg BPS not available");
        return couponLegBPS_;
    }

    Real CreditDefaultSwap::couponLegNPV() const {
        calculate();
        QL_REQUIRE(couponLegNPV_ != Null<Rate>(),
                   "coupon-leg NPV not available");
        return couponLegNPV_;
    }

    Real CreditDefaultSwap::defaultLegNPV() const {
        calculate();
        QL_REQUIRE(defaultLegNPV_ != Null<Rate>(),
                   "default-leg NPV not available");
        return defaultLegNPV_;
    }

    Real CreditDefaultSwap::upfrontNPV() const {
        calculate();
        QL_REQUIRE(upfrontNPV_ != Null<Real>(),
                   "upfront NPV not available");
        return upfrontNPV_;
    }

    Real CreditDefaultSwap::upfrontBPS() const {
        calculate();
        QL_REQUIRE(upfrontBPS_ != Null<Real>(),
                   "upfront BPS not available");
        return upfrontBPS_;
    }

    Real CreditDefaultSwap::accrualRebateNPV() const {
        calculate();
        QL_REQUIRE(accrualRebateNPV_ != Null<Real>(),
                   "accrual Rebate NPV not available");
        return accrualRebateNPV_;
    }

    namespace {

        class ObjectiveFunction {
          public:
            ObjectiveFunction(Real target,
                              SimpleQuote& quote,
                              PricingEngine& engine,
                              const CreditDefaultSwap::results* results)
            : target_(target), quote_(quote),
              engine_(engine), results_(results) {}

            Real operator()(Real guess) const {
                quote_.setValue(guess);
                engine_.calculate();
                return results_->value - target_;
            }
          private:
            Real target_;
            SimpleQuote& quote_;
            PricingEngine& engine_;
            const CreditDefaultSwap::results* results_;
        };

    }

    ext::shared_ptr<PricingEngine> CreditDefaultSwap::buildPricingEngine(
                               const Handle<DefaultProbabilityTermStructure>& p,
                               Real r, const Handle<YieldTermStructure>& d,
                               PricingModel model) const {
        ext::shared_ptr<PricingEngine> engine;
        switch (model) {
          case Midpoint:
            engine = ext::make_shared<MidPointCdsEngine>(p, r, d);
            break;
          case ISDA:
            engine = ext::make_shared<IsdaCdsEngine>(
                p, r, d,
                ext::nullopt,
                IsdaCdsEngine::Taylor,
                IsdaCdsEngine::HalfDayBias,
                IsdaCdsEngine::Piecewise);
            break;
          default:
            QL_FAIL("unknown CDS pricing model: " << model);
        }
        return engine;
    }
        
    Rate CreditDefaultSwap::impliedHazardRate(
                               Real targetNPV,
                               const Handle<YieldTermStructure>& discountCurve,
                               const DayCounter& dayCounter,
                               Real recoveryRate,
                               Real accuracy,
                               PricingModel model) const {

        calculate();
        ext::shared_ptr<SimpleQuote> flatRate = ext::make_shared<SimpleQuote>(0.0);

        Handle<DefaultProbabilityTermStructure> probability =
            Handle<DefaultProbabilityTermStructure>(
                ext::make_shared<FlatHazardRate>(0, WeekendsOnly(),
                                                   Handle<Quote>(flatRate), dayCounter));

        ext::shared_ptr<PricingEngine> engine = buildPricingEngine(probability, recoveryRate, discountCurve, model);

        setupArguments(engine->getArguments());
        const CreditDefaultSwap::results* results =
            dynamic_cast<const CreditDefaultSwap::results*>(
                engine->getResults());

        ObjectiveFunction f(targetNPV, *flatRate, *engine, results);
        //very close guess if targetNPV = 0.
        Rate guess = runningSpread_ / (1 - recoveryRate) * 365./360.;
        Real step = 0.1 * guess;
        return Brent().solve(f, accuracy, guess, step);
    }

    Rate CreditDefaultSwap::conventionalSpread(
                              Real conventionalRecovery,
                              const Handle<YieldTermStructure>& discountCurve,
                              const DayCounter& dayCounter,
                              PricingModel model) const {

        calculate();
        ext::shared_ptr<SimpleQuote> flatRate = ext::make_shared<SimpleQuote>(0.0);

        Handle<DefaultProbabilityTermStructure> probability =
            Handle<DefaultProbabilityTermStructure>(
                ext::make_shared<FlatHazardRate>(0, WeekendsOnly(),
                                                   Handle<Quote>(flatRate), dayCounter));

        ext::shared_ptr<PricingEngine> engine = buildPricingEngine(probability, conventionalRecovery, discountCurve, model);

        setupArguments(engine->getArguments());
        const CreditDefaultSwap::results* results =
            dynamic_cast<const CreditDefaultSwap::results*>(
                engine->getResults());

        ObjectiveFunction f(0., *flatRate, *engine, results);
        Rate guess = runningSpread_ / (1 - conventionalRecovery) * 365./360.;
        Real step = guess * 0.1;

        Brent().solve(f, 1e-9, guess, step);
        return results->fairSpread;
    }


    const Date& CreditDefaultSwap::protectionStartDate() const {
        return protectionStart_;
    }

    const Date& CreditDefaultSwap::protectionEndDate() const {
        return ext::dynamic_pointer_cast<Coupon>(leg_.back())
            ->accrualEndDate();
    }

    const ext::shared_ptr<SimpleCashFlow>& CreditDefaultSwap::upfrontPayment() const {
        return upfrontPayment_;
    }

    const ext::shared_ptr<SimpleCashFlow>& CreditDefaultSwap::accrualRebate() const {
        calculate();
        return accrualRebate_;
    }

    const ext::shared_ptr<SimpleCashFlow>& CreditDefaultSwap::accrualRebateCurrent() const {
        calculate();
        return accrualRebateCurrent_;
    }

    const Date& CreditDefaultSwap::tradeDate() const {
        return tradeDate_;
    }

    Natural CreditDefaultSwap::cashSettlementDays() const {
        return cashSettlementDays_;
    }

    CreditDefaultSwap::arguments::arguments()
    : side(Protection::Side(-1)), notional(Null<Real>()),
      spread(Null<Rate>()) {}

    void CreditDefaultSwap::arguments::validate() const {
        QL_REQUIRE(side != Protection::Side(-1), "side not set");
        QL_REQUIRE(notional != Null<Real>(), "notional not set");
        QL_REQUIRE(notional != 0.0, "null notional set");
        QL_REQUIRE(spread != Null<Rate>(), "spread not set");
        QL_REQUIRE(!leg.empty(), "coupons not set");
        QL_REQUIRE(upfrontPayment, "upfront payment not set");
        QL_REQUIRE(claim, "claim not set");
        QL_REQUIRE(protectionStart != Date(), "protection start date not set");
        QL_REQUIRE(maturity != Date(), "maturity date not set");
    }

    void CreditDefaultSwap::results::reset() {
        Instrument::results::reset();
        fairSpread = Null<Rate>();
        fairSpreadClean = Null<Rate>();
        fairSpreadDirty = Null<Rate>();
        fairUpfront = Null<Rate>();
        couponLegBPS = Null<Real>();
        couponLegNPV = Null<Real>();
        defaultLegNPV = Null<Real>();
        upfrontBPS = Null<Real>();
        upfrontNPV = Null<Real>();
        accrualRebateNPV = Null<Real>();
        accrualRebateNPVCurrent = Null<Real>();
    }

    std::ostream &operator<<(std::ostream &out,
                             const CreditDefaultSwap::ProtectionPaymentTime &t)  {
        if (t == CreditDefaultSwap::ProtectionPaymentTime::atDefault)
            out << "at default";
        else if (t == CreditDefaultSwap::ProtectionPaymentTime::atPeriodEnd)
            out << "at period end";
        else if (t == CreditDefaultSwap::ProtectionPaymentTime::atMaturity)
            out << "at maturity";
        else
            out << "unknown protection payment time";

        return out;
    }

}
