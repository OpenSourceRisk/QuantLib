/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2005 Joseph Wang
 Copyright (C) 2005, 2006 Theo Boafo

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

/*! \file callabilityschedule.hpp
    \brief Schedule of put/call dates
*/

#ifndef quantlib_callability_schedule_hpp
#define quantlib_callability_schedule_hpp

#include <ql/event.hpp>
#include <ql/instruments/bond.hpp>
#include <ql/interestrate.hpp>
#include <ql/patterns/visitor.hpp>
#include <ql/utilities/null.hpp>
#include <ql/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <vector>

namespace QuantLib {

    struct BondPriceGetter : boost::static_visitor<const Bond::Price&> {
        BondPriceGetter() {}
        const Bond::Price& operator()(const Bond::Price& p) const { return p; }
        const Bond::Price& operator()(const InterestRate& p) const { 
            QL_FAIL("Must be a Bond::Price");
        }
    };

    struct InterestRateGetter : boost::static_visitor<const InterestRate&> {
        InterestRateGetter() {}
        const InterestRate& operator()(const Bond::Price& p) const { QL_FAIL("Must be a Bond::Price"); }
        const InterestRate& operator()(const InterestRate& p) const { return p; }
    };

    //! %instrument callability
    class Callability : public Event {
      public:
        //! type of the callability
        enum Type { Call, Put };

        Callability(boost::variant<Bond::Price, InterestRate> variable, Type type, const Date& date)
        : variable_(variable), type_(type), date_(date) {}

        Callability(const Bond::Price& price, Type type, const Date& date)
        : variable_(price), type_(type), date_(date) {}

        Callability(const InterestRate& yield, Type type, const Date& date)
        : variable_(yield), type_(type), date_(date) {}

        const Bond::Price& price() const {
            QL_REQUIRE(variable_, "no bond price given");
            return boost::apply_visitor(BondPriceGetter(), *variable_);
        }
        const InterestRate& yield() const {
            QL_REQUIRE(variable_, "no yield given");
            return boost::apply_visitor(InterestRateGetter(), *variable_);
        }
        const bool isBondPrice() const { 
            QL_REQUIRE(variable_, "no bond price or yield given");
            return (*variable_).which() == 0;
        }
        Type type() const { return type_; }
        //! \name Event interface
        //@{
        Date date() const override { return date_; }
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&) override;
        //@}
      private:
        boost::optional<boost::variant<Bond::Price, InterestRate>> variable_;
        Type type_;
        Date date_;
    };

    inline void Callability::accept(AcyclicVisitor& v){
        auto* v1 = dynamic_cast<Visitor<Callability>*>(&v);
        if (v1 != nullptr)
            v1->visit(*this);
        else
            Event::accept(v);
    }

    typedef std::vector<ext::shared_ptr<Callability> > CallabilitySchedule;

}

#endif
