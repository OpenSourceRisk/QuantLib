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

#include <ql/observablegraph.hpp>
#include <iomanip>
#include <iostream>
#include <typeinfo>

namespace QuantLib {

    ObservableGraph::NodeType::NodeType(QuantLib::Observer* const o) { observer = o; }
    ObservableGraph::NodeType::NodeType(QuantLib::Observable* const o) { observable = o; }

    bool ObservableGraph::NodeType::operator==(const NodeType& a) const {
        return observer == a.observer && observable == a.observable;
    }

    std::string ObservableGraph::NodeType::getTypeName() const {
        if (observer != nullptr) {
            return typeid(*observer).name();
        } else if (observable != nullptr) {
            return typeid(*observable).name();
        } else {
            QL_FAIL("internal error: getTypeName(): got nullptr");
        }
    }

    boost::unordered_set<boost::shared_ptr<QuantLib::Observable> >
    ObservableGraph::NodeType::getObservables() const {
        if (observer != nullptr) {
            return observer->observables();
        } else {
            return {};
        }
    }

    ObservableGraph::ObservableGraph(const boost::unordered_set<QuantLib::Observer*>& observers) {
        for (auto const& o : observers) {
            nodes_.push_back(NodeType(o));
        }
        std::cout << "got " << nodes_.size() << " input nodes." << std::endl;
        visited_.resize(nodes_.size(), 0);
        order_.resize(nodes_.size(), 0);
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            if (visited_[i] == 0) {
                dfs(i, 0);
            }
        }
        std::cout << "total number of nodes = " << nodes_.size() << std::endl;
    }

    void ObservableGraph::dfs(std::size_t i, std::size_t depth) {
        std::cout << std::string(depth, ' ') << "visit " << i << " " << nodes_[i].getTypeName()
                  << std::endl;
        order_[i] = counter_++;
        visited_[i] = 1;
        for (auto const& p : nodes_[i].getObservables()) {
            std::size_t index;
            auto n = std::find(nodes_.begin(), nodes_.end(), NodeType(p.get()));
            if (n != nodes_.end()) {
                index = std::distance(nodes_.begin(), n);
            } else {
                index = nodes_.size();
                nodes_.push_back(p.get());
                visited_.push_back(0);
                order_.push_back(0);
            }
            if (visited_[index] == 0) {
                dfs(index, depth + 4);
            } else if (visited_[index] == 1) {
                std::cerr << "!!! back edge from " << i << " to " << index << std::endl;
            } else if (visited_[index] == 2) {
                if (order_[index] > order_[i]) {
                    // forward edge
                    std::cout << "*** forward edge from " << i << " to " << index << std::endl;
                } else {
                    // cross edge
                    std::cout << "xxx cross edge from " << i << " to " << index << std::endl;
                }
            }
        }
        visited_[i] = 2;
    }

} // namespace QuantLib
