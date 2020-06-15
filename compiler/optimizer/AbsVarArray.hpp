#pragma once

#include <vector>
#include "compiler/optimizer/VPConstraint.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/optimizer/IDTNode.hpp"

class AbsEnv;


class AbsVarArray {
public:
  AbsVarArray(TR::Compilation *comp, TR::Region &region);
  AbsVarArray(const AbsVarArray &absVarArray);
  void at(int, TR::VPConstraint*);
  TR::VPConstraint* at(int) const;
  void initialize(IDTNode*, AbsEnv*, TR::ValuePropagation*);
  int size() const;
  void merge(AbsVarArray &);
  void print();
  TR::ValuePropagation *_vp;
private:
  typedef TR::typed_allocator< TR::VPConstraint*, TR::Region& > VPConstraintPtrAllocator;
  typedef std::vector<TR::VPConstraint*, VPConstraintPtrAllocator> VPConstraintPtrVector;
  TR::VPConstraint* _base[32];
  TR::Compilation* _comp;
  TR::Region &_region;
};
