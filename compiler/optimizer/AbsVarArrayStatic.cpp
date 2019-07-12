#include "AbsVarArrayStatic.hpp"
#include "compiler/infra/ReferenceWrapper.hpp"

AbsVarArrayStatic::AbsVarArrayStatic(TR::Region &region, unsigned int maxSize) :
  _array(VectorAllocator(region))
  {
  _array.reserve(maxSize);
  }

void
AbsVarArrayStatic::at(unsigned int index, TR::VPConstraint *constraint)
  {
  if (index == this->size())
     {
     _array.push_back(constraint);
     return;
     }
  _array.at(index) = constraint;
  }

TR::VPConstraint*
AbsVarArrayStatic::at(unsigned int index)
  {
  return _array.at(index);
  }

size_t
AbsVarArrayStatic::size() const
  {
  return _array.size();
  }

void
AbsVarArrayStatic::trace(OMR::ValuePropagation *vp)
  {
  // only works with single thread compilation
  // otherwise printing statements might intertwine.
  TR::Compilation *comp = TR::comp();
  traceMsg(comp, "contents of variable array:\n");
  int size = this->size();
  for (int i = 0; i < size; i++)
    {
    TR::VPConstraint *c = this->at(i);
    if (!c) 
      traceMsg(comp, "a[%d] = NULL", i);
    else {
      traceMsg(comp, "a[%d] = ", i);
      this->at(i)->print(vp);
    }
    traceMsg(comp, "\n");
    }
  traceMsg(comp, "\n");
  }
