#ifndef INLININGPROPOSAL_INCL
#define INLININGPROPOSAL_INCL

#include <functional>
#include <unordered_set>
#include <iostream>
#include <string>
#include "env/Region.hpp"
#include "infra/List.hpp" 
#include "optimizer/IDT.hpp"
#include "optimizer/IDTNode.hpp"
#include "infra/BitVector.hpp" 
#include "compile/Compilation.hpp"

class InliningProposal
   {
   public:
   InliningProposal(TR::Region& region, IDT *idt);
   InliningProposal(InliningProposal&, TR::Region& region);
   void print(TR::Compilation *comp);
   bool isEmpty() const;
   void clear();
   int getCost();
   int getBenefit();
   void pushBack(IDTNode *);
   bool inSet(IDTNode* );
   bool inSet(int calleeIndex);
   void intersectInPlace(InliningProposal &, InliningProposal&);
   void unionInPlace(InliningProposal &, InliningProposal&);
   bool overlaps(InliningProposal *p);

   private:
   InliningProposal() = delete;
   InliningProposal(const InliningProposal&) = delete;
   InliningProposal & operator=(const InliningProposal&) = delete;
   void computeCostAndBenefit();
   void ensureBitVectorInitialized();
   TR::Region& _region;
   TR_BitVector *_nodes;
   int _cost;
   int _benefit;
   IDT *_idt;
};

#endif