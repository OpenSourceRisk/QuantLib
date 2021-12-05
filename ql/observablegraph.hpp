/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#pragma once

#include <ql/patterns/observable.hpp>
#include <boost/variant.hpp>

namespace QuantLib {

    class ObservableGraph {
      public:
        explicit ObservableGraph(const boost::unordered_set<QuantLib::Observer*>& observers);

      private:
        struct NodeType {
            NodeType(QuantLib::Observer* const);
            NodeType(QuantLib::Observable* const);

            std::string getTypeName() const;
            boost::unordered_set<boost::shared_ptr<QuantLib::Observable> > getObservables() const;
            bool operator==(const NodeType& a) const;

            QuantLib::Observer* observer = nullptr;
            QuantLib::Observable* observable = nullptr;
        };

        void dfs(std::size_t i, std::size_t depth);

        std::vector<std::size_t> visited_, order_;
        std::vector<NodeType> nodes_;

        std::size_t counter_ = 0;
    };

} // namespace QuantExt
