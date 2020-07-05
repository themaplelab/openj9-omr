#ifndef IDT_BUILDER_INCL
#define IDT_BUILDER_INCL

#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "optimizer/IDT.hpp"
#include "env/Region.hpp"
#include "optimizer/J9CallGraph.hpp"
#include "optimizer/BenefitInliner.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"

class IDTBuilder
   {
   friend class AbsInterpreter;

   public:
   IDTBuilder(TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Region& region, TR::Compilation* comp, OMR::BenefitInliner* inliner);
   void buildIDT();
   IDT* getIDT();
   
   private:
   void buildIDTHelper(IDTNode* node, int32_t budget,TR_CallStack* callStack);
   void performAbstractInterpretation(IDTNode* node, TR_CallStack* callStack, IDTNodeDeque& idtNodeChildren);
   void computeCallRatio(TR_CallSite* callsite, TR_CallStack* callStack, int callerIndex, TR::Block* block, TR::CFG* callerCfg );

   void addChildren(IDTNode*node,
      TR_ResolvedMethod*method, 
      int bcIndex, 
      int cpIndex, 
      TR::MethodSymbol::Kinds kind, 
      TR_CallStack* callStack, 
      IDTNodeDeque& idtNodeChildren, 
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

   TR::SymbolReference* getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind);
   TR::Compilation* comp();
   TR::Region& getRegion();

   //Why do we need an Inliner here? Because we need its applyPolicyToTargets()
   OMR::BenefitInliner* getInliner();
   //Used for calculating method branch profile
   OMR::BenefitInlinerUtil* getUtil();
   TR::CFG* generateCFG(TR_CallTarget* callTarget, TR_CallStack* callStack=NULL);
   TR::ValuePropagation *getValuePropagation();

   TR_J9EstimateCodeSize * _cfgGen;
   TR::ResolvedMethodSymbol* _rootSymbol;
   int _callerIndex;
   int _callSiteIndex;
   int32_t _rootBudget;
   IDT* _idt;
   TR::Region& _region;
   TR::Compilation* _comp;
   TR::ValuePropagation *_valuePropagation;
   OMR::BenefitInliner* _inliner;
   OMR::BenefitInlinerUtil* _util;
   };

#endif