#ifndef IDTNODE_INCL
#define IDTNODE_INCL

#include <stdint.h>
#include "env/Region.hpp"
#include "env/TRMemory.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "infra/deque.hpp"
#include "compile/Compilation.hpp"
#include "compiler/ilgen/J9ByteCodeIterator.hpp"
#include <queue>

class MethodSummary;
class IDTNode;

typedef TR::deque<IDTNode*, TR::Region&> IDTNodeDeque;
typedef TR::vector<IDTNode*, TR::Region&> IDTNodePtrVector;

typedef std::stack<IDTNode*, IDTNodeDeque> IDTNodeStack;
struct IDTNodePtrOrder {
   bool operator()(IDTNode *left, IDTNode *right);
};
typedef std::priority_queue<IDTNode*, IDTNodePtrVector, IDTNodePtrOrder> IDTNodePtrPriorityQueue;
typedef TR::deque<IDTNode*, TR::Region&> IDTNodeChildren;

class IDTNode
   {
   public:
   IDTNode(
      int idx,
      TR::MethodSymbol::Kinds kind,
      TR_CallTarget* callTarget,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol*,
      float callRatio,
      IDTNode *parent,
      int budget);

   IDTNode* addChild(
      int idx,
      TR::MethodSymbol::Kinds kind,
      TR_CallTarget* callTarget,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol*,  
      float callRatio, 
      TR::Region& region);

   TR::MethodSymbol::Kinds getMethodKind() {  return _kind;  };
   MethodSummary* getMethodSummary() {  return _methodSummary;  };
   void setMethodSummary(MethodSummary* methodSummary) {  _methodSummary = methodSummary;  };

   unsigned int getNumDescendants();
   unsigned int getNumDescendantsIncludingMe() {  return 1 + getNumDescendants();  };

   const char* getName(TR_Memory* mem) {  return _symbol->getResolvedMethod()->signature(mem);  };
   const char* getName() {  return _symbol->getResolvedMethod()->signature(TR::comp()->trMemory()); };

   IDTNode *getParent() {  return _parent;  };

   int getGlobalIndex() {  return _idx;  };
   int getParentGloablIndex()  {  return isRoot() ? -2 : getParent()->getGlobalIndex(); };

   unsigned int getBenefit() {  return _rootCallRatio * ( 1 + _staticBenefit);  };
   unsigned int getStaticBenefit() {  return _staticBenefit;  };
   void setStaticBenefit(unsigned int benefit) {  _staticBenefit = benefit;  };
   
   unsigned int getCost() {  return isRoot() ? 1 : getByteCodeSize();  };
   unsigned int getRecursiveCost();

   unsigned int getNumChildren();
   IDTNode *getChild(unsigned int);
   bool isRoot()  {  return _parent == NULL;  };
   IDTNode* findChildWithBytecodeIndex(int bcIndex);

   TR::ResolvedMethodSymbol* getResolvedMethodSymbol() {  return !getCallTarget() ? _symbol : getCallTarget()->getSymbol();  };
   
   int getBudget()  {  return _budget;  };

   void printTrace();

   TR_CallTarget *getCallTarget() {  return _callTarget;  };
   unsigned int getByteCodeIndex() {  return _callSiteBci;  };
   uint32_t getByteCodeSize() {   return getCallTarget()->_calleeMethod->maxBytecodeIndex();  };

   float getCallRatio() {  return _callRatio; };
   float getRootCallRatio() {  return _rootCallRatio; };

   private:
   TR_CallTarget *_callTarget;
   IDTNode *_parent;
   TR::MethodSymbol::Kinds _kind;

   int _idx;
   int _callSiteBci;
   
   IDTNodeChildren* _children; // NULL if 0, (IDTNode* & 1) if 1, otherwise a deque*
   unsigned int _staticBenefit;
   TR::ResolvedMethodSymbol* _symbol;
   int _budget;
   float _callRatio;
   float _rootCallRatio;
   MethodSummary *_methodSummary;

   bool isNodeSimilar(int32_t callSiteBci, TR::ResolvedMethodSymbol* rms);
   
   IDTNode* getOnlyChild(); // Returns NULL if 0 or > 1 children
   void setOnlyChild(IDTNode* child);
   };
    
#endif