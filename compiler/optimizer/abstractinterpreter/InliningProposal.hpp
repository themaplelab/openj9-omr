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

#ifndef INLINING_PROPOSAL_INCL
#define INLINING_PROPOSAL_INCL

#include "env/Region.hpp"
#include "optimizer/abstractinterpreter/IDT.hpp"
#include "optimizer/abstractinterpreter/IDTNode.hpp"
#include "infra/BitVector.hpp" 
#include "compile/Compilation.hpp"

/**
 *Inlining Proposal records which IDTNode is selected to be inlined.
 * 
 */
class InliningProposal
   {
   public:

   InliningProposal(TR::Region& region, IDT *idt);
   InliningProposal(InliningProposal&, TR::Region& region);

   void print(TR::Compilation *comp);
   bool isEmpty();

   int32_t getCost();
   int32_t getBenefit();

   /**
    * @brief add an IDTNode selected to be inlined to the proposal
    *
    * @param node IDTNode*
    * 
    * @return void
    */
   void addNode(IDTNode* node);

   /**
    * @brief Check if the node is selected to be inlined
    *
    * @param node IDTNode* 
    * 
    * @return bool
    */
   bool isNodeInProposal(IDTNode* node );

   /**
    * @brief Union two proposals.
    *
    * @param a InliningProposal*
    * @param b InliningProposal*
    * 
    * @return void
    */
   void unionInPlace(InliningProposal *a, InliningProposal* b);

   /**
    * @brief Check if self intersects with another proposal.
    *
    * @param other InliningProposal*
    * 
    * @return bool
    */
   bool intersects(InliningProposal* other);

   private:

   void computeCostAndBenefit();
   void ensureBitVectorInitialized();

   TR::Region& _region;
   TR_BitVector *_nodes;
   int32_t _cost;
   int32_t _benefit;
   IDT *_idt;
};


class InliningProposalTable
   {
   public:

   InliningProposalTable(int32_t rows, int32_t cols, TR::Region& region);
   InliningProposal* get(int32_t row, int32_t col);
   void set(int32_t row, int32_t col, InliningProposal* proposal);

   TR::Region& region() { return _region; }

   InliningProposal* getEmptyProposal() { return new (region()) InliningProposal(region(), NULL); }
   
   private:
   
   int32_t _rows;
   int32_t _cols;
   TR::Region& _region;
   InliningProposal ***_table;
   };
#endif