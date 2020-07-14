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
   IDTBuilder(TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Region& stackRegion, TR::Region& idtRegion, TR::Compilation* comp, OMR::BenefitInliner* inliner);
   IDT* buildIDT();

   private:
   bool buildIDTHelper(IDTNode* node, AbsState* invokeState, int callerIndex, int32_t budget, TR_CallStack* callStack);
   void performAbstractInterpretation(IDTNode* node, AbsState* invokeState,  int callerIndex, TR_CallStack* callStack);
   float computeCallRatio(TR_CallTarget* callTarget, TR_CallStack* callStack, TR::Block* block, TR::CFG* callerCfg );

   TR::SymbolReference* getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind);
   TR::Compilation* comp() {  return _comp;  };
   TR::Region& getIdtRegion() {  return _idtRegion;  };
   TR::Region& getStackRegion() {  return _stackRegion;  };

   IDTNode* getInterpretedMethod(TR::ResolvedMethodSymbol* symbol);
   void addInterpretedMethod(TR::ResolvedMethodSymbol *symbol, IDTNode* node);

   //Why do we need an Inliner here? Because we need its applyPolicyToTargets()
   OMR::BenefitInliner* getInliner()  {  return _inliner;  };
   
   //Used for calculating method branch profile
   OMR::BenefitInlinerUtil* getUtil();
   TR::CFG* generateCFG(TR_CallTarget* callTarget, TR_CallStack* callStack=NULL);
   TR::ValuePropagation *getValuePropagation();

   void addChild(IDTNode*node,
      int callerIndex,
      TR_ResolvedMethod* containingMethod, 
      AbsState* invokeState,
      int bcIndex, 
      int cpIndex, 
      TR::MethodSymbol::Kinds kind, 
      TR_CallStack* callStack,
      TR::Block* block, 
      TR::CFG* cfg);

   TR_CallSite* findCallSiteTargets(
      TR::ResolvedMethodSymbol *callerSymbol, 
      int bcIndex, 
      int cpIndex, 
      TR::MethodSymbol::Kinds kind, 
      int callerIndex, 
      TR_CallStack* callStack,
      TR::Block* block,
      TR::CFG* cfg);

   TR_CallSite* getCallSite(
      TR::MethodSymbol::Kinds kind,
      TR_ResolvedMethod *callerResolvedMethod,
      TR::TreeTop *callNodeTreeTop,
      TR::Node *parent,
      TR::Node *callNode,
      TR::Method * interfaceMethod,
      TR_OpaqueClassBlock *receiverClass,
      int32_t vftSlot,
      int32_t cpIndex,
      TR_ResolvedMethod *initialCalleeMethod,
      TR::ResolvedMethodSymbol * initialCalleeSymbol,
      bool isIndirectCall,
      bool isInterface,
      TR_ByteCodeInfo & bcInfo,
      TR::Compilation *comp,
      int32_t depth=-1,
      bool allConsts=false,
      TR::SymbolReference *symRef=NULL);

   IDT* _idt;
   TR_J9EstimateCodeSize * _cfgGen;
   TR::ResolvedMethodSymbol* _rootSymbol;

   typedef TR::typed_allocator<std::pair<TR_OpaqueMethodBlock*, IDTNode*>, TR::Region&> InterpretedMethodMapAllocator;
   typedef std::less<TR_OpaqueMethodBlock*> InterpretedMethodMapComparator;
   typedef std::map<TR_OpaqueMethodBlock *, IDTNode*, InterpretedMethodMapComparator, InterpretedMethodMapAllocator> InterpretedMethodMap;
   InterpretedMethodMap _interpretedMethodMap;

   int _callSiteIndex;
   int32_t _rootBudget;
   TR::Region& _idtRegion;
   TR::Region& _stackRegion;
   TR::Compilation* _comp;
   TR::ValuePropagation *_valuePropagation;
   OMR::BenefitInliner* _inliner;
   OMR::BenefitInlinerUtil* _util;
   };

#endif