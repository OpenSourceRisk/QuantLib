/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Johannes Goettker-Schnetmann
 Copyright (C) 2015 Klaus Spanderen

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

#include <ql/math/functional.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/processes/hestonprocess.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/math/integrals/gausslobattointegral.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/experimental/finitedifferences/bsmrndcalculator.hpp>
#include <ql/experimental/finitedifferences/hestonrndcalculator.hpp>

#if defined(__GNUC__) && (((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)) || (__GNUC__ > 4))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#include <boost/bind.hpp>
#if defined(__GNUC__) && (((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)) || (__GNUC__ > 4))
#pragma GCC diagnostic pop
#endif

#include <complex>

namespace QuantLib {

namespace {
        struct HestonParams {
            Real v0, kappa, theta, sigma, rho;
        };

        HestonParams getHestonParams(
            const boost::shared_ptr<HestonProcess>& process) {
            const HestonParams p = { process->v0(),    process->kappa(),
                                     process->theta(), process->sigma(),
                                     process->rho() };
            return p;
        }

        std::complex<Real> gamma(const HestonParams& p, Real p_x) {
            return std::complex<Real>(p.kappa, p.rho*p.sigma*p_x);
        }

        std::complex<Real> omega(const HestonParams& p, Real p_x) {
           const std::complex<Real> g = gamma(p, p_x);
           return sqrt(g*g
                       + p.sigma*p.sigma*std::complex<Real>(p_x*p_x, -p_x));
        }

        class CpxPv_Helper
            : public std::unary_function<Real, Real > {
          public:
            CpxPv_Helper(const HestonParams& p, Real x, Time t)
              : p_(p), t_(t), x_(x),
                c_inf_(min(Real(10.0), max(Real(0.0001),
                      sqrt(1.0-square<Real>()(p_.rho))/p_.sigma))
                      *(p_.v0 + p_.kappa*p_.theta*t))  {}

            Real operator()(Real x) const {
                return std::real(transformPhi(x));
            }

            Real p0(Real p_x) const {
                if (p_x < QL_EPSILON) {
                    return 0.0;
                }

                const Real u_x = max(QL_EPSILON, -log(p_x)/c_inf_);
                return std::real(phi(VALUE(u_x)) / VALUE(((p_x * c_inf_) * std::complex<Real>(Real(0.0), u_x))));
            }

          private:
            std::complex<Real> transformPhi(Real x) const {
                if (x < QL_EPSILON) {
                    return std::complex<Real>(0.0, 0.0);
                }

                const Real u_x = -log(x)/c_inf_;
                return phi(VALUE(u_x))/VALUE(x*c_inf_);
            }

            std::complex<double> phi(double p_x) const {
                const double sigma2 = VALUE(p_.sigma*p_.sigma);
                const std::complex<double> g = VALUE(gamma(p_, p_x));
                const std::complex<double> o = VALUE(omega(p_, p_x));
                const std::complex<double> gamma = (g-o)/(g+o);

                return double(2.0) *
                       exp(std::complex<double>(0.0, VALUE(p_x * x_)) -
                           VALUE(p_.v0) * std::complex<double>(p_x * p_x, -p_x) /
                               (g + o * (double(1.0) + exp(-o * VALUE(t_))) / (double(1.0) - exp(-o * VALUE(t_)))) +
                           VALUE(p_.kappa * p_.theta) / sigma2 *
                               ((g - o) * VALUE(t_) -
                                double(2.0) *
                                    log((double(1.0) - gamma * exp(-o * VALUE(t_))) / (double(1.0) - gamma))));
            }

            const HestonParams& p_;
            const Time t_;
            const Real x_, c_inf_;
        };
    }


    HestonRNDCalculator::HestonRNDCalculator(
        const boost::shared_ptr<HestonProcess>& hestonProcess,
        Real integrationEps, Size maxIntegrationIterations)
    : hestonProcess_(hestonProcess),
      x0_(log(hestonProcess_->s0()->value())),
      integrationEps_(integrationEps),
      maxIntegrationIterations_(maxIntegrationIterations) { }

    Real HestonRNDCalculator::x_t(Real x, Time t) const {
        const DiscountFactor dr = hestonProcess_->riskFreeRate()->discount(t);
        const DiscountFactor dq = hestonProcess_->dividendYield()->discount(t);

        return x - x0_ + log(dr/dq);
    }

    Real HestonRNDCalculator::pdf(Real x, Time t) const {
        return GaussLobattoIntegral(
            maxIntegrationIterations_, 0.1*integrationEps_)(
            CpxPv_Helper(getHestonParams(hestonProcess_), x_t(x, t), t),
            0.0, 1.0)/M_TWOPI;
    }

    Real HestonRNDCalculator::cdf(Real x, Time t) const {
        return GaussLobattoIntegral(
            maxIntegrationIterations_, 0.1*integrationEps_)(
            boost::bind(&CpxPv_Helper::p0,
                CpxPv_Helper(getHestonParams(hestonProcess_), x_t(x,t),t),_1),
            0.0, 1.0)/M_TWOPI + 0.5;

    }
    Real HestonRNDCalculator::invcdf(Real p, Time t) const {
        const Real v0    = hestonProcess_->v0();
        const Real kappa = hestonProcess_->kappa();
        const Real theta = hestonProcess_->theta();

        const Volatility expVol
            = sqrt(theta + (v0-theta)*(1-exp(-kappa*t))/(t*kappa));

        const boost::shared_ptr<BlackScholesMertonProcess> bsmProcess(
            new BlackScholesMertonProcess(
                hestonProcess_->s0(),
                hestonProcess_->dividendYield(),
                hestonProcess_->riskFreeRate(),
                Handle<BlackVolTermStructure>(
                    boost::shared_ptr<BlackVolTermStructure>(
                        new BlackConstantVol(
                            hestonProcess_->riskFreeRate()->referenceDate(),
                            NullCalendar(),
                            expVol,
                            hestonProcess_->riskFreeRate()->dayCounter())))));

        const Real guess = BSMRNDCalculator(bsmProcess).invcdf(p, t);

        return RiskNeutralDensityCalculator::InvCDFHelper(
            this, guess, 0.1*integrationEps_, maxIntegrationIterations_)
            .inverseCDF(p, t);
    }
}
