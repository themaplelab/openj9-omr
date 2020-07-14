#include "optimizer/AbsState.hpp"

AbsState::AbsState(TR::Region &region) :
      _region(region),
      _array(region),
      _stack(region)
   {
   }

AbsState::AbsState(AbsState* other) :
      _region(other->_region),
      _array(other->_array),
      _stack(other->_stack)
   {
   }

AbsValue* AbsState::at(unsigned int n)
   {
   return _array.at(n);
   }

void AbsState::set(unsigned int n, AbsValue* absValue)
   {
   _array.set(n, absValue);
   }

void AbsState::merge(AbsState* other, TR::ValuePropagation *vp)
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
   return _stack.top();
   }

void AbsState::push(AbsValue *absValue)
   {     
   _stack.push(absValue);
   }

void AbsState::trace(TR::ValuePropagation *vp)
   {
   _array.trace(vp);
   _stack.trace(vp);
   }