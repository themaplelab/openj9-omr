#include "optimizer/IDTNode.hpp"

#define SINGLE_CHILD_BIT 1

bool IDTNodePtrOrder::operator()(IDTNode *left, IDTNode *right)
   {  
   TR_ASSERT_FATAL(left && right, "Comparing against null");
   return left->getCost() < right->getCost() || left->getBenefit() < right->getBenefit();
   }

IDTNode::IDTNode(
      int idx, 
      TR::MethodSymbol::Kinds kind,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* rms, 
      IDTNode *parent, 
      int unsigned benefit, 
      int budget, 
      TR_CallSite* callSite, 
      float callRatio):
   _idx(idx),
   _kind(kind),
   _benefit(benefit),
   _callSiteBci(callSiteBci),
   _children(NULL),
   _rms(rms),
   _parent(parent),
   _budget(budget),
   _callSite(callSite),
   _callTarget(NULL),
   _callStack(NULL),
   _callRatio(callRatio),
   _rootCallRatio(parent ? parent->_rootCallRatio * callRatio : 1),
   _methodSummary(NULL),
   _invocationAbsState(NULL)
   {   
   }

IDTNode* IDTNode::addChildIfNotExists(
      int idx,
      TR::MethodSymbol::Kinds kind,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* rms,
      unsigned int benefit, 
      TR_CallSite* callSite, 
      float callRatioCallerCallee,
      TR::Region& region)
   {
   // don't add things that we do are 1/25th as good as the root
   if (_rootCallRatio * callRatioCallerCallee * 100 < 25)
      return NULL;

   // The case where there is no children
   if (getNumChildren()==0)
      {
      IDTNode* newNode = new (region) IDTNode(
                           idx, 
                           kind,
                           callSiteBci, 
                           rms, 
                           this,
                           benefit, 
                           getBudget() - rms->getResolvedMethod()->maxBytecodeIndex(), 
                           callSite, 
                           callRatioCallerCallee);

      setOnlyChild(newNode);

      return newNode;
      }   
   // The case when there is 1 child
   else if (getNumChildren() == 1)
      {
      IDTNode* onlyChild = getOnlyChild();
      if (onlyChild && onlyChild->isNodeSimilar(callSiteBci, rms))
         return NULL;
      
      _children = new (region) IDTNodeChildren(region);
      TR_ASSERT_FATAL(!((uintptr_t)_children & SINGLE_CHILD_BIT), "Maligned memory address.\n");
      TR_ASSERT_FATAL(onlyChild, "Storing a null child\n");
      _children->push_back(onlyChild);
      }
   
   for (auto curr = _children->begin(); curr != _children->end(); ++curr)
      {
      TR_ASSERT_FATAL(*curr, "No child can be null\n");
      if ((*curr)->isNodeSimilar(callSiteBci, rms))
         {
         return NULL;
         }
      }

   IDTNode *newChild = new (region) IDTNode(
                        idx, 
                        kind,
                        callSiteBci, 
                        rms, 
                        this, 
                        benefit, 
                        getBudget() - rms->getResolvedMethod()->maxBytecodeIndex(), 
                        callSite, 
                        callRatioCallerCallee);
                        
   TR_ASSERT_FATAL(newChild, "Storing a null child\n");
   _children->push_back(newChild);
   return _children->back();
   }

unsigned int IDTNode::getNumDescendants()
   {
   unsigned int numChildren = getNumChildren();
   unsigned int sum = 0;
   for (unsigned int i =0; i < numChildren; i ++)
      {
      sum += 1 + getChild(i)->getNumDescendants(); 
      }
   return sum;
   }

void IDTNode::setInvocationAbsState(AbsState* absState)
   {
   _invocationAbsState = absState;
   }

TR::MethodSymbol::Kinds IDTNode::getMethodKind()
   {
   return _kind;
   }

AbsState* IDTNode::getInvocationAbsState()
   {
   TR_ASSERT_FATAL(_invocationAbsState, "Invocation Abstract State is NULL");
   return _invocationAbsState;
   }

unsigned int IDTNode::getNumDescendantsIncludingMe()
   {
   return 1 + getNumDescendants();
   }

const char* IDTNode::getName(TR_Memory* mem) 
   {
   return _rms->getResolvedMethod()->signature(mem);
   }

const char* IDTNode::getName()
   {
   //slow implementation TR::comp() is expensive but here we are in a context where we don't have access to TR_Memory
   return _rms->getResolvedMethod()->signature(TR::comp()->trMemory());
   }

IDTNode* IDTNode::getParent()
   {
   return _parent;
   }

int IDTNode::getCalleeIndex()
   {
   return _idx;
   }

unsigned int IDTNode::getCost()
   {
   return isRoot() ? 1 : getByteCodeSize();
   }

unsigned int IDTNode::getRecursiveCost() 
   {
   unsigned int numChildren = getNumChildren();
   unsigned int cost = getCost();
   for (unsigned int i = 0; i < numChildren; i ++)
      {
      IDTNode *child = getChild(i);
      cost += child->getRecursiveCost();
      }
   
   return cost;
   }

unsigned int IDTNode::getBenefit()
   {
   return _rootCallRatio * ( 1 + _benefit);
   }

unsigned int IDTNode::getStaticBenefit()
   {
   return _benefit;
   }

MethodSummaryExtension *IDTNode::getMethodSummary()  
   {
   return _methodSummary;
   }

void IDTNode::setMethodSummary(MethodSummaryExtension* methodSummary)
   {
   //TR_ASSERT_FATAL(methodSummary, "Setting a NULL methodsummary");
   _methodSummary = methodSummary;
   }

