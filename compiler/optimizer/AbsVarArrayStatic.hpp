#pragma once

#include "env/Region.hpp"
#include "compiler/infra/ReferenceWrapper.hpp"
#include "compiler/infra/deque.hpp"
#include "compiler/env/Region.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"

//TODO: can we inherit instead of encapsulating?
//TODO: use AbsValue instead of VPConstraint
class AbsVarArrayStatic {
public:
  AbsVarArrayStatic(TR::Region &region, unsigned int maxSize);
  void trace(OMR::ValuePropagation *vp);
  void at(unsigned int, AbsValue*);
  size_t size() const;
  AbsValue *at(unsigned int);
private:
  typedef TR::deque<AbsValue*, TR::Region&> ConstraintArray;
  ConstraintArray _array;
};
