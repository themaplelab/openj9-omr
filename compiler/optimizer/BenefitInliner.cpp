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

#include "optimizer/BenefitInliner.hpp"
#include "optimizer/abstractinterpreter/IDTBuilder.hpp"
#include "il/Node.hpp"
#include "il/OMRNode_inlines.hpp"
#include <algorithm>


/**
 * Steps of BenefitInliner:
 * 
 * 
 * 1. perform() --> 2. build IDT -->  3. abstract interpretation  -->  5. run inliner packing (nested knapsack) --> 6. perform inlining
 *                  |                                          |
 *                  |--4. update IDT with inlining summaries --| 
 *
 * 
 * Note: Abstract Interpretation is part of the IDT building process. Check the IDTBuilder.
 * 
 */
int32_t BenefitInlinerWrapper::perform()
   {
   BenefitInliner inliner(optimizer(), this);

   inliner.buildInliningDependencyTree(); //IDT

   inliner.inlinerPacking(); // nested knapsack

   inliner.performInlining(comp()->getMethodSymbol());

   return 1;
   }

void BenefitInliner::buildInliningDependencyTree()
   {
   TR::IDTBuilder builder(comp()->getMethodSymbol(), _budget, region(), comp(), this);
   _inliningDependencyTree = builder.buildIDT();
   
   if (comp()->getOption(TR_TraceBIIDTGen))
      _inliningDependencyTree->printTrace();

   _nextIDTNodeToInlineInto = _inliningDependencyTree->getRoot();
   }

void BenefitInliner::inlinerPacking()
   {
   _inliningDependencyTree->buildIndices();

   int32_t idtSize = _inliningDependencyTree->getNumNodes();
   int32_t budget = _budget;

   //initialize InliningProposal Table (idtSize x budget+1)
   InliningProposalTable table(idtSize, budget + 1, comp()->trMemory()->currentStackRegion());

   IDTPreorderPriorityQueue preorderPQueue(_inliningDependencyTree, comp()->trMemory()->currentStackRegion()); 
   for (int32_t row = 0; row < idtSize; row ++)
      {
      for (int32_t col = 1; col < budget + 1; col ++)
         {
         InliningProposal currentSet(comp()->trMemory()->currentStackRegion(), _inliningDependencyTree); // []
         IDTNode* currentNode = preorderPQueue.get(row); 
         
         currentSet.addNode(currentNode); //[ currentNode ]
         
         int32_t offsetRow = row - 1;

         while (!currentNode->isRoot() 
            && !table.get(offsetRow, col-currentSet.getCost())->isNodeInProposal(currentNode->getParent()))
            {
            currentSet.addNode(currentNode->getParent());
            currentNode = currentNode->getParent();
            }

         while ( currentSet.intersects(table.get(offsetRow, col - currentSet.getCost()))
            || ( !(currentNode->getParent() && table.get(offsetRow, col - currentSet.getCost())->isNodeInProposal(currentNode->getParent()) )
                  && !table.get(offsetRow, col - currentSet.getCost())->isEmpty() 
                  ))
            {
            offsetRow--;
            }

         InliningProposal* newProposal = new (comp()->trMemory()->currentStackRegion()) InliningProposal(comp()->trMemory()->currentStackRegion(), _inliningDependencyTree);
         newProposal->unionInPlace(table.get(offsetRow, col - currentSet.getCost()), &currentSet);

         if (newProposal->getCost() <= col && newProposal->getBenefit() > table.get(row-1, col)->getBenefit()) //only set the new proposal if it fits the budget and has more benefits
            table.set(row, col, newProposal);
         else
            table.set(row, col, table.get(row-1, col));
         }
      }
   
   InliningProposal* result = new (region()) InliningProposal(region(), _inliningDependencyTree); 
   result->unionInPlace(result, table.get(idtSize-1, budget));

   if (comp()->getOption(TR_TraceBIProposal))
      {
      traceMsg(comp(), "\n#inliner packing:\n" );
      result->print(comp());
      }
   
   _inliningProposal = result;
   }

