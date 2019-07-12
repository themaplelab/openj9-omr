#pragma once

#include <vector>
#include "compiler/infra/ReferenceWrapper.hpp"
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
  void at(unsigned int, TR::VPConstraint*);
  size_t size() const;
  TR::VPConstraint *at(unsigned int);
private:
  //typedef TR::typed_allocator<TR::reference_wrapper<TR::VPConstraint>, TR::Region> VectorAllocator;
  //typedef std::vector<TR::reference_wrapper<TR::VPConstraint>, VectorAllocator> ConstraintArray;
  typedef TR::typed_allocator<TR::VPConstraint*, TR::Region> VectorAllocator;
  typedef std::vector<TR::VPConstraint*, VectorAllocator> ConstraintArray;
  ConstraintArray _array;
};
