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
  AbsVarArrayStatic(const AbsVarArrayStatic&, TR::Region &);
  AbsVarArrayStatic(const AbsVarArrayStatic&) = delete; // we need TR::Region

  AbsVarArrayStatic *getWidened(TR::Region &region);
  typedef AbsValue::CompareResult CompareResult;
  CompareResult compareWith(AbsVarArrayStatic *other);
  void merge(const AbsVarArrayStatic &, TR::Region &, OMR::ValuePropagation *);
  static AbsVarArrayStatic *mergeIdenticalValuesBottom(AbsVarArrayStatic &, AbsVarArrayStatic &, TR::Region &, OMR::ValuePropagation *);
  void trace(OMR::ValuePropagation *vp);
  void at(unsigned int, AbsValue*);
  size_t size() const;
  AbsValue *at(unsigned int);
private:
  typedef TR::deque<AbsValue*, TR::Region&> ConstraintArray;
  ConstraintArray _array;
  unsigned int _maxSize;
};
