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
   _array.merge(other->_array, _region, vp);
   _stack.merge(other->_stack, _region, vp);
   }

AbsValue* AbsState::pop()
   {
   //printf("pop\n");
  
   AbsValue* absValue = _stack.top();
   _stack.pop();
   //printf("size %d\n", getStackSize());
   return absValue;
   }

AbsValue* AbsState::top()
   {
   return _stack.top();
   }

void AbsState::push(AbsValue *absValue)
   {    
  //printf("push\n");

   _stack.push(absValue);
     //printf("size %d\n", getStackSize());
   }

void AbsState::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "\n#### Contents of AbsState ####\n");
   _array.trace(vp);
   _stack.trace(vp);
   }