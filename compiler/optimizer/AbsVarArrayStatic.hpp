#ifndef ABS_VAR_ARRAY_STATIC
#define ABS_VAR_ARRAY_STATIC

#include "env/Region.hpp"
#include "compiler/infra/ReferenceWrapper.hpp"
#include "compiler/infra/deque.hpp"
#include "compiler/env/Region.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"

//TODO: can we inherit instead of encapsulating?
//TODO: use AbsValue instead of VPConstraint
class AbsVarArrayStatic 
   {
   public:
   AbsVarArrayStatic(TR::Region &region, unsigned int maxSize);
   AbsVarArrayStatic(const AbsVarArrayStatic&, TR::Region &);
   AbsVarArrayStatic(const AbsVarArrayStatic&) = delete; // we need TR::Region
   void merge(const AbsVarArrayStatic&, TR::Region &, OMR::ValuePropagation *);
   void trace(OMR::ValuePropagation *vp);
   void set(unsigned int, AbsValue*);
   size_t size();
   AbsValue *at(unsigned int);

   private:
   typedef TR::deque<AbsValue*, TR::Region&> AbsValueArray;
   AbsValueArray _array;
   };

#endif