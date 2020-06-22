#include "optimizer/IDTNode.hpp"

#define SINGLE_CHILD_BIT 1

bool IDTNodePtrOrder::operator()(IDTNode *left, IDTNode *right)
   {  
   TR_ASSERT(left && right, "Comparing against null\n");
   return left->getCost() < right->getCost() || left->getBenefit() < right->getBenefit();
   }

IDTNode::IDTNode(
      int idx, 
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* rms, 
      IDTNode *parent, 
      int unsigned benefit, 
      int budget, 
      TR_CallSite* callSite, 
      float callRatioCallerCallee,
      TR::ValuePropagation* vp):
   _idx(idx),
   _benefit(benefit),
   _callSiteBci(callSiteBci),
   _children(NULL),
   _rms(rms),
   _parent(parent),
   _budget(budget),
   _callSite(callSite),
   _callTarget(NULL),
   _callStack(NULL),
   _callRatioCallerCallee(callRatioCallerCallee),
   _callRatioRootCallee(parent ? parent->_callRatioRootCallee * callRatioCallerCallee : 1),
   _vp(vp)
   {   
   }

IDTNode* IDTNode::addChildIfNotExists(
      int idx,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* rms,
      unsigned int benefit, 
      TR_CallSite* callSite, 
      float callRatioCallerCallee,
      TR::Region& region)
   {
   // don't add things that we do are 1/25th as good as the root
   if (_callRatioRootCallee * callRatioCallerCallee * 100 < 25)
      return NULL;

   // The case where there is no children
   if (getNumChildren()==0)
      {
      IDTNode* newNode = new (region) IDTNode(
                           idx, 
                           callSiteBci, 
                           rms, 
                           this,
                           benefit, 
                           getBudget() - rms->getResolvedMethod()->maxBytecodeIndex(), 
                           callSite, 
                           callRatioCallerCallee,
                           _vp);

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
      TR_ASSERT(_children->size() == 1, "Something wrong\n");
      }
   
   for (auto curr = _children->begin(); curr != _children->end(); ++curr)
      {
      TR_ASSERT(*curr, "No child can be null\n");
      if ((*curr)->isNodeSimilar(callSiteBci, rms))
         {
         return NULL;
         }
      }

   IDTNode *newChild = new (region) IDTNode(
                        idx, 
                        callSiteBci, 
                        rms, 
                        this, 
                        benefit, 
                        getBudget() - rms->getResolvedMethod()->maxBytecodeIndex(), 
                        callSite, 
                        callRatioCallerCallee,
                        _vp);
                        
   TR_ASSERT_FATAL(newChild, "Storing a null child\n");
   _children->push_back(newChild);
   TR_ASSERT(_children->size() > 1, "Something wrong\n");
   return _children->back();
   }

unsigned int IDTNode::getNumDescendants() const
   {
   unsigned int numChildren = getNumChildren();
   unsigned int sum = 0;
   for (unsigned int i =0; i < numChildren; i ++)
      {
      sum += 1 + getChild(i)->getNumDescendants(); 
      }
   return sum;
   }

unsigned int IDTNode::getNumDescendantsIncludingMe() const
   {
   return 1 + getNumDescendants();
   }

const char* IDTNode::getName(TR_Memory* mem) const 
   {
   return _rms->getResolvedMethod()->signature(mem);
   }

const char* IDTNode::getName() const
   {
   //slow implementation TR::comp() is expensive but here we are in a context where we don't have access to TR_Memory
   return _rms->getResolvedMethod()->signature(TR::comp()->trMemory());
   }

IDTNode* IDTNode::getParent() const
   {
   return _parent;
   }

int IDTNode::getCalleeIndex() const
   {
   return _idx;
   }

unsigned int IDTNode::getCost() const
   {
   return isRoot() ? 1 : getByteCodeSize();
   }

unsigned int IDTNode::getRecursiveCost() const 
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

unsigned int IDTNode::getBenefit() const
   {
   return _callRatioRootCallee * 100 * ( 1 + _benefit) + 1;
   }

void IDTNode::printByteCode() const
   {
   TR_ResolvedJ9Method* resolvedMethod = static_cast<TR_ResolvedJ9Method*>(_rms->getResolvedMethod());
   TR_J9VMBase *fe = static_cast<TR_J9VMBase*>(TR::comp()->fe());
   TR_J9ByteCodeIterator bci(_rms, resolvedMethod, fe, TR::comp());
   const char* signature1 = bci.methodSymbol()->signature(TR::comp()->trMemory());
   const char* signature2 = bci.method()->signature(TR::comp()->trMemory());
   unsigned long int end = resolvedMethod->maxBytecodeIndex();
   for (TR_J9ByteCode bc = bci.current(); bc != J9BCunknown && bci.currentByteCodeIndex() < end; bc = bci.next())
   {
      bci.printByteCode();
      IDTNode *child = NULL;
      switch(bc)
         {
         case J9BCinvokestatic:
         case J9BCinvokevirtual:
         case J9BCinvokedynamic:
         case J9BCinvokespecial:
         case J9BCinvokeinterface:
         // print name of target...
            child = findChildWithBytecodeIndex(bci.currentByteCodeIndex());
            if (!child) {
               traceMsg(TR::comp(), "No child for bytecode index %d\n", bci.currentByteCodeIndex());
               break;
            }
            traceMsg(TR::comp(), "CalleeSymbol signature %s\n", child->getCallTarget()->_calleeSymbol->signature(TR::comp()->trMemory()));
            traceMsg(TR::comp(), "calleeMethod signature %s\n", child->getCallTarget()->_calleeMethod->signature(TR::comp()->trMemory()));
         break;
         default:
         break;
         }
   }
   traceMsg(TR::comp(), "\n");
   }

