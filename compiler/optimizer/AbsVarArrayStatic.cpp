#include "AbsVarArrayStatic.hpp"
#include "compiler/infra/ReferenceWrapper.hpp"

AbsVarArrayStatic::AbsVarArrayStatic(TR::Region &region, unsigned int maxSize) :
  _array(0, nullptr, region)
  {
  }

AbsVarArrayStatic::AbsVarArrayStatic(const AbsVarArrayStatic &other, TR::Region &region) :
  _array(other._array)
  {
  //_array = other._array;
  }

void
AbsVarArrayStatic::merge(const AbsVarArrayStatic &array, TR::Region &regionAbsEnv, OMR::ValuePropagation *vp)
  {
  //TR_ASSERT(this->size() == array.size(), "array different sizes");
  // arrays CAN be different sizes, just not the stack.
  TR::Region &region = TR::comp()->trMemory()->currentStackRegion();
  ConstraintArray copy(array._array);
  int copySize = copy.size();
  int size = this->size();
  size = size > copySize ? size : copySize;
  for (int i = 0; i < size; i++)
     {
        AbsValue *self = i < this->size() ? this->at(i) : nullptr;
        AbsValue *other = i < copy.size() ? copy.at(i) : nullptr;
        if (!self && other) {
           this->at(i, other);
           continue;
        }
        else if (self && other) {
          AbsValue *merge = self->merge(other, regionAbsEnv, vp);
          this->at(i, merge);
        } else if (self) {
          this->at(i, self);
        } else if (other) {
          this->at(i, other);
        }
     }
  }

void
AbsVarArrayStatic::at(unsigned int index, AbsValue *constraint)
  {
  if (this->size() > index)
     {
     _array.at(index) = constraint;
     return;
     }
  if (index == this->size())
     {
     _array.push_back(constraint);
     return;
     }
  while (this->size() < index)
     {
     _array.push_back(nullptr);
     }
     _array.push_back(constraint);
  }

AbsValue*
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
  TR::Compilation *comp = TR::comp();
  traceMsg(comp, "contents of variable array:\n");
  int size = this->size();
  for (int i = 0; i < size; i++)
    {
    traceMsg(comp, "a[%d] = ", i);
    if (!this->at(i)) {
       traceMsg(comp, "nullptr\n");
       continue;
    }
    this->at(i)->print(vp);
    traceMsg(comp, "\n");
    }
  traceMsg(comp, "\n");
  }
