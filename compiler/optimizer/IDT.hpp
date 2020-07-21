#ifndef IDT_INCL
#define IDT_INCL

#include "compile/Compilation.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "optimizer/IDTNode.hpp"
#include "infra/deque.hpp"
#include "env/Region.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"

class IDT
   {
   public:
   IDT(TR::Region& region, TR::ResolvedMethodSymbol*, TR_CallTarget*, int budget, TR::Compilation* comp);

   IDTNode* getRoot() { return _root; };

   uint32_t getCost() { return _totalCost; }
   void addCost(uint32_t cost) { _totalCost = _totalCost + cost; }

   TR::Compilation* comp() { return _comp; };
   TR::Region& getMemoryRegion() { return _region; };
   
   unsigned int getNumNodes() { return _maxIdx + 1; };
   void copyDescendants(IDTNode* fromNode, IDTNode*toNode);
   
   int getNextGlobalIDTNodeIndex() { return _maxIdx; };
   void increaseGlobalIDTNodeIndex()  { _maxIdx ++; };

   IDTNode *getNodeByGlobalIndex(int index);
   void buildIndices();
   void printTrace();

   private:
   uint32_t _totalCost;
   TR::Compilation *_comp;
   TR::Region&  _region;
   int _maxIdx;
   IDTNode* _root;
   IDTNode** _indices;
   };
#endif
