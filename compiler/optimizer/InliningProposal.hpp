#ifndef INLINING_PROPOSAL_INCL
#define INLINING_PROPOSAL_INCL

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
   void addNode(IDTNode *);
   bool inSet(IDTNode* );
   bool inSet(int calleeIndex);
   void intersectInPlace(InliningProposal &, InliningProposal&);
   void unionInPlace(InliningProposal &, InliningProposal&);
   bool overlaps(InliningProposal *p);
   bool intersects(InliningProposal* other);

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


class InliningProposalTable
   {
   public:
   InliningProposalTable(unsigned int rows, unsigned int cols, TR::Region& region);
   InliningProposal* get(unsigned int row, unsigned int col);
   void set(unsigned int row, unsigned int col, InliningProposal* proposal);
   TR::Region& region() { return _region; };
   InliningProposal* getEmptyProposal() { return new (region()) InliningProposal(region(), NULL); };
   
   private:
   unsigned int _rows;
   unsigned int _cols;
   TR::Region& _region;
   InliningProposal ***_table;
   };
#endif