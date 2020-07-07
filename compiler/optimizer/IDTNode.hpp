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
#include "compiler/optimizer/AbsState.hpp"
#include "compiler/ilgen/J9ByteCodeIterator.hpp"

#include <queue>

class MethodSummaryExtension;
class IDTNode;

typedef TR::deque<IDTNode*, TR::Region&> IDTNodeIndices;
typedef TR::deque<IDTNode*, TR::Region&> IDTNodeDeque;
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
      TR::MethodSymbol::Kinds kind,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* rms,
      IDTNode *parent,
      int unsigned benefit,
      int budget,
      TR_CallSite* callSite,
      float callRatio
      );

   IDTNode* addChildIfNotExists(
      int idx,
      TR::MethodSymbol::Kinds kind,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* rms, 
      unsigned int benefit, 
      TR_CallSite* CallSite, 
      float callRatioCallerCallee, 
      TR::Region& region);

   void setInvocationAbsState(AbsState* absState);
   
   AbsState* getInvocationAbsState();
   TR::MethodSymbol::Kinds getMethodKind();
   MethodSummaryExtension* getMethodSummary();
   void setMethodSummary(MethodSummaryExtension* methodSummary);
   unsigned int getNumDescendants();
   unsigned int getNumDescendantsIncludingMe() ;
   const char* getName(TR_Memory* mem);
   const char* getName();
   IDTNode *getParent();
   int getCalleeIndex();
   unsigned int getCost();
   unsigned int getRecursiveCost();
   unsigned int getBenefit();
   unsigned int getStaticBenefit();
   void setBenefit(unsigned int);
   void enqueueSubordinates(IDTNodePtrPriorityQueue *q);
   unsigned int getNumChildren();
   IDTNode *getChild(unsigned int);
   bool isRoot();
   IDTNode* findChildWithBytecodeIndex(int bcIndex);
   bool isSameMethod(IDTNode* aNode);
   bool isSameMethod(TR::ResolvedMethodSymbol *);
   unsigned int numberOfParameters();
   TR::ResolvedMethodSymbol* getResolvedMethodSymbol();
   void setResolvedMethodSymbol(TR::ResolvedMethodSymbol* rms);
   int getCallerIndex();
   int getBudget();
   void printTrace();
   //TODO: maybe get rid of this
   void setCallTarget(TR_CallTarget* callTarget);
   TR_CallTarget *getCallTarget();
   void setCallStack(TR_CallStack* callStack);
   TR_CallStack* getCallStack();
   unsigned int getByteCodeIndex();
   uint32_t getByteCodeSize();
   TR_CallSite* getCallSite();
   float getCallRatio();
   float getRootCallRatio();
   TR::ValuePropagation* getValuePropagation();

   private:
   TR_CallStack *_callStack;
   TR_CallTarget *_callTarget;
   AbsState* _invocationAbsState;
   TR_CallSite *_callSite;
   IDTNode *_parent;
   TR::MethodSymbol::Kinds _kind;
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

   bool isNodeSimilar(int32_t callSiteBci, TR::ResolvedMethodSymbol* rms);
   // Returns NULL if 0 or > 1 children
   IDTNode* getOnlyChild() ;
   void setOnlyChild(IDTNode* child);
   };
    
#endif