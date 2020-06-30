#include "compiler/optimizer/AbsOpStackStatic.hpp"
#include "env/Region.hpp"
#include "compiler/infra/deque.hpp"

//TODO: can we use maxSize somehow to make memory allocation more efficient?
AbsOpStackStatic::AbsOpStackStatic(TR::Region &region, unsigned int maxSize) :
      _stack(StackContainer(0, nullptr, region))
   {
   }


AbsOpStackStatic::AbsOpStackStatic(const AbsOpStackStatic &other, TR::Region &region) :
      _stack(other._stack)
   {
   }

void AbsOpStackStatic::merge(const AbsOpStackStatic &stack, TR::Region &regionForAbsValue, OMR::ValuePropagation *valuePropagation)
   {
   TR_ASSERT_FATAL(stack._stack.size() == this->_stack.size(), "stacks are different sizes!");

   //FIXME? I think I can use TR::comp()->trMemory()->currentStackRegion()
   TR::Region &region = TR::comp()->trMemory()->currentStackRegion();
   AbsValueStack copyOfOther(stack._stack);

   StackContainer dequeSelf(region);
   StackContainer dequeOther(region);
  
   //TODO: is there an easier way to merge to stacks?
   int size = this->_stack.size();
   for (int i = 0; i < size; i++)
      {
      dequeSelf.push_back(this->top());
      dequeOther.push_back(copyOfOther.top());
      this->pop();
      copyOfOther.pop();
      }
   // cool so now we have the contents of both of these stack in a deque.
   // top is at front and bottom is at back.
   // we want to start merging from the bottom.
   for (int i = 0; i < size; i++)
      {
      AbsValue *valueSelf = dequeSelf.back();
      AbsValue *valueOther = dequeOther.back();
      AbsValue *merge = valueSelf->merge(valueOther, regionForAbsValue, valuePropagation);
      this->push(merge);
      dequeSelf.pop_back();
      dequeOther.pop_back();
      }
   }

void AbsOpStackStatic::push(AbsValue *value)
   {
   TR_ASSERT(value, "null value");
   _stack.push(value);
   }

void AbsOpStackStatic::pop()
   {
   TR_ASSERT_FATAL(size() > 0, "Pop an empty stack!");
   _stack.pop();
   }

AbsValue* AbsOpStackStatic::top()
   {
   TR_ASSERT_FATAL(size() > 0, "Top an empty stack!");
   return _stack.top();
   }

bool AbsOpStackStatic::empty()
   {
   return _stack.empty();
   }

size_t AbsOpStackStatic::size()
   {
   return _stack.size();
   }

void AbsOpStackStatic::trace(OMR::ValuePropagation *vp, TR::Region &region)
   {
   TR::Compilation *comp = TR::comp();
   traceMsg(comp, "contents of operand stack:\n");
   int size = this->size();
   if (size == 0)
      {
      traceMsg(comp, "<empty>\n");
      traceMsg(comp, "\n");
      return;
      }
   AbsValueStack copy(this->_stack);
   traceMsg(comp, "<top>\n");
   for (int i = 0; i < size; i++) 
      {
      AbsValue *value = copy.top();
      copy.pop();
      traceMsg(comp, "fp[%d] = ", size - i - 1);
      if (value) value->print(vp);
      traceMsg(comp, "\n");
      }
   traceMsg(comp, "<bottom>\n");
   traceMsg(comp, "\n");
   }
