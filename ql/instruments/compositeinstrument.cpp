/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 StatPro Italia srl

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

#include <ql/instruments/compositeinstrument.hpp>
#include <sstream>

namespace QuantLib {

    void CompositeInstrument::add(
           const ext::shared_ptr<Instrument>& instrument, Real multiplier) {
        components_.emplace_back(instrument, multiplier);
        registerWith(instrument);
        update();
        // When we ask for the NPV of an expired composite, the
        // components are not recalculated and thus wouldn't forward
        // later notifications according to the default behavior of
        // LazyObject instances.  This means that even if the
        // evaluation date changes so that the composite is no longer
        // expired, the instrument wouldn't be notified and thus it
        // wouldn't recalculate.  To avoid this, we override the
        // default behavior of the components.
        instrument->alwaysForwardNotifications();
    }

    void CompositeInstrument::subtract(
           const ext::shared_ptr<Instrument>& instrument, Real multiplier) {
        add(instrument, -multiplier);
    }

    bool CompositeInstrument::isExpired() const {
        for (const auto& component : components_) {
            if (!component.first->isExpired())
                return false;
        }
        return true;
    }

    void CompositeInstrument::performCalculations() const {
        NPV_ = 0.0;
        for (const auto& component : components_) {
            NPV_ += component.second * component.first->NPV();
        }
        updateAdditionalResults();
    }

    void CompositeInstrument::deepUpdate() {
        for (const_iterator i=components_.begin(); i!=components_.end(); ++i) {
            i->first->deepUpdate();
        }
        update();
    }

    void CompositeInstrument::updateAdditionalResults() const {

        using std::string;
        typedef std::map<string, boost::any> Results;
        typedef Results::const_iterator RIt;

        // Loop over each component's additional results and add them to additionalResults_.
        additionalResults_.clear();
        for (auto i = components_.begin(); i != components_.end(); ++i) {

            // Keep track of component's index. Prepend it to additional results.
            Size cmpIdx = std::distance(components_.begin(), i) + 1;
            std::stringstream ss;
            ss << cmpIdx << "_";
            string prefix = ss.str();

            // Update the additionalResults_.
            const Results& cmpResults = i->first->additionalResults();
            for (const auto& cmpResult : cmpResults) {
                additionalResults_[prefix + cmpResult.first] = cmpResult.second;
            }
            additionalResults_[prefix + "multiplier"] = i->second;
        }
    }

}

