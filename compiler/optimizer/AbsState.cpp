#include "optimizer/AbsState.hpp"

AbsState::AbsState(TR::Region &region, unsigned int maxLocalVarArrayDepth, unsigned int maxOpStackDepth) :
      _region(region),
      _array(region, maxLocalVarArrayDepth),
      _stack(region, maxOpStackDepth)
   {
   }

AbsState::AbsState(AbsState* other) :
      _region(other->_region),
      _array(other->_array, other->_region),
      _stack(other->_stack, other->_region)
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

size_t AbsState::getStackSize()
   {
   return _stack.size();
   }

size_t AbsState::getArraySize()
   {
   return _array.size();
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
   printf("pop\n");
  
   _stack.pop();
    printf("size %d\n", _stack.size());
   return absValue;
   }

AbsValue* AbsState::top()
   {
      int a = 1;
   return _stack.top();
   }

void AbsState::push(AbsValue *absValue)
   {
      printf("push\n");
     
   _stack.push(absValue);
    printf("size %d\n", _stack.size());
   }

void AbsState::trace(TR::ValuePropagation *vp)
   {
   _array.trace(vp);
   _stack.trace(vp, _region);
   }