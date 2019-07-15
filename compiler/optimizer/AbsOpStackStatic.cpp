#include "compiler/optimizer/AbsOpStackStatic.hpp"

AbsOpStackStatic::AbsOpStackStatic(TR::Region &region, unsigned int maxSize) :
  _stack(StackContainer(DequeAllocator(region)))
  {
  }

void
AbsOpStackStatic::push(AbsValue *value)
  {
  //TODO: fix for types that occupy two stack positions.
  _stack.push(value);
  }

void
AbsOpStackStatic::pop()
  {
  //TODO: fix for types that occupy two stack positions.
  _stack.pop();
  }

AbsValue*
AbsOpStackStatic::top()
  {
  //TODO: fix for types that occupy two stack positions.
  return _stack.top();
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
AbsOpStackStatic::trace(OMR::ValuePropagation *vp)
  {
  AbsOpStackStatic copy(*this);
  TR::Compilation *comp = TR::comp();
  traceMsg(comp, "contents of operand stack:\n");
  int size = this->size();
  for (int i = 0; i < size; i++)
    {
    copy.top()->print(vp);
    copy.pop();
    traceMsg(comp, "\n");
    }
  traceMsg(comp, "\n");
  }
