/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2002, 2003 Ferdinando Ametrano
 Copyright (C) 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2003 Neil Firth
 Copyright (C) 2007 StatPro Italia srl

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

#include <ql/exercise.hpp>
#include <ql/pricingengines/barrier/analyticbarrierengine.hpp>
#include <utility>

namespace QuantLib {

    AnalyticBarrierEngine::AnalyticBarrierEngine(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process)) {
        registerWith(process_);
    }

    void AnalyticBarrierEngine::calculate() const {

        ext::shared_ptr<PlainVanillaPayoff> payoff =
            ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");
        QL_REQUIRE(payoff->strike()>0.0,
                   "strike must be positive");

        QL_REQUIRE(arguments_.exercise->type() == Exercise::European,
                   "only european style option are supported");

        Real strike = payoff->strike();
        Real spot = process_->x0();
        QL_REQUIRE(spot > 0.0, "negative or null underlying given");
        QL_REQUIRE(!triggered(spot), "barrier touched");

        Barrier::Type barrierType = arguments_.barrierType;

        results_.additionalResults["volatility"] = volatility();
        results_.additionalResults["spot"] = spot;
        results_.additionalResults["dividendDiscount"] = dividendDiscount();
        results_.additionalResults["riskFreeDiscount"] = riskFreeDiscount();
        results_.additionalResults["strike"] = strike;
        results_.additionalResults["barrier"] = barrier();
        results_.additionalResults["rebate"] = rebate();
        results_.additionalResults["timeToExpiry"] = process_->time(arguments_.exercise->lastDate());

        const Date exerciseDate = arguments_.exercise->lastDate();
        const Handle<YieldTermStructure>& riskFreeCurve = process_->riskFreeRate();
        const Handle<YieldTermStructure>& dividendCurve = process_->dividendYield();
        Time tteRiskFree = riskFreeCurve->dayCounter().yearFraction(riskFreeCurve->referenceDate(), exerciseDate);
        Time tteDividend = dividendCurve->dayCounter().yearFraction(dividendCurve->referenceDate(), exerciseDate);
        if(tteRiskFree != tteDividend){
            results_.additionalResults["timeToExpiryRiskFree"] = tteRiskFree;
            results_.additionalResults["timeToExpiryDividend"] = tteDividend;
        }

        switch (payoff->optionType()) {
          case Option::Call:
            switch (barrierType) {
              case Barrier::DownIn:
                if (strike >= barrier())
                    results_.value = C(1,1) + E(1);
                else
                    results_.value = A(1) - B(1) + D(1,1) + E(1);
                results_.additionalResults["BarrierOptionType"] = std::string("CallDownAndIn");
                break;
              case Barrier::UpIn:
                if (strike >= barrier())
                    results_.value = A(1) + E(-1);
                else
                    results_.value = B(1) - C(-1,1) + D(-1,1) + E(-1);
                results_.additionalResults["BarrierOptionType"] = std::string("CallUpAndIn");
                break;
              case Barrier::DownOut:
                if (strike >= barrier())
                    results_.value = A(1) - C(1,1) + F(1);
                else
                    results_.value = B(1) - D(1,1) + F(1);
                results_.additionalResults["BarrierOptionType"] = std::string("CallDownAndOut");
                break;
              case Barrier::UpOut:
                if (strike >= barrier())
                    results_.value = F(-1);
                else
                    results_.value = A(1) - B(1) + C(-1,1) - D(-1,1) + F(-1);
                results_.additionalResults["BarrierOptionType"] = std::string("CallUpAndOut");
                break;
            }
            break;
          case Option::Put:
            switch (barrierType) {
              case Barrier::DownIn:
                if (strike >= barrier())
                    results_.value = B(-1) - C(1,-1) + D(1,-1) + E(1);
                else
                    results_.value = A(-1) + E(1);
                results_.additionalResults["BarrierOptionType"] = std::string("PutDownAndIn");
                break;
              case Barrier::UpIn:
                if (strike >= barrier())
                    results_.value = A(-1) - B(-1) + D(-1,-1) + E(-1);
                else
                    results_.value = C(-1,-1) + E(-1);
                results_.additionalResults["BarrierOptionType"] = std::string("PutUpAndIn");
                break;
              case Barrier::DownOut:
                if (strike >= barrier())
                    results_.value = A(-1) - B(-1) + C(1,-1) - D(1,-1) + F(1);
                else
                    results_.value = F(1);
                results_.additionalResults["BarrierOptionType"] = std::string("PutDownAndOut");
                break;
              case Barrier::UpOut:
                if (strike >= barrier())
                    results_.value = B(-1) - D(-1,-1) + F(-1);
                else
                    results_.value = A(-1) - C(-1,-1) + F(-1);
                results_.additionalResults["BarrierOptionType"] = std::string("PutUpAndOut");
                break;
            }
            break;
          default:
            QL_FAIL("unknown type");
        }
    }


    Real AnalyticBarrierEngine::underlying() const {
        return process_->x0();
    }

    Real AnalyticBarrierEngine::strike() const {
        ext::shared_ptr<PlainVanillaPayoff> payoff =
            ext::dynamic_pointer_cast<PlainVanillaPayoff>(arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");
        return payoff->strike();
    }

    Volatility AnalyticBarrierEngine::volatility() const {
        return process_->blackVolatility()->blackVol(
                    arguments_.exercise->lastDate(),
                    strike());
    }

    Real AnalyticBarrierEngine::stdDeviation() const {
        return std::sqrt(process_->blackVolatility()->blackVariance(
                        arguments_.exercise->lastDate(),
                        strike()));
    }

    Real AnalyticBarrierEngine::barrier() const {
        return arguments_.barrier;
    }

    Real AnalyticBarrierEngine::rebate() const {
        return arguments_.rebate;
    }

    Rate AnalyticBarrierEngine::riskFreeRate() const {
        return process_->riskFreeRate()->zeroRate(
                    arguments_.exercise->lastDate(),
                    process_->riskFreeRate()->dayCounter(),
                    Continuous, NoFrequency);
    }

    DiscountFactor AnalyticBarrierEngine::riskFreeDiscount() const {
        return process_->riskFreeRate()->discount(
                    arguments_.exercise->lastDate());
    }

    Rate AnalyticBarrierEngine::dividendYield() const {
        return process_->dividendYield()->zeroRate(
                    arguments_.exercise->lastDate(),
                    process_->dividendYield()->dayCounter(),
                    Continuous, NoFrequency);
    }

    DiscountFactor AnalyticBarrierEngine::dividendDiscount() const {
        return process_->dividendYield()->discount(
                    arguments_.exercise->lastDate());
    }

    Rate AnalyticBarrierEngine::mu() const {
        Volatility vol = volatility();
        Real mu_ = (riskFreeRate() - dividendYield())/(vol * vol) - 0.5;
        results_.additionalResults["mu"] = mu_;
        return mu_;
    }

    Real AnalyticBarrierEngine::muSigma() const {
        return (1 + mu()) * stdDeviation();
    }

    Real AnalyticBarrierEngine::A(Real phi) const {
        Real x1 =
            std::log(underlying()/strike())/stdDeviation() + muSigma();
        Real N1 = f_(phi*x1);
        Real N2 = f_(phi*(x1-stdDeviation()));
        Real A_ = phi * (underlying() * dividendDiscount() * N1 - strike() * riskFreeDiscount() * N2);
        results_.additionalResults["x1_A"] = x1;
        results_.additionalResults["Haug_A"] = A_;
        results_.additionalResults["phi"] = phi;
        return A_;
    }

    Real AnalyticBarrierEngine::B(Real phi) const {
        Real x2 =
            std::log(underlying()/barrier())/stdDeviation() + muSigma();
        Real N1 = f_(phi*x2);
        Real N2 = f_(phi*(x2-stdDeviation()));
        Real B_ = phi*(underlying() * dividendDiscount() * N1
                      - strike() * riskFreeDiscount() * N2);
        results_.additionalResults["x2_B"] = x2;
        results_.additionalResults["Haug_B"] = B_;
        results_.additionalResults["phi"] = phi;
        return B_;
    }

    Real AnalyticBarrierEngine::C(Real eta, Real phi) const {
        Real HS = barrier()/underlying();
        Real powHS0 = std::pow(HS, 2 * mu());
        Real powHS1 = powHS0 * HS * HS;
        Real y1 = std::log(barrier()*HS/strike())/stdDeviation() + muSigma();
        Real N1 = f_(eta*y1);
        Real N2 = f_(eta*(y1-stdDeviation()));
        // when N1 or N2 are zero, the corresponding powHS might
        // be infinity, resulting in a NaN for their products.  The limit should be 0.
        Real C_ = phi*(underlying() * dividendDiscount() * (N1 == 0.0 ? Real(0.0) : Real(powHS1 * N1))
                      - strike() * riskFreeDiscount() * (N2 == 0.0 ? Real(0.0) : Real(powHS0 * N2)));
        results_.additionalResults["y1_C"] = y1;
        results_.additionalResults["Haug_C"] = C_;
        results_.additionalResults["eta"] = eta;
        results_.additionalResults["phi"] = phi;
        return C_;
    }

    Real AnalyticBarrierEngine::D(Real eta, Real phi) const {
        Real HS = barrier()/underlying();
        Real powHS0 = std::pow(HS, 2 * mu());
        Real powHS1 = powHS0 * HS * HS;
        Real y2 = std::log(barrier()/underlying())/stdDeviation() + muSigma();
        Real N1 = f_(eta*y2);
        Real N2 = f_(eta*(y2-stdDeviation()));
        // when N1 or N2 are zero, the corresponding powHS might
        // be infinity, resulting in a NaN for their products.  The limit should be 0.
        Real D_ = phi*(underlying() * dividendDiscount() * (N1 == 0.0 ? Real(0.0) : Real(powHS1 * N1))
                      - strike() * riskFreeDiscount() * (N2 == 0.0 ? Real(0.0) : Real(powHS0 * N2)));
        results_.additionalResults["y2_D"] = y2;
        results_.additionalResults["Haug_D"] = D_;
        results_.additionalResults["eta"] = eta;
        results_.additionalResults["phi"] = phi;
        return D_;
    }

    Real AnalyticBarrierEngine::E(Real eta) const {
        Real E_;
        if (rebate() > 0) {
            Real powHS0 = std::pow(barrier()/underlying(), 2 * mu());
            Real x2 =
                std::log(underlying()/barrier())/stdDeviation() + muSigma();
            Real y2 =
                std::log(barrier()/underlying())/stdDeviation() + muSigma();
            results_.additionalResults["x2_E"] = x2;
            results_.additionalResults["y2_E"] = y2;
            Real N1 = f_(eta*(x2 - stdDeviation()));
            Real N2 = f_(eta*(y2 - stdDeviation()));
            // when N2 is zero, powHS0 might be infinity, resulting in
            // a NaN for their product.  The limit should be 0.
            E_ = rebate() * riskFreeDiscount() * (N1 - (N2 == 0.0 ? Real(0.0) : Real(powHS0 * N2)));
        } else {
            E_ = 0.0;
        }
        results_.additionalResults["Haug_E"] = E_;
        results_.additionalResults["eta"] = eta;
        return E_;
    }

    Real AnalyticBarrierEngine::F(Real eta) const {
        Real F_;
        if (rebate() > 0) {
            Rate m = mu();
            Volatility vol = volatility();
            Real lambda = std::sqrt(m*m + 2.0*riskFreeRate()/(vol * vol));
            results_.additionalResults["lambda"] = lambda;
            Real HS = barrier()/underlying();
            Real powHSplus = std::pow(HS, m + lambda);
            Real powHSminus = std::pow(HS, m - lambda);

            Real sigmaSqrtT = stdDeviation();
            Real z = std::log(barrier()/underlying())/sigmaSqrtT
                + lambda * sigmaSqrtT;
            results_.additionalResults["z"] = z;

            Real N1 = f_(eta * z);
            Real N2 = f_(eta * (z - 2.0 * lambda * sigmaSqrtT));
            // when N1 or N2 are zero, the corresponding powHS might
            // be infinity, resulting in a NaN for their product.  The limit should be 0.
            F_ = rebate() * ((N1 == 0.0 ? Real(0.0) : Real(powHSplus * N1)) + (N2 == 0.0 ? Real(0.0) : Real(powHSminus * N2)));
        } else {
            F_ = 0.0;
        }
        results_.additionalResults["Haug_F"] = F_;
        results_.additionalResults["eta"] = eta;
        return F_;
    }

}
