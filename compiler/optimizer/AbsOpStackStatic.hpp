#ifndef ABS_OP_STACK_STATIC_INCL
#define ABS_OP_STACK_STATIC_INCL

#include "env/Region.hpp"
#include "compiler/infra/ReferenceWrapper.hpp"
#include "compiler/infra/deque.hpp"
#include "compiler/env/Region.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"

//TODO: can we inherit instead of encapsulating?
//TODO: use AbsValue instead of VPConstraint
class AbsOpStackStatic
   {
   public:
   AbsOpStackStatic(TR::Region& region, unsigned int maxSize);
   AbsOpStackStatic(const AbsOpStackStatic&, TR::Region &);
   AbsOpStackStatic(const AbsOpStackStatic&) = delete; // we need a region...

   void merge(const AbsOpStackStatic &, TR::Region &, OMR::ValuePropagation *);
   void pop();
   void trace(OMR::ValuePropagation *, TR::Region &region);
   void push(AbsValue*);
   bool empty();
   size_t size();
   AbsValue* top();

   private:
   typedef TR::deque<AbsValue*, TR::Region&> StackContainer;
   typedef std::stack<AbsValue*, StackContainer> AbsValueStack;
   
   AbsValueStack _stack; 

   };

#endif

