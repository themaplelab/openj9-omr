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

class MethodSummaryExtension;
class IDTNode;

typedef TR::deque<IDTNode*, TR::Region&> IDTNodeIndices;
typedef TR::vector<IDTNode*, TR::Region&> IDTNodePtrVector;
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
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* rms,
      IDTNode *parent,
      int unsigned benefit,
      int budget,
      TR_CallSite* callSite,
      float callRatio,
      TR::ValuePropagation* vp
      );

   IDTNode* addChildIfNotExists(
      int idx,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* rms, 
      unsigned int benefit, 
      TR_CallSite* CallSite, 
      float callRatioCallerCallee, 
      TR::Region& region);

   
   MethodSummaryExtension* getMethodSummary() const;
   void setMethodSummary(MethodSummaryExtension* methodSummary);
   unsigned int getNumDescendants() const;
   unsigned int getNumDescendantsIncludingMe() const;
   const char* getName(TR_Memory* mem) const;
   const char* getName() const;
   IDTNode *getParent() const;
   int getCalleeIndex() const;
   unsigned int getCost() const;
   unsigned int getRecursiveCost() const;
   unsigned int getBenefit() const;
   unsigned int getStaticBenefit() const;
   void printByteCode() const;
   void setBenefit(unsigned int);
   void enqueueSubordinates(IDTNodePtrPriorityQueue *q) const;
   unsigned int getNumChildren() const;
   IDTNode *getChild(unsigned int) const;
   bool isRoot() const;
   IDTNode* findChildWithBytecodeIndex(int bcIndex) const;
   bool isSameMethod(IDTNode* aNode) const;
   bool isSameMethod(TR::ResolvedMethodSymbol *) const;
   unsigned int numberOfParameters();
   TR::ResolvedMethodSymbol* getResolvedMethodSymbol() const;
   void setResolvedMethodSymbol(TR::ResolvedMethodSymbol* rms);
   int getCallerIndex() const;
   int getBudget() const;
   void printTrace() const;
   //TODO: maybe get rid of this
   void setCallTarget(TR_CallTarget* callTarget);
   TR_CallTarget *getCallTarget() const;
   void setCallStack(TR_CallStack* callStack);
   TR_CallStack* getCallStack() const;
   unsigned int getByteCodeIndex() const;
   uint32_t getByteCodeSize() const;
   TR_CallSite* getCallSite() const;
   float getCallRatio() const;
   float getRootCallRatio() const;
   TR::ValuePropagation* getValuePropagation() const;

   private:
   TR_CallStack *_callStack;
   TR_CallTarget *_callTarget;
   TR_CallSite *_callSite;
   IDTNode *_parent;
   int _idx;
   int _callSiteBci;
   // NULL if 0, (IDTNode* & 1) if 1, otherwise a deque*
   IDTNodeChildren* _children;
   unsigned int _benefit;
   TR::ResolvedMethodSymbol* _rms;
   int _budget;
   float _callRatio;
   float _rootCallRatio;
   MethodSummaryExtension *_methodSummary;
   TR::ValuePropagation* _vp;
   bool isNodeSimilar(int32_t callSiteBci, TR::ResolvedMethodSymbol* rms) const;
   // Returns NULL if 0 or > 1 children
   IDTNode* getOnlyChild() const;
   void setOnlyChild(IDTNode* child);
   };
    
#endif