#pragma once

#include <stack>
#include <deque>
#include "compiler/infra/ReferenceWrapper.hpp"
#include "compiler/env/Region.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"

//TODO: can we inherit instead of encapsulating?
//TODO: use AbsValue instead of VPConstraint
class AbsOpStackStatic
{
public:
  AbsOpStackStatic(TR::Region & region, unsigned int maxSize);
  void pop();
  void trace(OMR::ValuePropagation *);
  void push(TR::VPConstraint *);
  bool empty() const;
  size_t size() const;
  TR::VPConstraint* top();
private:
  //typedef TR::typed_allocator<TR::reference_wrapper<TR::VPConstraint>, TR::Region> DequeAllocator;
  //typedef std::deque<TR::reference_wrapper<TR::VPConstraint>, DequeAllocator> StackContainer;
  //typedef std::stack<TR::reference_wrapper<TR::VPConstraint>, StackContainer> ConstraintStack;
  typedef TR::typed_allocator<TR::VPConstraint*, TR::Region> DequeAllocator;
  typedef std::deque<TR::VPConstraint*, DequeAllocator> StackContainer;
  typedef std::stack<TR::VPConstraint*, StackContainer> ConstraintStack;
  ConstraintStack _stack; 
};
