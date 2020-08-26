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

#include "optimizer/abstractinterpreter/InliningProposal.hpp"
#include "infra/deque.hpp"
#include "compile/Compilation.hpp"
#include "env/Region.hpp"
#include "infra/BitVector.hpp"

InliningProposal::InliningProposal(TR::Region& region, IDT *idt):
   _cost(-1),
   _benefit(-1),
   _idt(idt),
   _region(region),
   _nodes(NULL) // Lazy Initialization of BitVector
   {
   }

InliningProposal::InliningProposal(InliningProposal &proposal, TR::Region& region):
   _cost(proposal._cost),
   _benefit(proposal._benefit),
   _idt(proposal._idt),
   _region(region)
   {
   _nodes = new (region) TR_BitVector(proposal._nodes->getHighestBitPosition(), region);
   *_nodes = *proposal._nodes;
   }

void InliningProposal::print(TR::Compilation* comp)
   {
   bool traceBIProposal = comp->getOption(TR_TraceBIProposal);
   bool verboseInlining = comp->getOptions()->getVerboseOption(TR_VerboseInlining);

   if (!traceBIProposal && !verboseInlining) //no need to run the following code if neither flag is set
      return; 

   if (!_nodes)
      { 
      traceMsg(comp, "Inlining Proposal is NULL\n");
      return;
      }

   int32_t numMethodsToInline =  _nodes->elementCount()-1;
  
   TR_ASSERT_FATAL(_idt, "Must have an IDT");

   char header[1024];
   sprintf(header,"#Proposal: %d methods inlined into %s", numMethodsToInline, _idt->getRoot()->getName(comp->trMemory()));

   if (traceBIProposal)
      traceMsg(comp, "%s\n", header);
   if (verboseInlining)
      TR_VerboseLog::writeLineLocked(TR_Vlog_BI, header);

   IDTNodeDeque idtNodeQueue(comp->trMemory()->currentStackRegion());
   idtNodeQueue.push_back(_idt->getRoot());

   //BFS 
   while (!idtNodeQueue.empty()) 
      {
      IDTNode* currentNode = idtNodeQueue.front();
      idtNodeQueue.pop_front();
      int32_t index = currentNode->getGlobalIndex();

      if (index != -1) //do not print the root node
         { 
         char line[1024];
         sprintf(line, "#Proposal: #%d : #%d %s @%d -> bcsz=%d %s target %s, benefit = %d, cost = %d, budget = %d",
            currentNode->getGlobalIndex(),
            currentNode->getParentGloablIndex(),
            _nodes->isSet(index + 1) ? "INLINED" : "NOT inlined",
            currentNode->getByteCodeIndex(),
            currentNode->getByteCodeSize(),
            currentNode->getResolvedMethodSymbol()->signature(comp->trMemory()),
            currentNode->getName(comp->trMemory()),
            currentNode->getBenefit(),
            currentNode->getCost(),
            currentNode->getBudget()
         );

         if (traceBIProposal)                                                                                                                                       
            traceMsg(comp, "%s\n",line);
         if (verboseInlining)
            TR_VerboseLog::writeLineLocked(TR_Vlog_BI, line);
         } 
      
      //process children
      int32_t numChildren = currentNode->getNumChildren();

      for (int32_t i = 0; i < numChildren; i++)
         idtNodeQueue.push_back(currentNode->getChild(i));
         
      } 

   traceMsg(comp, "\nBit Vector\n");
   _nodes->print(comp, comp->getOutFile());
   traceMsg(comp, "\n");
   }


void InliningProposal::addNode(IDTNode *node)
   {
   ensureBitVectorInitialized();

   int32_t index = node->getGlobalIndex() + 1;
   if (_nodes->isSet(index))
      {
      return;
      }

   _nodes->set(index);
   }

bool InliningProposal::isEmpty()
   {
   if (!_nodes)
      return true;

   return _nodes->isEmpty();
   }

int32_t InliningProposal::getCost()
   {
   if (_cost == -1) 
      {
      computeCostAndBenefit();
      }

   return _cost;
   }

int32_t InliningProposal::getBenefit()
   {
   if (_benefit == -1)
      {
      computeCostAndBenefit();
      }

   return _benefit;
   }

void InliningProposal::computeCostAndBenefit()
   {  
   _cost = 0;
   _benefit = 0;

   if (!_idt)
      return;

   TR_BitVectorIterator bvi(*_nodes);
   int32_t idtNodeIndex;
   
   while (bvi.hasMoreElements()) 
      {
      idtNodeIndex = bvi.getNextElement();
      IDTNode *node = _idt->getNodeByGlobalIndex(idtNodeIndex - 1);
      if (node == NULL) 
         {
         continue;
         }
      _cost += node->getCost();
      _benefit += node->getBenefit();
      }
   }

void InliningProposal::ensureBitVectorInitialized()
   {
   if (!_nodes)
      _nodes = new (_region) TR_BitVector(_region);
   }

bool InliningProposal::isNodeInProposal(IDTNode* node)
   {
   if (node == NULL)
      return false;
   if (_nodes == NULL)
      return false;
   if (_nodes->isEmpty())
      return false;

   int32_t idx = node->getGlobalIndex() + 1;

   return _nodes->isSet(idx);
   }

void InliningProposal::unionInPlace(InliningProposal *a, InliningProposal *b)
   {
   ensureBitVectorInitialized();
   a->ensureBitVectorInitialized();
   b->ensureBitVectorInitialized();
   
   *_nodes = *a->_nodes;
   *_nodes |= *b->_nodes;
   _cost = -1;
   _benefit = -1;
   }

bool InliningProposal::intersects(InliningProposal* other)
   {
   if (!_nodes || !other->_nodes)
      return false;
   
   return _nodes->intersects(*other->_nodes);
   }


InliningProposalTable::InliningProposalTable(int32_t rows, int32_t cols, TR::Region& region) :
      _rows(rows),
      _cols(cols),
      _region(region)
   {
   _table = new (region) InliningProposal**[rows];

   for (int32_t i = 0; i < rows; i ++)
      {
      _table[i] = new (region) InliningProposal*[cols];
      memset(_table[i], 0, sizeof(InliningProposal*)*cols);
      }
   }

InliningProposal* InliningProposalTable::get(int32_t row, int32_t col)
   {
   InliningProposal* proposal = NULL;

   if (row <0 || col <0 || row >= _rows || col >= _cols)
      proposal = getEmptyProposal();
   else
      proposal = _table[row][col] ? _table[row][col] : getEmptyProposal();

   return proposal;
   }

void InliningProposalTable::set(int32_t row, int32_t col, InliningProposal* proposal)
   {
   TR_ASSERT_FATAL(proposal, "proposal is NULL");
   TR_ASSERT_FATAL(row >=0 && row < _rows, "Invalid row index" );
   TR_ASSERT_FATAL(col >= 0 && col < _cols, "Invalid col index" );

   _table[row][col] = proposal; 
   }