void IDTNode::setBenefit(unsigned int benefit)
   {
   _benefit = benefit;
   }

void IDTNode::enqueueSubordinates(IDTNodePtrPriorityQueue *q)
   {
   TR_ASSERT_FATAL(q, "Priority queue cannot be NULL!");
   unsigned int numChildren = getNumChildren();
   if (numChildren == 0)
      return;

   if (numChildren == 1) //The case where there is only one child
      {
      IDTNode* onlyChild = getOnlyChild();
      TR_ASSERT_FATAL(onlyChild, "Child cannot be NULL!");
      q->push(onlyChild);
      return;
      }
   
   for (unsigned int i = 0; i < numChildren; i ++)
      {
      IDTNode *child = _children->at(i);
      TR_ASSERT_FATAL(child, "Child cannot be NULL!");
      q->push(child);
      }
   }

unsigned int IDTNode::getNumChildren()
   {
   if (_children == NULL)
      return 0;
   
   if (getOnlyChild() != NULL)
      return 1;
   
   size_t num = _children->size();
   TR_ASSERT_FATAL(num > 1, "num cannot be 1 or 0!\n");
   return num;
   }

IDTNode* IDTNode::getChild(unsigned int index)
   {
   unsigned int numChildren = getNumChildren();
   
   if (numChildren == 0)
      return NULL;

   TR_ASSERT_FATAL(index < numChildren, "Child index out of range!\n");

   if (index == 0 && numChildren == 1) // only one child
      return getOnlyChild();

   return _children->at(index);
   }

bool IDTNode::isRoot()
   {
   return getParent() == NULL;
   }

IDTNode* IDTNode::findChildWithBytecodeIndex(int bcIndex)
   {
   traceMsg(TR::comp(), "%s:%d:%s\n", __FILE__, __LINE__, __func__);
   unsigned int size = getNumChildren();
   //TODO: not linear search
   if (size == 0)
      return NULL;
   
   if (size == 1)
      {
      IDTNode* onlyChild = getOnlyChild();
      return onlyChild->getByteCodeIndex() == bcIndex ? onlyChild : NULL;
      }
   
   IDTNode* child = NULL;
   for (unsigned int i = 0; i < size; i ++)
      {
      if (child && _children->at(i)->getByteCodeIndex() == bcIndex)
         TR_ASSERT_FATAL(false, "Two children with the same bytecode index!");
      
      if (_children->at(i)->getByteCodeIndex() == bcIndex)
         child = _children->at(i);
      }
   return child;
   }

bool IDTNode::isSameMethod(IDTNode* aNode)  
   {
   return isSameMethod(aNode->getResolvedMethodSymbol());
   }

bool IDTNode::isSameMethod(TR::ResolvedMethodSymbol* rms)
   {
   return getResolvedMethodSymbol()->getResolvedMethod()->isSameMethod(rms->getResolvedMethod());
   }

unsigned int IDTNode::numberOfParameters()
   {
   TR_ASSERT_FATAL(false, "Unimplemented!\n");
   return 0;
   }

TR::ResolvedMethodSymbol* IDTNode::getResolvedMethodSymbol()
   {
   return !this->getCallTarget() ? _rms : getCallTarget()->getSymbol();
   }

void IDTNode::setResolvedMethodSymbol(TR::ResolvedMethodSymbol* rms)
   {
   _rms = rms;
   }

int IDTNode::getCallerIndex()
   {
   if (isRoot())
      return -2;

   IDTNode* parent = getParent();
   return parent->getCalleeIndex();
   }

int IDTNode::getBudget()
   {
   return _budget;
   }

TR_CallSite* IDTNode::getCallSite()
   {
   return _callSite;
   }

void IDTNode::printTrace()
   {
   if (TR::comp()->getOption(TR_TraceBIIDTGen))
      traceMsg(TR::comp(), "IDT: name = %s\n",getName());
   }

void IDTNode::setCallTarget(TR_CallTarget* callTarget)
   {
   _callTarget = callTarget;
   }

void IDTNode::setCallStack(TR_CallStack* callStack)
   {
   _callStack = callStack;
   }

TR_CallTarget* IDTNode::getCallTarget()
   {
   return _callTarget;
   }

TR_CallStack* IDTNode::getCallStack()
   {
   return _callStack;
   }

unsigned int IDTNode::getByteCodeIndex()
   {
   return _callSiteBci;
   }

uint32_t IDTNode::getByteCodeSize()
   {
   return getCallTarget()->_calleeMethod->maxBytecodeIndex();
   }

bool IDTNode::isNodeSimilar(int32_t callSiteBci, TR::ResolvedMethodSymbol* rms)
   {
   auto a = _rms->getResolvedMethod()->maxBytecodeIndex();
   auto b = rms->getResolvedMethod()->maxBytecodeIndex();

   return a == b && _callSiteBci == callSiteBci;
   }

IDTNode* IDTNode::getOnlyChild()
   {
   if (((uintptr_t)_children) & SINGLE_CHILD_BIT)
      return (IDTNode *)((uintptr_t)(_children) & ~SINGLE_CHILD_BIT);
   return NULL;
   }

void IDTNode::setOnlyChild(IDTNode* child)
   {
   TR_ASSERT_FATAL(!((uintptr_t)child & SINGLE_CHILD_BIT), "Maligned memory address.\n");
   _children = (IDTNodeChildren*)((uintptr_t)child | SINGLE_CHILD_BIT);
   }

float IDTNode::getCallRatio() 
   {
   return _callRatio;
   }

float IDTNode::getRootCallRatio()   
   {
   return _rootCallRatio;
   }
