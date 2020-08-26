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

#ifndef IDT_INCL
#define IDT_INCL

#include "compile/Compilation.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/abstractinterpreter/IDTNode.hpp"
#include "infra/deque.hpp"
#include "env/Region.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include <queue>

/**
 * IDT stands for Inlining Dependency Tree
 * It is a data structure that holds all candidate methods to be inlined. 
 * 
 * The parent-child relationship in the IDT corresponds to the caller-callee relationship.
 */
class IDT
   {
   public:
   IDT(TR::Region& region, TR_CallTarget*, TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Compilation* comp);

   IDTNode* getRoot() { return _root; };

   TR::Region& getRegion() { return _region; }
   
   /**
    * @brief Get the total number of nodes in this IDT.
    *
    * @return int32_t
    */
   int32_t getNumNodes() { return _maxIdx + 1; }

   /**
    * @brief Copy the descendants of fromNode and add them as the descents of toNode
    * Note: fromNode and toNode must be the same method.
    * Otherwise, it does not make any sense.
    * 
    * @param fromNode IDTNode*
    * @param toNode IDTNode*
    * 
    * @return void
    */
   void copyDescendants(IDTNode* fromNode, IDTNode* toNode);
   
   /**
    * @brief Get the next avaible IDTNode index.
    *
    * @return int32_t
    */
   int32_t getNextGlobalIDTNodeIndex() { return _maxIdx; }

   /**
    * @brief Increase the next available IDTNode index by 1.
    * This should only be called when successfully adding an IDTNode to the IDT
    *
    * @return void
    */
   void increaseGlobalIDTNodeIndex()  { _maxIdx ++; }

   /**
    * @brief Get the IDTNode using index.
    * Note: Before using this method for accessing IDTNode, buildIndices() must be called.
    *
    * @return IDTNode*
    */
   IDTNode *getNodeByGlobalIndex(int32_t index);
   
   /**
    * @brief Flatten all the IDTNodes in this IDT into a list. 
    * Therefore, IDTNode can be accessed by using its global node index.
    * 
    * @return void
    */
   void buildIndices();

   void printTrace();

   private:

   TR::Compilation* comp() { return _comp; }

   TR::Compilation *_comp;
   TR::Region&  _region;
   int32_t _maxIdx;
   IDTNode* _root;
   IDTNode** _indices;
   };

/**
 * Accessing IDTNode by the priority of its cost.
 */
class IDTPreorderPriorityQueue
   {
   public:
   IDTPreorderPriorityQueue(IDT* idt, TR::Region& region);
   int32_t size() { return _idt->getNumNodes(); }

   IDTNode* get(int32_t index);

   private:

   struct IDTNodeCompare {
      bool operator()(IDTNode *left, IDTNode *right)
         {
         TR_ASSERT_FATAL(left && right, "Comparing against null");
         return left->getCost() < right->getCost() || left->getBenefit() < right->getBenefit();
         };
   };

   typedef TR::vector<IDTNode*, TR::Region&> IDTNodeVector;
   typedef std::priority_queue<IDTNode*, IDTNodeVector, IDTNodeCompare> IDTNodePriorityQueue;
   
   IDT* _idt;
   IDTNodePriorityQueue _pQueue;
   IDTNodeVector _entries;
   };
#endif
