#include "AbsOpStack.hpp"

#include <cstring>

#include "compiler/optimizer/VPConstraint.hpp"
#include "compile/Compilation.hpp"
#include "infra/List.hpp"
#include "infra/TRlist.hpp"
#include "infra/vector.hpp"

#include "env/jittypes.h"

#ifndef TRACE
//#define TRACE(COND, M, ...) 
#define TRACE(COND, M, ...) \
if ((COND)) { traceMsg(this->_comp, M, ##__VA_ARGS__); }
#endif

using TR::VPConstraint;


AbsOpStack::AbsOpStack(TR::Compilation *comp, TR::Region &region)  :
  _comp(comp),
  _region(region),
  _base(NULL)
{
     TR_ASSERT(_comp, "comp is null");
     this->_base = new (region) TR_Stack<VPConstraint*> (_comp->trMemory(), 8, true, heapAlloc);
}

AbsOpStack::AbsOpStack(const AbsOpStack &absOpStack) :
   _region(absOpStack._region),
   _comp(absOpStack._comp) ,
   _base(NULL)
{
    TR_ASSERT(_comp, "comp is null");
    this->_base = new (_region) TR_Stack<VPConstraint*> (_comp->trMemory(), 8, true, heapAlloc);
}

// TODO
AbsOpStack::CompareResult AbsOpStack::compareWith(AbsOpStack *other)
  {
  if (other == nullptr)
    {
    return CompareResult::Narrower;
    }
  return CompareResult::Narrower;
  }

void
AbsOpStack::copyStack(AbsOpStack &absOpStack) {
  if (absOpStack.empty()) { return ; }
  for (int i = 0; i < absOpStack.size(); i++) {
     this->_base->insert(absOpStack.element(i), i);
  }
}

TR::VPConstraint*
AbsOpStack::element(int i) {
  return this->_base->element(i);
}

void
AbsOpStack::pop() {
  if (!this->empty()) { this->_base->pop(); }
}


TR::VPConstraint*
AbsOpStack::top() {
  return this->empty() ? NULL : this->_base->top();
}


TR::VPConstraint*
AbsOpStack::topAndPop() {
  auto constraint = this->top();
  this->pop();
  return constraint;
}


void
AbsOpStack::pushConstInt(int i) {
  TR::VPIntConst *iconst = new (this->_region) TR::VPIntConst(i);
  //iconst->print(this->_comp, this->_comp->getOutFile());
  this->pushConstraint(iconst);      
}

void
AbsOpStack::print() {
  if(this->empty()) return;

  TR_ASSERT(_comp, "comp is null");
  TRACE(true, "stack size = %d", this->size());
  for (int i = 0; i < this->size(); i++) {
    TRACE(true, "stack[%d] = ", i);
    if (this->_base->element(i) == NULL)  {
      TRACE(true, "NULL\n");
      continue;
    }
    this->_base->element(i)->print(this->_comp, this->_comp->getOutFile());
    TRACE(true, "\n");
  }
}

void
AbsOpStack::pushConstraint(TR::VPConstraint* constraint) {
  this->_base->push(constraint);
}

void
AbsOpStack::pushNullConstraint() {
  this->pushConstraint(NULL);
}

void
AbsOpStack::merge(AbsOpStack &absOpStack, TR::ValuePropagation *_vp) {
  if (absOpStack.empty()) return;

  for (int i = 0; i < absOpStack.size(); i++) {
     if (this->_base->size() <= i) break;
     if (this->_base->element(i) == NULL) continue;
     if (absOpStack.element(i) == NULL) continue;
     this->_base->element(i)->merge(absOpStack.element(i), _vp);
  }
}


bool
AbsOpStack::empty()
  {
  return this->_base->isEmpty();
  }

uint32_t
AbsOpStack::size() const
  {
  return this->_base->size();
  }

