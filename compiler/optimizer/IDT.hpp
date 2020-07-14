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

   IDTNode* getRoot() {  return _root;  };
   TR::Compilation* comp() {  return _comp;  };
   unsigned int getNumNodes() {  return getRoot()->getNumDescendantsIncludingMe();  };
   void copyDescendants(IDTNode* fromNode, IDTNode*toNode);
   TR::Region& getMemoryRegion() { return _region; };
   int getNextGlobalIDTNodeIndex() {  return _maxIdx;  };
   void increaseGlobalIDTNodeIndex()  {  _maxIdx ++;  };

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
#endif
