#include "compiler/optimizer/AbsOpStackStatic.hpp"

#include "env/Region.hpp"
#include "compiler/infra/deque.hpp"

//TODO: can we use maxSize somehow to make memory allocation more efficient?
AbsOpStackStatic::AbsOpStackStatic(TR::Region &region, unsigned int maxSize) :
  _stack(StackContainer(0, nullptr, region))
  //_stack(0, nullptr, region)
  {
  }

//AbsOpStackStatic::AbsOpStackStatic(AbsOpStackStatic &other, TR::Region &region) :
AbsOpStackStatic::AbsOpStackStatic(const AbsOpStackStatic &other, TR::Region &region) :
  //_stack(0, nullptr, region)
  //_stack(StackContainer(0, nullptr, region))
  _stack(other._stack)
  {
/*
  int size = other.size();
  for (int i = 0; i < size; i++)
    {
    AbsValue *value = other._stack.back();
    other._stack.pop_back(); 
    this->_stack.push_front(value);
    other._stack.push_front(value);
    }
*/
  }

void
AbsOpStackStatic::merge(const AbsOpStackStatic &stack, TR::Region &regionForAbsValue, OMR::ValuePropagation *valuePropagation)
  {
  TR_ASSERT(stack._stack.size() == this->_stack.size(), "stacks are different sizes!");
  //FIXME? I think I can use TR::comp()->trMemory()->currentStackRegion()
  TR::Region &region = TR::comp()->trMemory()->currentStackRegion();
  ConstraintStack copyOfOther(stack._stack);
  //ConstraintStack copyOfOther(StackContainer(0, nullptr, region));
  //copyOfOther = stack._stack;
  StackContainer dequeSelf(region);
  StackContainer dequeOther(region);
  //dequeSelf(region);
  //StackContainer dequeOther(region);
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

void
AbsOpStackStatic::push(AbsValue *value)
  {
  _stack.push(value);
  //_stack.push_back(value);
  }

void
AbsOpStackStatic::pop()
  {
  _stack.pop();
  //_stack.pop_back();
  }

AbsValue*
AbsOpStackStatic::top()
  {
  return _stack.top();
  //return _stack.back();
  }

bool
AbsOpStackStatic::empty() const
  {
  return _stack.empty();
  }

size_t
AbsOpStackStatic::size() const
  {
  return _stack.size();
  }

void
AbsOpStackStatic::trace(OMR::ValuePropagation *vp, TR::Region &region)
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
  ConstraintStack copy(this->_stack);
  traceMsg(comp, "<top>\n");
  for (int i = 0; i < size; i++) {
    AbsValue *value = copy.top();
    copy.pop();
    //AbsValue *value = this->_stack.back();
    //this->_stack.pop_back();
    traceMsg(comp, "fp[%d] = ", size - i - 1);
    if (value) value->print(vp);
    traceMsg(comp, "\n");
    //this->_stack.push_front(value);
  }
  traceMsg(comp, "<bottom>\n");
  traceMsg(comp, "\n");
  }
