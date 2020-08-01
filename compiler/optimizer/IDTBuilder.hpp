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
   void buildIDTHelper(IDTNode* node, AbsParameterArray* parameterArray, int callerIndex, int32_t budget, TR_CallStack* callStack);

   TR::CFG* generateFlowGraph(TR_CallTarget* callTarget, TR::Region& region, TR_CallStack* callStack=NULL);
   
   void performAbstractInterpretation(IDTNode* node,  int callerIndex, TR_CallStack* callStack);

   float computeCallRatio(TR::Block* block, TR::CFG* callerCfg );

   //call this after we have the method summary of the method / or pass method summary as NULL to clean the invoke state
   int computeStaticBenefitWithMethodSummary(MethodSummary* methodSummary, AbsParameterArray* parameterArray);

   TR::Compilation* comp() { return _comp; };
   TR::Region& region() { return _region; };

   //Check if the method has already been interpeted thus no need to perform abstract interpretation again.
   IDTNode* getInterpretedMethod(TR::ResolvedMethodSymbol* symbol);
   void addInterpretedMethod(TR::ResolvedMethodSymbol *symbol, IDTNode* node);

   OMR::BenefitInliner* getInliner()  { return _inliner; };
   

   //This will be called from AbsInterpreter
   void addChild(IDTNode*node,
      int callerIndex,
      TR_CallSite* callSite,
      AbsParameterArray* parameterArray,
      TR_CallStack* callStack,
      TR::Block* block);

   IDT* _idt;
   TR::ResolvedMethodSymbol* _rootSymbol;

   typedef TR::typed_allocator<std::pair<TR_OpaqueMethodBlock*, IDTNode*>, TR::Region&> InterpretedMethodMapAllocator;
   typedef std::less<TR_OpaqueMethodBlock*> InterpretedMethodMapComparator;
   typedef std::map<TR_OpaqueMethodBlock *, IDTNode*, InterpretedMethodMapComparator, InterpretedMethodMapAllocator> InterpretedMethodMap;
   InterpretedMethodMap _interpretedMethodMap;

   int32_t _rootBudget;
   TR::Region& _region;
   TR::Compilation* _comp;
   OMR::BenefitInliner* _inliner;
   };

#endif