void IDTNode::setBenefit(unsigned int benefit)
   {
   _benefit = benefit;
   }

void IDTNode::enqueueSubordinates(IDTNodePtrPriorityQueue *q) const
   {
   TR_ASSERT(q, "Priority queue cannot be NULL!\n");
   unsigned int numChildren = getNumChildren();
   if (numChildren == 0)
      return;

   if (numChildren == 1) //The case where there is only one child
      {
      IDTNode* onlyChild = getOnlyChild();
      TR_ASSERT(onlyChild, "Child cannot be NULL!\n");
      q->push(onlyChild);
      return;
      }
   
   for (unsigned int i = 0; i < numChildren; i ++)
      {
      IDTNode *child = _children->at(i);
      TR_ASSERT(child, "Child cannot be NULL!\n");
      q->push(child);
      }
   }

unsigned int IDTNode::getNumChildren() const
   {
   if (_children == NULL)
      return 0;
   
   if (getOnlyChild() != NULL)
      return 1;
   
   size_t num = _children->size();
   TR_ASSERT(num > 1, "num cannot be 1 or 0!\n");
   return num;
   }

IDTNode* IDTNode::getChild(unsigned int index) const
   {
   unsigned int numChildren = getNumChildren();
   
   if (numChildren == 0)
      return NULL;

   TR_ASSERT_FATAL(index < numChildren, "Child index out of range!\n");

   if (index == 0 && numChildren == 1) // only one child
      return getOnlyChild();

   return _children->at(index);
   }

bool IDTNode::isRoot() const
   {
   return getParent() == NULL;
   }

IDTNode* IDTNode::findChildWithBytecodeIndex(int bcIndex) const
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
         TR_ASSERT(false, "we have more than two, how to disambiguate?");
      
      if (_children->at(i)->getByteCodeIndex() == bcIndex)
         child = _children->at(i);
      }
   return child;
   }

bool IDTNode::isSameMethod(IDTNode* aNode) const  
   {
   return isSameMethod(aNode->getResolvedMethodSymbol());
   }

bool IDTNode::isSameMethod(TR::ResolvedMethodSymbol* rms) const
   {
   return getResolvedMethodSymbol()->getResolvedMethod()->isSameMethod(rms->getResolvedMethod());
   }

unsigned int IDTNode::numberOfParameters()
   {
   TR_ASSERT_FATAL(false, "Unimplemented!\n");
   return 0;
   }

TR::ResolvedMethodSymbol* IDTNode::getResolvedMethodSymbol() const
   {
   return !this->getCallTarget() ? _rms : getCallTarget()->getSymbol();
   }

void IDTNode::setResolvedMethodSymbol(TR::ResolvedMethodSymbol* rms)
   {
   _rms = rms;
   }

int IDTNode::getCallerIndex() const
   {
   if (isRoot())
      return -2;

   const IDTNode* parent = getParent();
   return parent->getCalleeIndex();
   }

UDATA IDTNode::maxStack() const
   {
   TR_ResolvedJ9Method *method = (TR_ResolvedJ9Method *)getResolvedMethodSymbol()->getResolvedMethod();
   J9ROMMethod *romMethod = method->romMethod();
   return J9_MAX_STACK_FROM_ROM_METHOD(romMethod);
   }

IDATA IDTNode::maxLocals() const
   {
   TR_ResolvedJ9Method *method = (TR_ResolvedJ9Method *)getResolvedMethodSymbol()->getResolvedMethod();
   J9ROMMethod *romMethod = method->romMethod();
   return J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
   }

int IDTNode::getBudget() const
   {
   return _budget;
   }

TR_CallSite* IDTNode::getCallSite() const
   {
   return _callSite;
   }

void IDTNode::printTrace() const
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

TR_CallTarget* IDTNode::getCallTarget() const
   {
   return _callTarget;
   }

TR_CallStack* IDTNode::getCallStack() const
   {
   return _callStack;
   }

unsigned int IDTNode::getByteCodeIndex() const
   {
   return _callSiteBci;
   }

uint32_t IDTNode::getByteCodeSize() const
   {
   return getCallTarget()->_calleeMethod->maxBytecodeIndex();
   }

bool IDTNode::isNodeSimilar(int32_t callSiteBci, TR::ResolvedMethodSymbol* rms) const
   {
   auto a = _rms->getResolvedMethod()->maxBytecodeIndex();
   auto b = rms->getResolvedMethod()->maxBytecodeIndex();

   return a == b && _callSiteBci == callSiteBci;
   }

IDTNode* IDTNode::getOnlyChild() const
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

float IDTNode::getCallRatio() const
   {
   return _callRatioCallerCallee;
   }

float IDTNode::getRootCallRatio() const  
   {
   return _callRatioRootCallee;
   }

TR::ValuePropagation* IDTNode::getValuePropagation() const
   {
   return _vp;
   }