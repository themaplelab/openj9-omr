#include "optimizer/AbsState.hpp"

AbsState::AbsState(TR::Region &region, TR::ResolvedMethodSymbol* symbol) :
      _region(region),
      _array(region, maxLocal(symbol)),
      _stack(region, maxStack(symbol))
   {
   }

AbsState::AbsState(const AbsState& other) :
      _region(other._region),
      _array(other._array, other._region),
      _stack(other._stack, other._region)
   {
   }

UDATA AbsState::maxStack(TR::ResolvedMethodSymbol* symbol)
   {
   TR_ResolvedJ9Method *method = (TR_ResolvedJ9Method *)symbol->getResolvedMethod();
   J9ROMMethod *romMethod = method->romMethod();
   return J9_MAX_STACK_FROM_ROM_METHOD(romMethod);
   }

IDATA AbsState::maxLocal(TR::ResolvedMethodSymbol* symbol)
   {
   TR_ResolvedJ9Method *method = (TR_ResolvedJ9Method *)symbol->getResolvedMethod();
   J9ROMMethod *romMethod = method->romMethod();
   return J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
   }

AbsValue* AbsState::at(unsigned int n)
   {
   return _array.at(n);
   }

void AbsState::set(unsigned int n, AbsValue* absValue)
   {
   _array.set(n, absValue);
   }

size_t AbsState::getStackSize()
   {
   return _stack.size();
   }

size_t AbsState::getArraySize()
   {
   return _array.size();
   }

void AbsState::merge(AbsState &other, TR::ValuePropagation *vp)
   {
   AbsState copyOfOther(other);
   _array.merge(copyOfOther._array, _region, vp);
   _stack.merge(copyOfOther._stack, _region, vp);

   if (TR::comp()->trace(OMR::benefitInliner)) 
      trace(vp);
   }

AbsValue* AbsState::pop()
   {
   AbsValue* absValue = _stack.top();
   _stack.pop();
   return absValue;
   }

AbsValue* AbsState::top()
   {
      int a = 1;
   return _stack.top();
   }

void AbsState::push(AbsValue *absValue)
   {
   _stack.push(absValue);
   }

void AbsState::trace(TR::ValuePropagation *vp)
   {
   _array.trace(vp);
   _stack.trace(vp, _region);
   }