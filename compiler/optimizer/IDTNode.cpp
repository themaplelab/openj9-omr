#include "optimizer/IDTNode.hpp"

#define SINGLE_CHILD_BIT 1

IDTNode::IDTNode(
      int idx, 
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      int32_t callSiteBci, 
      float callRatio, 
      IDTNode *parent, 
      int budget):
   _idx(idx),
   _callTarget(callTarget),
   _staticBenefit(0),
   _callSiteBci(callSiteBci),
   _symbol(symbol),
   _children(NULL),
   _parent(parent),
   _budget(budget),
   _callRatio(callRatio),
   _rootCallRatio(parent ? parent->_rootCallRatio * callRatio : 1),
   _methodSummary(NULL)
   {   
   }

IDTNode* IDTNode::addChild(
      int idx,
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      int32_t callSiteBci, 
      float callRatio,
      TR::Region& region)
   {
   if (_rootCallRatio * callRatio * 100 < 25) // do not add to the IDT if the root call ratio is less than 0.25
      return NULL;

   int budget =  getBudget() - callTarget->_calleeMethod->maxBytecodeIndex();
   
   if (budget < 0)
      return NULL;

   // The case where there is no children
   if (getNumChildren() == 0)
      {
      IDTNode* newNode = new (region) IDTNode(
                           idx, 
                           callTarget,
                           symbol,
                           callSiteBci, 
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
      _children = new (region) IDTNodeDeque(region); 
      _children->push_back(onlyChild);
      }

   IDTNode *newChild = new (region) IDTNode(
                        idx, 
                        callTarget,
                        symbol,
                        callSiteBci, 
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
      
      if (_children->at(i)->getByteCodeIndex() == bcIndex)
         return _children->at(i);
      }

   return NULL;
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
   _children = (IDTNodeDeque*)((uintptr_t)child | SINGLE_CHILD_BIT);
   }

