#pragma once

#include "compiler/optimizer/AbsVarArrayStatic.hpp"
#include "compiler/optimizer/AbsOpStackStatic.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"

class AbsEnvStatic {
public:
  AbsEnvStatic(TR::Region&, UDATA, IDATA, TR::ValuePropagation *);
  void trace();
  void enterMethod(TR::ResolvedMethodSymbol *);
private:
  TR::ValuePropagation *_vp;
  AbsVarArrayStatic _array;
  AbsOpStackStatic _stack;

  void at(unsigned int, TR::VPConstraint *);
  TR::VPConstraint* getClassConstraint(TR_OpaqueClassBlock *);
};
