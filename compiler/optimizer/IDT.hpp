#ifndef IDT_INCL
#define IDT_INCL

#include "compile/Compilation.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "optimizer/IDTNode.hpp"
#include "infra/deque.hpp"
#include "env/Region.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"

typedef TR::deque<IDTNode, TR::Region&> BuildStack;

class IDT
   {
   public:
   IDT(TR::Region& region, TR::ResolvedMethodSymbol* rms, int budget, TR::Compilation* comp);
   int32_t getMaxDepth();
   void addCost(int32_t cost) {_cost += cost; };
   int32_t getCost() { return _cost; };
   IDTNode* getRoot() const;
   TR::Compilation* comp() const;
   unsigned int getNumNodes() const;
   void printTrace() const;
   IDTNode *getNodeByCalleeIndex(int calleeIndex);
   TR::ValuePropagation* getValuePropagation();
   void copyDescendants(IDTNode* fromNode, IDTNode*toNode);
   TR::Region& getMemoryRegion() const;
   int getNextGlobalIDTNodeIdx();
   void increaseGlobalIDTNodeIndex();
   void buildIndices();

   private:
   int32_t _maxDepth;
   int32_t _cost;
   TR::Compilation *_comp;
   TR::Region&  _region;
   TR::ValuePropagation *_vp;
   int _max_idx;
   IDTNode* _root;
   IDTNode** _indices;
   };
#endif
