#include "AbsVarArrayStatic.hpp"
#include "compiler/infra/ReferenceWrapper.hpp"

AbsVarArrayStatic::AbsVarArrayStatic(TR::Region &region, unsigned int maxSize) :
  _array(maxSize, nullptr, region),
  _maxSize(maxSize)
  {
  }

AbsVarArrayStatic::AbsVarArrayStatic(const AbsVarArrayStatic &other, TR::Region &region) :
  _array(other._array),
  _maxSize(other._maxSize)
  {
  }


// TODO
AbsVarArrayStatic::CompareResult AbsVarArrayStatic::compareWith(AbsVarArrayStatic *other)
  {
    if (other == nullptr)
      {
      return CompareResult::Narrower;
      }
    return CompareResult::Narrower;
  }

AbsVarArrayStatic *AbsVarArrayStatic::getWidened(TR::Region &region)
  {
    AbsVarArrayStatic *top = new (region) AbsVarArrayStatic(region, _maxSize);
    for (int i = 0; i < size(); i++)
      {
      AbsValue *v = at(i);
      if (v != nullptr)
        {
        v = v->getWidened(region);
        }
      top->at(i, v);
      }
    return top;
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
        } else {
          this->at(i, nullptr);
        }
     }
  }

void
AbsVarArrayStatic::at(unsigned int index, AbsValue *constraint)
  {
  // Passing size to the deque constructor will make it the correct size
  _array.at(index) = constraint;
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
       traceMsg(comp, "nullptr");
       continue;
    }
    this->at(i)->print(vp);
    traceMsg(comp, "\n");
    }
  traceMsg(comp, "\n");
  }

AbsVarArrayStatic *
AbsVarArrayStatic::mergeIdenticalValuesBottom(AbsVarArrayStatic &a, AbsVarArrayStatic &b, TR::Region &region, OMR::ValuePropagation *vp)
  {
    TR_ASSERT(a._maxSize == b._maxSize, "Arrays are different");
    AbsVarArrayStatic *merged = new (region) AbsVarArrayStatic(region, a._maxSize);
    for (int i = 0; i < a.size(); i++)
      {
      AbsValue *v1 = a.at(i);
      AbsValue *v2 = b.at(i);
      AbsValue *v;
      if (v1 != nullptr && v2 != nullptr && v1 == v2)
        {
        v = AbsValue::getBottom();
        }
      else
        {
        v = v1->merge(v2, region, vp);
        }
      merged->at(i, v);
      }
    return merged;
  }
