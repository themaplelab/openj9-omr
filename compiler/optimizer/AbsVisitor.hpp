#ifndef ABS_VISTOR_INCL
#define ABS_VISITOR_INCL

#include "optimizer/CallInfo.hpp"
#include "optimizer/AbsState.hpp"

class AbsVisitor
   {
   public:
   virtual void visit(TR_CallSite* callSite, AbsArguments* arguments)=0;
   };

#endif