#ifndef IDT_INCL
#define IDT_INCL

#include "compile/Compilation.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "optimizer/IDTNode.hpp"
#include "infra/deque.hpp"
#include "env/Region.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include <queue>

class IDT
   {
   public:
   IDT(TR::Region& region, TR::ResolvedMethodSymbol*, TR_CallTarget*, int budget, TR::Compilation* comp);

   IDTNode* getRoot() { return _root; };

   TR::Compilation* comp() { return _comp; };
   TR::Region& getMemoryRegion() { return _region; };
   
   unsigned int getNumNodes() { return _maxIdx + 1; };
   void copyDescendants(IDTNode* fromNode, IDTNode* toNode);
   
   int getNextGlobalIDTNodeIndex() { return _maxIdx; };
   void increaseGlobalIDTNodeIndex()  { _maxIdx ++; };

   IDTNode *getNodeByGlobalIndex(int index);
   void buildIndices();
   void printTrace();

   private:
   TR::Compilation *_comp;
   TR::Region&  _region;
   int _maxIdx;
   IDTNode* _root;
   IDTNode** _indices;
   };


class IDTPreorderPriorityQueue
   {
   public:
   IDTPreorderPriorityQueue(IDT* idt, TR::Region& region);
   unsigned int size() { return _idt->getNumNodes(); }

   IDTNode* get(unsigned int index);

   private:

   struct IDTNodeCompare {
      bool operator()(IDTNode *left, IDTNode *right)
         {
         TR_ASSERT_FATAL(left && right, "Comparing against null");
         return left->getBenefit() < right->getBenefit() || left->getCost() < right->getCost();
         };
   };

   typedef TR::vector<IDTNode*, TR::Region&> IDTNodeVector;
   typedef std::priority_queue<IDTNode*, IDTNodeVector, IDTNodeCompare> IDTNodePriorityQueue;
   
   IDT* _idt;
   IDTNodePriorityQueue _pQueue;
   IDTNodeVector _entries;
   };
#endif
