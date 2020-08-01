#ifndef TR_BENEFIT_INLINER
#define TR_BENEFIT_INLINER
#pragma once

#include "infra/Cfg.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/J9Inliner.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/IDTNode.hpp"
#include "optimizer/IDT.hpp"
#include "optimizer/MethodSummary.hpp"
#include "compiler/ilgen/J9ByteCodeIterator.hpp"
#include <deque>



class InliningProposal;
class IDTBuilder;

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
            return "O^O Benefit Inliner Wrapper: ";
         }
      int32_t getBudget(TR::ResolvedMethodSymbol *resolvedMethodSymbol);
   };



class BenefitInlinerBase: public TR_InlinerBase 
   {
   protected:
      
      virtual inline void updateBenefitInliner();
      virtual inline void popBenefitInlinerInformation();
      virtual bool analyzeCallSite(TR_CallStack *, TR::TreeTop *, TR::Node *, TR::Node *, TR_CallTarget*);
      BenefitInlinerBase(TR::Optimizer *optimizer, TR::Optimization *optimization);
      TR::Region _cfgRegion;
   public:
      int level = 0;
      virtual bool supportsMultipleTargetInlining () { return false; }
      virtual void applyPolicyToTargets(TR_CallStack *, TR_CallSite *);
      int _callerIndex;
      int _nodes;
      //std::deque<int> inlinedNodes;
      //void performInlining(TR::ResolvedMethodSymbol *);
protected:
      virtual bool inlineCallTargets(TR::ResolvedMethodSymbol *, TR_CallStack *, TR_InnerPreexistenceInfo *info);
      bool usedSavedInformation(TR::ResolvedMethodSymbol *, TR_CallStack *, TR_InnerPreexistenceInfo *info, IDTNode *idtNode);
      IDT *_idt;
      IDTNode *_currentNode;
      IDTNode *_previousNode;
      IDTNode *_currentChild;
      InliningProposal *_inliningProposal;
   private:
   };


class BenefitInliner: public BenefitInlinerBase 
   {
   public:
   friend class BenefitInlinerWrapper;
      void addEverything();
      void addEverythingRecursively(IDTNode*);
      void buildIDT();
      
      BenefitInliner(TR::Optimizer *, TR::Optimization *, uint32_t);
      void inlinerPacking();
      
      
      TR::Region _holdingProposalRegion;
      inline const uint32_t budget() const { return this->_budget; }
   private:
      TR::Region _idtRegion;
      TR::CFG *_rootRms;

      const uint32_t _budget;

     
   };


}

#endif
