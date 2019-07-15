#pragma once

#include "compiler/optimizer/AbsVarArrayStatic.hpp"
#include "compiler/optimizer/AbsOpStackStatic.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"

class AbsEnvStatic {
public:
  AbsEnvStatic(TR::Region&, UDATA, IDATA, TR::ValuePropagation *);
  void trace(const char*);
  void enterMethod(TR::ResolvedMethodSymbol *);
private:
  TR::ValuePropagation *_vp;
  AbsVarArrayStatic _array;
  AbsOpStackStatic _stack;
  TR::Region &_region;

  void at(unsigned int, AbsValue*);
  AbsValue* getClassConstraint(TR_OpaqueClassBlock *);
};
