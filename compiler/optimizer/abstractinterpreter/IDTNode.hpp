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

#ifndef IDT_NODE_INCL
#define IDT_NODE_INCL

#include "infra/deque.hpp"
#include "env/Region.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/abstractinterpreter/InliningMethodSummary.hpp"

class IDTNode;
typedef TR::deque<IDTNode*, TR::Region&> IDTNodeDeque;

/**
 * IDTNode is the building block of IDT. 
 * It is an abstract representation of a method containing useful information for inlining.
 * 
 */
class IDTNode
   {
   public:
   IDTNode(
      int32_t idx,
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      int32_t byteCodeIndex, 
      float callRatio,
      IDTNode *parent,
      int32_t budget);

   /**
    * @brief Add a child to the current IDTNode
    * 
    * @param idx int32_t The global index of the child to be added
    * @param callTarget TR_CallTarget* 
    * @param symbol TR::ResolvedMethodSymbol*
    * @param byteCodeIndex int32_t
    * @param callRatio float
    * @param region TR::Region& The region where the child node will be allocated
    * 
    * @return IDTNode*
    * 
    * The return value will be the newly created IDTNode only if addChild() operation is successful.
    * Otherwise, the return value will be NULL. 
    * For example, when it runs out of budget.
    */
   IDTNode* addChild(
      int32_t idx,
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      int32_t byteCodeIndex, 
      float callRatio, 
      TR::Region& region);

   InliningMethodSummary* getInliningMethodSummary() { return _inliningMethodSummary; }
   void setInliningMethodSummary(InliningMethodSummary* inliningMethodSummary) { _inliningMethodSummary = inliningMethodSummary; }

   int32_t getNumDescendants();
   int32_t getNumDescendantsIncludingMe() { return 1 + getNumDescendants(); }

   const char* getName(TR_Memory* mem) { return _callTarget->getSymbol()->getResolvedMethod()->signature(mem); }

   IDTNode *getParent() { return _parent; }

   int32_t getGlobalIndex() { return _idx; }
   int32_t getParentGloablIndex()  { return isRoot() ? -2 : getParent()->getGlobalIndex(); }

   int32_t getBenefit();

   int32_t getStaticBenefit() { return _staticBenefit; };

   void setStaticBenefit(int32_t staticBenefit) { _staticBenefit = staticBenefit; }
   
   int32_t getCost() { return isRoot() ? 0 : getByteCodeSize(); }

   int32_t getRecursiveCost();

   int32_t getNumChildren();
   IDTNode *getChild(int32_t index);

   bool isRoot()  { return _parent == NULL; };

   IDTNode* findChildWithBytecodeIndex(int32_t bcIndex);

   TR::ResolvedMethodSymbol* getResolvedMethodSymbol() { return _symbol; }
   TR_ResolvedMethod* getResolvedMethod() { return _callTarget->_calleeMethod; }
   
   int32_t getBudget()  { return _budget; };

   TR_CallTarget *getCallTarget() { return _callTarget; }

   int32_t getByteCodeIndex() { return _byteCodeIndex; }
   int32_t getByteCodeSize() { return _callTarget->_calleeMethod->maxBytecodeIndex(); }

   float getCallRatio() { return _callRatio; }
   float getRootCallRatio() { return _rootCallRatio; }

   private:
   TR_CallTarget* _callTarget;
   TR::ResolvedMethodSymbol* _symbol;

   IDTNode *_parent;

   int32_t _idx;
   int32_t _byteCodeIndex;
   
   IDTNodeDeque* _children; 
   int32_t _staticBenefit;

   int32_t _budget;

   float _callRatio;
   float _rootCallRatio;

   InliningMethodSummary *_inliningMethodSummary;
   
   /**
    * @brief It is common that a large number of IDTNodes have only one child.
    * So instead of allocating a deque containing only one element, 
    * we use the _children as the address of that only child to save memory usage.
    * 
    * @return IDTNode*
    * 
    * The return value will be NULL if there are more that one children or no child.
    * If there is exactly one child, the return value will be this child.
    */
   IDTNode* getOnlyChild();

   void setOnlyChild(IDTNode* child);
   };
    
#endif