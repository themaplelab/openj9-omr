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
      TR_CallTarget* callTarget,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* symbol,
      float callRatio, 
      IDTNode *parent, 
      int budget):
   _idx(idx),
   _kind(kind),
   _callTarget(callTarget),
   _staticBenefit(0),
   _callSiteBci(callSiteBci),
   _children(NULL),
   _symbol(symbol),
   _parent(parent),
   _budget(budget),
   _callRatio(callRatio),
   _rootCallRatio(parent ? parent->_rootCallRatio * callRatio : 1),
   _methodSummary(NULL)
   {   
   }

IDTNode* IDTNode::addChild(
      int idx,
      TR::MethodSymbol::Kinds kind,
      TR_CallTarget* callTarget,
      int32_t callSiteBci, 
      TR::ResolvedMethodSymbol* symbol, 
      float callRatio,
      TR::Region& region)
   {
   
   if (_rootCallRatio * callRatio * 100 < 25) // don't add things that we do are 1/25th as good as the root
      return NULL;

   int budget =  getBudget() - callTarget->_calleeMethod->maxBytecodeIndex();
   
   if (budget < 0)
      return NULL;

   // The case where there is no children
   if (getNumChildren() == 0)
      {
      IDTNode* newNode = new (region) IDTNode(
                           idx, 
                           kind,
                           callTarget,
                           callSiteBci, 
                           symbol, 
                           callRatio,
                           this,
                           budget);

      setOnlyChild(newNode);
      return newNode;
      }   

   // The case when there is 1 child
   if (getNumChildren() == 1)
      {
      IDTNode* onlyChild = getOnlyChild();
      if (onlyChild && onlyChild->isNodeSimilar(callSiteBci, symbol))
         return NULL;
      
      _children = new (region) IDTNodeChildren(region);
      TR_ASSERT_FATAL(!((uintptr_t)_children & SINGLE_CHILD_BIT), "Maligned memory address.\n");
 
      _children->push_back(onlyChild);
      }
   
   for (auto curr = _children->begin(); curr != _children->end(); ++curr)
      {
      if ((*curr)->isNodeSimilar(callSiteBci, symbol))
         {
         return NULL;
         }
      }

   IDTNode *newChild = new (region) IDTNode(
                        idx, 
                        kind,
                        callTarget,
                        callSiteBci, 
                        symbol, 
                        callRatio,
                        this, 
                        budget);
                     
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

IDTNode* IDTNode::findChildWithBytecodeIndex(int bcIndex)
   {
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

void IDTNode::printTrace()
   {
   if (TR::comp()->getOption(TR_TraceBIIDTGen))
      traceMsg(TR::comp(), "IDT: name = %s\n",getName());
   }

bool IDTNode::isNodeSimilar(int32_t callSiteBci, TR::ResolvedMethodSymbol* rms)
   {
   auto a = _symbol->getResolvedMethod()->maxBytecodeIndex();
   auto b = _symbol->getResolvedMethod()->maxBytecodeIndex();

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

