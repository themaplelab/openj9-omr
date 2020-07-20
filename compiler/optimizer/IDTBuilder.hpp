#ifndef IDT_BUILDER_INCL
#define IDT_BUILDER_INCL

#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "optimizer/IDT.hpp"
#include "env/Region.hpp"
#include "optimizer/J9CallGraph.hpp"
#include "optimizer/BenefitInliner.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"
#include <map>
#include "optimizer/AbsState.hpp"

class IDTBuilder
   {
   friend class AbsInterpreter;

   public:
   IDTBuilder(TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Region& region, TR::Compilation* comp, OMR::BenefitInliner* inliner);
   IDT* buildIDT();

   private:
   void buildIDTHelper(IDTNode* node, AbsState* invokeState, int callerIndex, int32_t budget, TR_CallStack* callStack);
   void performAbstractInterpretation(IDTNode* node,  int callerIndex, TR_CallStack* callStack);

   float computeCallRatio(TR_CallTarget* callTarget, TR_CallStack* callStack, TR::Block* block, TR::CFG* callerCfg );
   int computeStaticBenefitWithMethodSummary(TR::Method* calleeMethod, bool isStaticMethod, MethodSummary* methodSummary, AbsState* invokeState);

   //Any method that is not going to be asbtract interpreted should call this method to pop the parameters from AbsState
   //In order to ensure the Abs Op Stack is valid.
   void cleanInvokeState(TR::Method* calleeMethod, bool isStaticMethod, AbsState* invokeState);

   TR::Compilation* comp() { return _comp; };
   TR::Region& region() { return _region; };


   IDTNode* getInterpretedMethod(TR::ResolvedMethodSymbol* symbol);
   void addInterpretedMethod(TR::ResolvedMethodSymbol *symbol, IDTNode* node);

   //Why do we need an Inliner here? Because we need its applyPolicyToTargets()
   OMR::BenefitInliner* getInliner()  { return _inliner; };
   
   //Used for calculating method branch profile
   OMR::BenefitInlinerUtil* getUtil();

   //TODO: move to absinterpter
   TR::CFG* generateCFG(TR_CallTarget* callTarget, TR_CallStack* callStack=NULL);
   TR::ValuePropagation *getValuePropagation();


   //This will be called from AbsInterpreter
   void addChild(IDTNode*node,
      int callerIndex,
      TR::Method* calleeMethod, 
      bool isStaticMethod,
      TR_CallSite* callSite,
      AbsState* invokeState,
      TR_CallStack* callStack,
      TR::Block* block);

   IDT* _idt;
   TR_J9EstimateCodeSize * _cfgGen;
   TR::ResolvedMethodSymbol* _rootSymbol;

   typedef TR::typed_allocator<std::pair<TR_OpaqueMethodBlock*, IDTNode*>, TR::Region&> InterpretedMethodMapAllocator;
   typedef std::less<TR_OpaqueMethodBlock*> InterpretedMethodMapComparator;
   typedef std::map<TR_OpaqueMethodBlock *, IDTNode*, InterpretedMethodMapComparator, InterpretedMethodMapAllocator> InterpretedMethodMap;
   InterpretedMethodMap _interpretedMethodMap;

   int _callSiteIndex;
   int32_t _rootBudget;
   TR::Region& _region;
   TR::Compilation* _comp;
   TR::ValuePropagation *_valuePropagation;
   OMR::BenefitInliner* _inliner;
   OMR::BenefitInlinerUtil* _util;
   };

#endif