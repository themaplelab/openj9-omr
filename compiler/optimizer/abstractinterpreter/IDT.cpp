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

#include "optimizer/abstractinterpreter/IDT.hpp"

IDT::IDT(TR::Region& region, TR_CallTarget* callTarget, TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Compilation* comp):
      _region(region),
      _maxIdx(-1),
      _comp(comp),
      _root(new (_region) IDTNode(getNextGlobalIDTNodeIndex(), callTarget, symbol, -1, 1, NULL, budget)),
      _indices(NULL)
   {   
   increaseGlobalIDTNodeIndex();
   }

void IDT::printTrace()
   {
   bool verboseInlining = comp()->getOptions()->getVerboseOption(TR_VerboseInlining);
   bool traceBIIDTGen = comp()->getOption(TR_TraceBIIDTGen);

   if (!verboseInlining && !traceBIIDTGen)
      return;
   
   // print header line
   char header[1024];
   const int32_t candidates = getNumNodes() - 1;
   sprintf(header,"#IDT: %d candidate methods inlinable into %s with a budget %d", 
      candidates,
      getRoot()->getName(comp()->trMemory()),
      getRoot()->getBudget()
   );

   if (verboseInlining)
      TR_VerboseLog::writeLineLocked(TR_Vlog_BI, header);
   if (traceBIIDTGen)
      traceMsg(comp(), "%s\n", header);

   if (candidates <= 0) 
      {
      return;
      }

   //print the IDT nodes in BFS
   IDTNodeDeque idtNodeQueue(comp()->trMemory()->currentStackRegion());

   idtNodeQueue.push_back(getRoot());

   while (!idtNodeQueue.empty())
      {
      IDTNode* currentNode = idtNodeQueue.front();
      idtNodeQueue.pop_front();

      int32_t index = currentNode->getGlobalIndex();

      //print IDT node info
      if (index != -1) //skip root node
         {
         char line[1024];
         sprintf(line, "#IDT: #%d: #%d inlinable @%d -> bcsz=%d %s target %s, static benefit = %d, benefit = %d, cost = %d, budget = %d, callratio = %f, rootcallratio = %f", 
            index,
            currentNode->getParentGloablIndex(),
            currentNode->getByteCodeIndex(),
            currentNode->getByteCodeSize(),
            currentNode->getResolvedMethodSymbol()->signature(comp()->trMemory()),
            currentNode->getName(comp()->trMemory()),
            currentNode->getStaticBenefit(),
            currentNode->getBenefit(),
            currentNode->getCost(),
            currentNode->getBudget(),
            currentNode->getCallRatio(),
            currentNode->getRootCallRatio()
         );

         if (verboseInlining)
            TR_VerboseLog::writeLineLocked(TR_Vlog_BI, line);

         if (traceBIIDTGen) 
            traceMsg(comp(), "%s\n", line);
         }
         
      //process children
      for (int32_t i = 0; i < currentNode->getNumChildren(); i ++)
         idtNodeQueue.push_back(currentNode->getChild(i));
         
      }
         
   }

void IDT::buildIndices()
   {
   if (_indices != NULL)
      return;

   //initialize nodes index array
    int32_t numNodes = getNumNodes();
   _indices = new (_region) IDTNode *[numNodes];

   memset(_indices, 0, sizeof(IDTNode*) * numNodes);

   //add all the descendents of the root node to the indices array
   IDTNodeDeque idtNodeQueue(comp()->trMemory()->currentStackRegion());
   idtNodeQueue.push_back(getRoot());

   while (!idtNodeQueue.empty())
      {
      IDTNode* currentNode = idtNodeQueue.front();
      idtNodeQueue.pop_front();

      int32_t calleeIndex = currentNode->getGlobalIndex();
      TR_ASSERT_FATAL(_indices[calleeIndex+1] == 0, "Callee index not unique!\n");

      _indices[calleeIndex + 1] = currentNode;

      for (int32_t i = 0; i < currentNode->getNumChildren(); i ++)
         {
         idtNodeQueue.push_back(currentNode->getChild(i));
         }
      }
   }

IDTNode *IDT::getNodeByGlobalIndex(int32_t index)
   {
   TR_ASSERT_FATAL(_indices, "Call buildIndices() first");
   TR_ASSERT_FATAL(index < getNextGlobalIDTNodeIndex(), "Index out of range!");
   return _indices[index + 1];
   }

void IDT::copyDescendants(IDTNode* fromNode, IDTNode* toNode)
   {
   TR_ASSERT_FATAL(
      fromNode->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier() 
      == toNode->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier(), 
      "Copying different nodes is not allowed!");

   for ( int32_t i =0 ; i < fromNode->getNumChildren(); i ++)
      {
      IDTNode* child = fromNode->getChild(i);

      if (toNode->getBudget() - child->getCost() < 0)
         continue;
         

      IDTNode* copiedChild = toNode->addChild(
                           getNextGlobalIDTNodeIndex(),
                           child->getCallTarget(),
                           child->getResolvedMethodSymbol(),
                           child->getByteCodeIndex(),
                           child->getCallRatio(),
                           getRegion()
                           );
      if (copiedChild)
            {
            increaseGlobalIDTNodeIndex();
            copiedChild->setInliningMethodSummary(child->getInliningMethodSummary());
            copiedChild->setStaticBenefit(child->getStaticBenefit());
            copyDescendants(child, copiedChild);
            }
      }
  
   }

IDTPreorderPriorityQueue::IDTPreorderPriorityQueue(IDT* idt, TR::Region& region)  :
      _entries(region),
      _idt(idt),
      _pQueue(IDTNodeCompare(), IDTNodeVector(region))
   {
   _pQueue.push(idt->getRoot());
   }

IDTNode* IDTPreorderPriorityQueue::get(int32_t index)
   {
   size_t entriesSize = _entries.size();

   if (entriesSize > index) //already in entries
      return _entries.at(index);

    int32_t idtSize = size();

   if (index > idtSize - 1)
      return NULL;

   //not in entries yet. Update entries.
   while (_entries.size() <= index) 
      {
      IDTNode *newEntry = _pQueue.top();
      _pQueue.pop();

      _entries.push_back(newEntry);
      for ( int32_t j = 0; j < newEntry->getNumChildren(); j++)
         {
         _pQueue.push(newEntry->getChild(j));
         }
      }

   return _entries.at(index);
   }