int32_t BenefitInlinerBase::getInliningBudget(TR::ResolvedMethodSymbol* callerSymbol)
   {
   int32_t size = callerSymbol->getResolvedMethod()->maxBytecodeIndex();
   
   int32_t budget;

   if (comp()->getMethodHotness() >= scorching)
      budget = std::max(1500, size * 2);
   else if (comp()->getMethodHotness() >= hot)
      budget = std::max(1500, size + (size >> 2));
   else if (comp()->getMethodHotness() >= warm && size < 250)
      budget = 250;
   else if (comp()->getMethodHotness() >= warm && size < 700)
      budget = std::max(700, size + (size >> 2));
   else if (comp()->getMethodHotness() >= warm)
      budget = size + (size >> 3);
   else 
      budget = 25;

   return budget;
   }

bool BenefitInlinerBase::inlineCallTargets(TR::ResolvedMethodSymbol *symbol, TR_CallStack *prevCallStack, TR_InnerPreexistenceInfo *info)
   {
   if (!_nextIDTNodeToInlineInto) 
      return false;

   if (comp()->trace(OMR::inlining))
      traceMsg(comp(), "#BenefitInliner: inlining into %s\n", _nextIDTNodeToInlineInto->getName(comp()->trMemory()));

   TR_CallStack callStack(comp(), symbol, symbol->getResolvedMethod(), prevCallStack, 1500, true);

   if (info)
      callStack._innerPrexInfo = info;

   bool inlined = inlineIntoIDTNode(symbol, &callStack, _nextIDTNodeToInlineInto);
   return inlined;
   }

bool BenefitInlinerBase::inlineIntoIDTNode(TR::ResolvedMethodSymbol *symbol, TR_CallStack *callStack, IDTNode *idtNode)
   {
   int32_t inlineCount = 0;

   for (TR::TreeTop * tt = symbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node * parent = tt->getNode();
      if (!parent->getNumChildren())
         continue;

      TR::Node * node = parent->getChild(0);
      if (!node->getOpCode().isCall()) 
         continue;

      if (node->getVisitCount() == _visitCount)
         continue;

      TR_ByteCodeInfo &bcInfo = node->getByteCodeInfo();

      //The actual call target to inline
      IDTNode *childToInline = idtNode->findChildWithBytecodeIndex(bcInfo.getByteCodeIndex());

      if (!childToInline)
         continue;

      //only inline this call target if it is in inlining proposal
      bool shouldInline = _inliningProposal->isNodeInProposal(childToInline);

      if (!shouldInline)
         continue;

      _nextIDTNodeToInlineInto = childToInline;

      bool success = analyzeCallSite(callStack, tt, parent, node, childToInline->getCallTarget());

      _nextIDTNodeToInlineInto = _nextIDTNodeToInlineInto->getParent();

      if (success)
         {
         inlineCount++;

#define MAX_INLINE_COUNT 1000
         if (inlineCount >= MAX_INLINE_COUNT)
            {
            if (comp()->trace(OMR::inlining))
               traceMsg(comp(), "BenefitInliner: stopping inlining as max inline count of %d reached\n", MAX_INLINE_COUNT);
            break;
            }

#undef MAX_INLINE_COUNT
         node->setVisitCount(_visitCount);

         }
      }

   callStack->commit();
   return inlineCount > 0;
   }

bool BenefitInlinerBase::analyzeCallSite(TR_CallStack * callStack, TR::TreeTop * callNodeTreeTop, TR::Node * parent, TR::Node * callNode, TR_CallTarget *calltargetToInline)
   {

   TR::SymbolReference *symRef = callNode->getSymbolReference();

   TR_CallSite *callsite = TR_CallSite::create(callNodeTreeTop, parent, callNode,
                                               (TR_OpaqueClassBlock*) 0, symRef, (TR_ResolvedMethod*) 0,
                                               comp(), trMemory() , stackAlloc);

   getSymbolAndFindInlineTargets(callStack, callsite);

   if (!callsite->numTargets())
      return false;

   bool success = false;

   for(int32_t i=0; i<callsite->numTargets(); i++)
      {
      TR_CallTarget *calltarget = callsite->getTarget(i);

      if (calltarget->_calleeMethod->isSameMethod(calltargetToInline->_calleeMethod) && !calltarget->_alreadyInlined) //we need to inline the exact call target in the IDTNode
         {
         success = inlineCallTarget(callStack, calltarget, false);
         break;
         }
      }

   return success;
   }
