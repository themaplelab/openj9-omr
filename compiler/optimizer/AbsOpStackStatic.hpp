#pragma once

#include "env/Region.hpp"
#include "compiler/infra/ReferenceWrapper.hpp"
#include "compiler/infra/deque.hpp"
#include "compiler/env/Region.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"
#include "compiler/optimizer/AbsStateVisitor.hpp"


//TODO: can we inherit instead of encapsulating?
//TODO: use AbsValue instead of VPConstraint
class AbsOpStackStatic
{
public:
  AbsOpStackStatic(TR::Region & region, unsigned int maxSize);
  AbsOpStackStatic(const AbsOpStackStatic&, TR::Region &);
  AbsOpStackStatic(const AbsOpStackStatic&) = delete; // we need a region...

  void visit(AbstractStateVisitor *visitor);
  void visit(AbstractStateVisitor *visitor, AbsOpStackStatic *other);
  void merge(const AbsOpStackStatic &, TR::Region &, OMR::ValuePropagation *);
  void pop();
  void trace(OMR::ValuePropagation *, TR::Region &region);
  void push(AbsValue*);
  bool empty() const;
  size_t size() const;
  AbsValue* top();
  AbsOpStackStatic *getWidened(TR::Region &region);
private:
  friend class ArgumentsEstimator;
  typedef TR::deque<AbsValue*, TR::Region&> ConstraintStack;
  ConstraintStack _stack;
  int _maxSize;

};


