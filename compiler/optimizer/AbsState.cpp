#include "optimizer/AbsState.hpp"

AbsState::AbsState(TR::Region &region) :
      _array(region),
      _stack(region)
   {
   }

AbsState::AbsState(AbsState* other) :
      _array(other->_array),
      _stack(other->_stack)
   {
   }

void AbsState::merge(AbsState* other, TR::ValuePropagation *vp)
   {
   _array.merge(other->_array, vp);
   _stack.merge(other->_stack, vp);
   }


void AbsState::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "\nContents of AbsState \n");
   _array.trace(vp);
   _stack.trace(vp);
   }