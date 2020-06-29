#include "AbsVarArrayStatic.hpp"
#include "compiler/infra/ReferenceWrapper.hpp"

AbsVarArrayStatic::AbsVarArrayStatic(TR::Region &region, unsigned int maxSize) :
      _array(0, NULL, region)
   {
   }

AbsVarArrayStatic::AbsVarArrayStatic(const AbsVarArrayStatic &other, TR::Region &region) :
      _array(other._array)
   {
   }

void AbsVarArrayStatic::merge(const AbsVarArrayStatic &array, TR::Region &region, OMR::ValuePropagation *vp)
   {
   //TR_ASSERT(this->size() == array.size(), "array different sizes");
   // arrays CAN be different sizes, just not the stack.
   AbsValueArray copy(array._array);
   int copySize = copy.size();
   int size = this->size();
   size = std::max(size, copySize);
   for (int i = 0; i < size; i++)
      {
         AbsValue *self = i < this->size() ? this->at(i) : NULL;
         AbsValue *other = i < copy.size() ? copy.at(i) : NULL;
         // TODO: Jack. Is this modification correct? Let's see later
         if (!self && !other) 
            {
            continue;
            }
         else if (self && other) 
            {
            AbsValue *mergedValue = self->merge(other, region, vp);
            this->set(i, mergedValue);
            } 
         else if (self) 
            {
            this->set(i, self);
            } 
         else if (other)
            {
            this->set(i, other);
            }
      }
   }

void AbsVarArrayStatic::set(unsigned int index, AbsValue *constraint)
   {
   if (size() > index)
      {
      _array.at(index) = constraint;
      return;
      }

   if (index == size())
      {
      _array.push_back(constraint);
      return;
      }
      
   while (size() < index)
      {
      _array.push_back(NULL);
      }
      _array.push_back(constraint);
   }

AbsValue* AbsVarArrayStatic::at(unsigned int index)
   {
   TR_ASSERT_FATAL(index < size(), "Index out of range!");
   return _array.at(index);
   }

size_t AbsVarArrayStatic::size()
   {
   return _array.size();
   }

void AbsVarArrayStatic::trace(OMR::ValuePropagation *vp)
   {
   TR::Compilation *comp = TR::comp();
   traceMsg(comp, "Contents of Abstract Variable Array:\n");
   int size = this->size();
   for (int i = 0; i < size; i++)
      {
      traceMsg(comp, "a[%d] = ", i);
      if (!this->at(i))
         {
         traceMsg(comp, "NULL\n");
         continue;
         }
      this->at(i)->print(vp);
      traceMsg(comp, "\n");
      }
   traceMsg(comp, "\n");
   }
