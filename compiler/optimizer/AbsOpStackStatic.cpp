#include "compiler/optimizer/AbsOpStackStatic.hpp"

#include "env/Region.hpp"
#include "compiler/infra/deque.hpp"

AbsOpStackStatic::AbsOpStackStatic(TR::Region &region, unsigned int maxSize) :
  _stack(StackContainer(0, nullptr, region))
  {
  }

void
AbsOpStackStatic::push(AbsValue *value)
  {
  _stack.push(value);
  }

void
AbsOpStackStatic::pop()
  {
  _stack.pop();
  }

AbsValue*
AbsOpStackStatic::top()
  {
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
    traceMsg(comp, "[%d] = ", size - i);
    if (value) value->print(vp);
    traceMsg(comp, "\n");
  }
  traceMsg(comp, "<bottom>\n");
  traceMsg(comp, "\n");
  }
