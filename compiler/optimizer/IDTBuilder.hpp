#ifndef IDT_BUILDER_INCL
#define IDT_BUILDER_INCL

#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "optimizer/IDT.hpp"
#include "env/Region.hpp"
#include "optimizer/J9CallGraph.hpp"
#include "compiler/optimizer/BenefitInliner.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"

typedef struct InvokeByteCodeInfo
{
   int bcIndex;
   int cpIndex;
   TR::MethodSymbol::Kinds kind;
} InvokeByteCodeInfo;

class IDTBuilder
   {
   public:
   IDTBuilder(TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Region& region, TR::Compilation* comp, OMR::BenefitInliner* inliner);
   void buildIDT();
   IDT* getIDT() const;
   OMR::BenefitInliner* getInliner() const;

   private:
   void buildIDTHelper(IDTNode* node, int32_t budget,TR_CallStack* callStack);
   bool isInvocation(TR_J9ByteCode bc, TR_J9ByteCodeIterator bcIterator, InvokeByteCodeInfo &info);
   // TR_CallSite* findCallSiteTargets(TR::ResolvedMethodSymbol *callerSymbol, int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind, int callerIndex, TR_CallStack* callStack);
   // TR_CallSite* getCallSite(TR::MethodSymbol::Kinds kind,
   //                            TR_ResolvedMethod *callerResolvedMethod,
   //                            TR::TreeTop *callNodeTreeTop,
   //                            TR::Node *parent,
   //                            TR::Node *callNode,
   //                            TR::Method * interfaceMethod,
   //                            TR_OpaqueClassBlock *receiverClass,
   //                            int32_t vftSlot,
   //                            int32_t cpIndex,
   //                            TR_ResolvedMethod *initialCalleeMethod,
   //                            TR::ResolvedMethodSymbol * initialCalleeSymbol,
   //                            bool isIndirectCall,
   //                            bool isInterface,
   //                            TR_ByteCodeInfo & bcInfo,
   //                            TR::Compilation *comp,
   //                            int32_t depth=-1,
   //                            bool allConsts=false,
   //                            TR::SymbolReference *symRef=NULL);
   TR::SymbolReference* getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind);
   TR::Compilation* comp() const;
   TR::Region& getRegion() const;
   TR::CFG* generateCFG(TR_CallTarget* callTarget);
   TR::ValuePropagation *getValuePropagation();

   TR_J9EstimateCodeSize * _cfgGen;
   TR::ResolvedMethodSymbol* _rootSymbol;
   int32_t _rootBudget;
   IDT* _idt;
   TR::Region& _region;
   TR::Compilation* _comp;
   TR::ValuePropagation *_valuePropagation;
   OMR::BenefitInliner* _inliner;
   };

#endif