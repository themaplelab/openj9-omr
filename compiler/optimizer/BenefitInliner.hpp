#ifndef TR_BENEFIT_INLINER
#define TR_BENEFIT_INLINER
#pragma once

#include "infra/Cfg.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/J9Inliner.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "IDT.hpp"
#include "optimizer/MethodSummary.hpp"


class InliningProposal;

namespace OMR {

   class BenefitInlinerWrapper : public TR::Optimization
   {
   public:
      BenefitInlinerWrapper(TR::OptimizationManager *manager) : TR::Optimization(manager) {};
      static TR::Optimization *create(TR::OptimizationManager *manager)
         {
            return new (manager->allocator()) BenefitInlinerWrapper(manager);
         }

      virtual int32_t perform();
      virtual const char * optDetailString() const throw()
         {
            return "O^O Beneft Inliner Wrapper: ";
         }
      int32_t getBudget(TR::ResolvedMethodSymbol *resolvedMethodSymbol);
   };


class AbsEnvInlinerUtil;
class BenefitInlinerBase: public TR_InlinerBase 
   {
   protected:
      void debugTrees(TR::ResolvedMethodSymbol*);
      void getSymbolAndFindInlineTargets(TR_CallStack *callStack, TR_CallSite *callsite, bool findNewTargets=true);
      virtual bool supportsMultipleTargetInlining () { return false; }
      virtual inline void updateBenefitInliner();
      virtual inline void popBenefitInlinerInformation();
      virtual bool analyzeCallSite(TR_CallStack *, TR::TreeTop *, TR::Node *, TR::Node *);
      BenefitInlinerBase(TR::Optimizer *optimizer, TR::Optimization *optimization);
      TR::Region _cfgRegion;
   public:
      virtual void applyPolicyToTargets(TR_CallStack *, TR_CallSite *, TR::Block *block=NULL);
      int _callerIndex;
      int _nodes;
      //void performInlining(TR::ResolvedMethodSymbol *);
protected:
      virtual bool inlineCallTargets(TR::ResolvedMethodSymbol *, TR_CallStack *, TR_InnerPreexistenceInfo *info);
      bool usedSavedInformation(TR::ResolvedMethodSymbol *, TR_CallStack *, TR_InnerPreexistenceInfo *info, IDT::Node *idtNode);
      IDT *_idt;
      IDT::Node *_currentNode;
      IDT::Node *_previousNode;
      IDT::Node *_currentChild;
      InliningProposal *_inliningProposal;
   private:
      AbsEnvInlinerUtil *_util2;
      void setAbsEnvUtil(AbsEnvInlinerUtil *u) { this->_util2 = u; }
      AbsEnvInlinerUtil *getAbsEnvUtil() { return this->_util2; }
   };


class BenefitInliner: public BenefitInlinerBase 
   {
   public:
   friend class BenefitInlinerWrapper;
      void addEverything();
      BenefitInliner(TR::Optimizer *, TR::Optimization *, uint32_t);
      void initIDT(TR::ResolvedMethodSymbol *root, int);
      void analyzeIDT();
      void abstractInterpreter();
      void obtainIDT(IDT::Node *node, int32_t budget);
      
      bool obtainIDT(IDT::Indices&, IDT::Node*, int32_t budget);
      void obtainIDT(IDT::Indices&, IDT::Node*, TR_J9ByteCodeIterator&, TR::Block *, int32_t);
      void obtainIDT(IDT::Indices*, IDT::Node *, TR_CallSite*, int32_t budget, int cpIndex);
      void traceIDT();
      TR::Region _holdingProposalRegion;
      inline const uint32_t budget() const { return this->_budget; }
   private:
      typedef TR::typed_allocator<std::pair<TR_OpaqueMethodBlock*, IDT::Node*>, TR::Region&> MethodSummaryMapAllocator;
      typedef std::less<TR_OpaqueMethodBlock*> MethodSummaryMapComparator;
      typedef std::map<TR_OpaqueMethodBlock *, IDT::Node*, MethodSummaryMapComparator, MethodSummaryMapAllocator> MethodSummaryMap;

      TR::CFG *_rootRms;
      TR::Region _absEnvRegion;
      TR::Region _callSitesRegion;
      TR::Region _callStacksRegion;
      TR::Region _mapRegion;
      TR::Region _IDTConstructorRegion;
      MethodSummaryMap _methodSummaryMap;
      TR_CallStack *_inliningCallStack;
      const uint32_t _budget;

      TR_CallSite *findCallSiteTarget(TR::ResolvedMethodSymbol *resolvedMethodSymbol, int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind, TR::Block *block);
      void printTargets(TR::ResolvedMethodSymbol *resolvedMethodSymbol, int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind);
      void printInterfaceTargets(TR::ResolvedMethodSymbol *resolvedMethodSymbol, int bcIndex, int cpIndex);
      void printVirtualTargets(TR::ResolvedMethodSymbol *resolvedMethodSymbol, int bcIndex, int cpIndex);
      void printStaticTargets(TR::ResolvedMethodSymbol *resolvedMethodSymbol, int bcIndex, int cpIndex);
      void printSpecialTargets(TR::ResolvedMethodSymbol *resolvedMethodSymbol, int bcIndex, int cpIndex);
      // TODO: delete me
      TR::SymbolReference* getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind);
      TR_CallSite * getCallSite(TR::MethodSymbol::Kinds kind,
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
                                    bool allConsts=false);
   };

   class AbsEnvInlinerUtil : public TR_J9InlinerUtil
   {
      friend class TR_InlinerBase;
      friend class TR_MultipleCallTargetInliner;
      friend class BenefitInliner;
      public:
      AbsEnvInlinerUtil(TR::Compilation *comp);
      void computeMethodBranchProfileInfo2(TR::Block *, TR_CallTarget *, TR::ResolvedMethodSymbol*, int, TR::Block *);
   };
}

#endif
