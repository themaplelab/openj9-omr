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

#ifndef BENEFIT_INLINER
#define BENEFIT_INLINER

#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/abstractinterpreter/IDT.hpp"
#include "optimizer/abstractinterpreter/InliningProposal.hpp"

class BenefitInlinerWrapper : public TR::Optimization
   {
   public:
   BenefitInlinerWrapper(TR::OptimizationManager* manager) : TR::Optimization(manager) {};

   static TR::Optimization* create(TR::OptimizationManager *manager)
      {
      return  new (manager->allocator()) BenefitInlinerWrapper(manager);
      }

   virtual int32_t perform();

   virtual const char * optDetailString() const throw()
      {
      return "O^O Benefit Inliner: ";
      }
   };

class BenefitInlinerBase : public TR_InlinerBase
   {
   protected:
   BenefitInlinerBase(TR::Optimizer* optimizer, TR::Optimization* optimization) :
         TR_InlinerBase(optimizer, optimization),
         _inliningProposal(NULL),
         _budget(getInliningBudget(comp()->getMethodSymbol())),
         _inliningDependencyTree(NULL),
         _region(comp()->region())
      {};


   virtual bool inlineCallTargets(TR::ResolvedMethodSymbol* symbol, TR_CallStack* callStack, TR_InnerPreexistenceInfo* info);

   bool inlineIntoIDTNode(TR::ResolvedMethodSymbol *symbol, TR_CallStack *callStack, IDTNode *idtNode);

   virtual bool supportsMultipleTargetInlining() { return false; };
   virtual bool analyzeCallSite(TR_CallStack * callStack, TR::TreeTop * callNodeTreeTop, TR::Node * parent, TR::Node * callNode, TR_CallTarget *calltargetToInline);

   TR::Region& region() { return _region; };
   
   InliningProposal* _inliningProposal;

   protected:
   TR::Region _region;

   int32_t getInliningBudget(TR::ResolvedMethodSymbol* callerSymbol);

   int32_t _budget;

   IDT* _inliningDependencyTree;

   IDTNode* _nextIDTNodeToInlineInto;
   };

class BenefitInliner : public BenefitInlinerBase
   {
   public:
   BenefitInliner(TR::Optimizer* optimizer, TR::Optimization* optimization) :
         BenefitInlinerBase(optimizer, optimization)
      {};

   void buildInliningDependencyTree();

   void inlinerPacking();
   };
   
#endif