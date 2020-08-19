#include "Unifier.h"

#include "Substituter.h"
#include "TipAlpha.h"
#include "TipCons.h"
#include "TipMu.h"
#include "TypeVars.h"
#include "UnificationError.h"
#include "loguru.hpp"
#include <iostream>
#include <sstream>
#include <utility>

namespace { // Anonymous namespace for local helper functions

bool contains(std::set<std::shared_ptr<TipVar>> s, std::shared_ptr<TipVar> t) {
  for (auto e : s) {
    if (e.get() == t.get()) return true;
  } 
  return false;
}

}

Unifier::Unifier() : unionFind(std::move(std::make_unique<UnionFind>())) {}

Unifier::Unifier(std::vector<TypeConstraint> constrs) : constraints(std::move(constrs)) {
    std::vector<std::shared_ptr<TipType>> types;
    for(TypeConstraint& constraint : constraints) {
        auto lhs = constraint.lhs;
        auto rhs = constraint.rhs;
        types.push_back(lhs);
        types.push_back(rhs);

        if(auto f1 = std::dynamic_pointer_cast<TipCons>(lhs)) {
            for(auto &a : f1->getArguments()) {
                types.push_back(a);
            }
        }
        if(auto f2 = std::dynamic_pointer_cast<TipCons>(rhs)) {
            for(auto &a : f2->getArguments()) {
                types.push_back(a);
            }
        }
    }

    unionFind = std::make_unique<UnionFind>(types);
}

void Unifier::solve() {
    for(TypeConstraint &constraint: constraints) {
        unify(constraint.lhs, constraint.rhs);
    }
}

void Unifier::unify(std::shared_ptr<TipType> t1, std::shared_ptr<TipType> t2) {
    LOG_S(1) << "Unifying " << *t1 << " and " << *t2;

    auto rep1 = unionFind->find(t1);
    auto rep2 = unionFind->find(t2);

    LOG_S(1) << "  Unifying with representatives " << *rep1 << " and " << *rep2;

    if(*rep1 == *rep2) {
       return;
    }

    if(isVar(rep1) && isVar(rep2)) {
        unionFind->quick_union(rep1, rep2);
    } else if(isVar(rep1) && isProperType(rep2)) {
        unionFind->quick_union(rep1, rep2);
    } else if(isProperType(rep1) && isVar(rep2)) {
        unionFind->quick_union(rep2, rep1);
    } else if(isCons(rep1) && isCons(rep2)) {
        auto f1 = std::dynamic_pointer_cast<TipCons>(rep1);
        auto f2 = std::dynamic_pointer_cast<TipCons>(rep2);
        if(!f1->doMatch(f2.get())) {
            throwUnifyException(t1,t2);
        }

        unionFind->quick_union(rep1, rep2);
        for(int i = 0; i < f1->getArguments().size(); i++) {
            auto a1 = f1->getArguments().at(i);
            auto a2 = f2->getArguments().at(i);
            unify(a1, a2);
        }
    } else {
        throwUnifyException(t1,t2);
    }

    LOG_S(1) << "  Unified representative is " << *unionFind->find(t1);
    LOG_S(1) << "  Unified double check " << *unionFind->find(t2);
}

/*! \fn close
 *  \brief Close a type expression replacing all variables with primitives.
 *
 * The method uses the solution to the type equations stored in the union-find
 * structure after solving.  It also makes use of two helper classes to
 * perform substitutions of variables and to identify the free variables in
 * the type expression (i.e., the one's not bound in mu quantifiers).
 * \sa Substituter
 * \sa TypeVars
 */
std::shared_ptr<TipType> Unifier::close(
        std::shared_ptr<TipType> type, std::set<std::shared_ptr<TipVar>> visited) {

  LOG_SCOPE_FUNCTION(1);

  if (isVar(type)) {
    auto v = std::dynamic_pointer_cast<TipVar>(type);

    LOG_S(1) << "Unifier closing variable: " << *v;

    if (!contains(visited, v) && (unionFind->find(type) != v)) {
      // No cyclic reference to v and it does not map to itself
      visited.insert(v);
      auto closedV = close(unionFind->find(type), visited);

      // If the variable is an alpha, then reuse it else create a new
      // alpha with the node.
      auto newV = (isAlpha(v)) ? v : std::make_shared<TipAlpha>(v->getNode());
      auto freeV = TypeVars::collect(closedV.get());
      if (contains(freeV,newV)) {
        // Cyclic reference requires a mu type constructor
        auto substClosedV = Substituter::substitute(closedV.get(), v.get(), newV);
        auto mu = std::make_shared<TipMu>(newV, substClosedV);

        LOG_S(1) << "Done closing variable with " << *mu;
        return mu;

      } else {
        // No cyclic reference in closed type
        LOG_S(1) << "Done closing variable with " << *closedV;
        return closedV;
      }
    } else {
      // Unconstrained type variable
      auto alpha = std::make_shared<TipAlpha>(v->getNode());

      LOG_S(1) << "Done closing variable with " << *alpha;
      return alpha;
    } 

  } else if (isCons(type)) {
    auto c = std::dynamic_pointer_cast<TipCons>(type);

    LOG_S(1) << "Unifier closing constructor: " << *c;

    // close each argument of the constructor for each free variable
    auto freeV = TypeVars::collect(c.get());

    std::vector<std::shared_ptr<TipType>> temp;
    auto current = c->getArguments();
    for (auto v : freeV) {
      for (auto a : current) {
         auto closedV = close(unionFind->find(v), visited);
         auto subst = Substituter::substitute(a.get(), v.get(), closedV);
         temp.push_back(subst);
      }

      current = temp;
      temp.clear();
    }

    // replace arguments with current
    c->setArguments(current);

    LOG_S(1) << "Unifier done closing constructor with " << *c;

    return c;

  } else if (isMu(type)) {
    auto m = std::dynamic_pointer_cast<TipMu>(type);

    LOG_S(1) << "Unifier done closing mu with " << *m;

    return std::make_shared<TipMu>(m->getV(), close(m->getT(), visited));

  } 

  // TBD : I think this is unreachable
  return type;
}

/*! \brief Looks up the inferred type in the type solution.
 *
 * Here we want to produce an inferred type that is "closed" in the
 * sense that all variables in the type definition are replaced with
 * their base types.
 */ 
std::shared_ptr<TipType> Unifier::inferred(std::shared_ptr<TipVar> v) {
  std::set<std::shared_ptr<TipVar>> visited;
  return close(unionFind->find(v), visited);
}

void Unifier::throwUnifyException(std::shared_ptr<TipType> t1, std::shared_ptr<TipType> t2) {
    std::stringstream s;
    s << "Type error cannot unify " << *t1 << " and " << *t2 <<
        " (respective roots are: " << *unionFind->find(t1) << " and " <<
        *unionFind->find(t2) << ")";
    throw UnificationError(s.str().c_str());
}

bool Unifier::isVar(std::shared_ptr<TipType> type) {
    return std::dynamic_pointer_cast<TipVar>(type) != nullptr;
}

bool Unifier::isProperType(std::shared_ptr<TipType> type) {
    return std::dynamic_pointer_cast<TipVar>(type) == nullptr;
}

bool Unifier::isCons(std::shared_ptr<TipType> type) {
    return std::dynamic_pointer_cast<TipCons>(type) != nullptr;
}

bool Unifier::isMu(std::shared_ptr<TipType> type) {
    return std::dynamic_pointer_cast<TipMu>(type) != nullptr;
}

bool Unifier::isAlpha(std::shared_ptr<TipType> type) {
    return std::dynamic_pointer_cast<TipAlpha>(type) != nullptr;
}

