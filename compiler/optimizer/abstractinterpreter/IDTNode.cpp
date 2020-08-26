/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "optimizer/abstractinterpreter/IDTNode.hpp"

#define SINGLE_CHILD_BIT 1

IDTNode::IDTNode(
      int32_t idx, 
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      int32_t byteCodeIndex, 
      float callRatio, 
      IDTNode *parent, 
      int32_t budget) :
   _idx(idx),
   _callTarget(callTarget),
   _staticBenefit(0),
   _byteCodeIndex(byteCodeIndex),
   _symbol(symbol),
   _children(NULL),
   _parent(parent),
   _budget(budget),
   _callRatio(callRatio),
   _rootCallRatio(parent ? parent->_rootCallRatio * callRatio : 1),
   _inliningMethodSummary(NULL)
   {   
   }

IDTNode* IDTNode::addChild(
      int32_t idx,
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      int32_t byteCodeIndex, 
      float callRatio,
      TR::Region& region)
   {
   if (_rootCallRatio * callRatio * 100 < 25) // do not add to the IDT if the root call ratio is less than 0.25
      return NULL;

   int32_t budget =  getBudget() - callTarget->_calleeMethod->maxBytecodeIndex();
   
   if (budget < 0)
      return NULL;

   // The case where there is no children
   if (getNumChildren() == 0)
      {
      IDTNode* newNode = new (region) IDTNode(
                           idx, 
                           callTarget,
                           symbol,
                           byteCodeIndex, 
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
                        byteCodeIndex, 
                        callRatio,
                        this, 
                        budget);
                     
   _children->push_back(newChild);
   return _children->back();
   }

int32_t IDTNode::getNumDescendants()
   {
   int32_t numChildren = getNumChildren();
   int32_t sum = 0;
   for (int32_t i =0; i < numChildren; i ++)
      {
      sum += 1 + getChild(i)->getNumDescendants(); 
      }
   return sum;
   }

int32_t IDTNode::getRecursiveCost() 
   {
   int32_t numChildren = getNumChildren();
   int32_t cost = getCost();
   for (int32_t i = 0; i < numChildren; i ++)
      {
      IDTNode *child = getChild(i);
      cost += child->getRecursiveCost();
      }
   
   return cost;
   }


int32_t IDTNode::getNumChildren()
   {
   if (_children == NULL)
      return 0;
   
   if (getOnlyChild() != NULL)
      return 1;
   
   size_t num = _children->size();
   return num;
   }

IDTNode* IDTNode::getChild(int32_t index)
   {
   int32_t numChildren = getNumChildren();
   
   if (numChildren == 0)
      return NULL;

   TR_ASSERT_FATAL(index < numChildren, "Child index out of range!\n");

   if (index == 0 && numChildren == 1) // only one child
      return getOnlyChild();

   return _children->at(index);
   }

int32_t IDTNode::getBenefit()
   {
   float benefit = _rootCallRatio  * (1 + _staticBenefit) * 10; 
   return benefit;
   }

IDTNode* IDTNode::findChildWithBytecodeIndex(int32_t bcIndex)
   {
   int32_t size = getNumChildren();
   
   if (size == 0)
      return NULL;
   
   if (size == 1)
      {
      IDTNode* onlyChild = getOnlyChild();
      return onlyChild->getByteCodeIndex() == bcIndex ? onlyChild : NULL;
      }
   
   IDTNode* child = NULL;
   for (int32_t i = 0; i < size; i ++)
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

