#pragma once

#include <stack>
#include <deque>
#include "compiler/optimizer/VPConstraint.hpp"
#include "compile/Compilation.hpp"
#include "cs2/allocator.h"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "infra/Stack.hpp"
#include "AbsValue.hpp"


class AbsOpStack {
public:
  AbsOpStack(TR::Compilation *comp, TR::Region & region);
  AbsOpStack(const AbsOpStack & );
  typedef AbsValue::CompareResult CompareResult;
  // TODO
  CompareResult compareWith(AbsOpStack *other);
  void pushConstraint(TR::VPConstraint*);
  void pushConstInt(int i);
  void pushNullConstraint(void);
  void copyStack(AbsOpStack &);
  TR::VPConstraint* element(int i);
  TR::VPConstraint* top();
  void pop();
  bool empty();
  void merge(AbsOpStack &absOpStack, TR::ValuePropagation *_vp);
  TR::VPConstraint* topAndPop();
  uint32_t size() const;
  void print();
private:
  TR_Stack<TR::VPConstraint*> *_base;
  TR::Compilation* _comp;
  TR::Region &_region;
};
