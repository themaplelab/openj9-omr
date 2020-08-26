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

#ifndef OMR_IDT_BUILDER_INCL
#define OMR_IDT_BUILDER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_IDT_BUILDER_CONNECTOR
#define OMR_IDT_BUILDER_CONNECTOR
namespace OMR { class IDTBuilder; }
namespace OMR { typedef OMR::IDTBuilder IDTBuilderConnector; }
#endif

#include "il/ResolvedMethodSymbol.hpp"
#include "env/Region.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/abstractinterpreter/AbsVisitor.hpp"
#include "optimizer/abstractinterpreter/IDT.hpp"
#include "optimizer/abstractinterpreter/IDTNode.hpp"

namespace TR { class IDTBuilder; }
class IDTBuilderVisitor;

namespace OMR
{
class IDTBuilder
   {
   friend class ::IDTBuilderVisitor;

   public:
   
   IDTBuilder(TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Region& region, TR::Compilation* comp, TR_InlinerBase* inliner);
   
   /**
    * @brief building the IDT in DFS order.
    * It starts from creating the root IDTNode using the _rootSymbol
    * and then builds the IDT recursively.
    * It stops when no more call site is found or the budget runs out.
    * 
    * @return IDT*
    * 
    * The returned IDT is the final IDT that can be consumed by the inliner packing algorithm.
    */
   IDT* buildIDT();

   protected:

   TR::Compilation* comp() { return _comp; };
   TR::Region& region() { return _region; };
   TR_InlinerBase* getInliner()  { return _inliner; };

   private:

   /**
    * @brief generate the control flow graph of a call target so that the abstract interpretation can use. 
    * Note: This method needs language specific implementation.
    *
    * @param callTarget TR_CallTarget*
    * 
    * @return TR::CFG*
    */
   virtual TR::CFG* generateControlFlowGraph(TR_CallTarget* callTarget) { TR_UNIMPLEMENTED(); return NULL; }

   /**
    * @brief Perform the abstract interpretation on the method in the IDTNode. 
    * Note: This method needs language specific implementation.
    *
    * @param node IDTNode*
    * @param visitor IDTBuilderVisitor& - The IDTBuilderVisitor defines the callback method 
    *                that will be called when visiting a call site during abtract interpretation.
    * @param arguments AbsArguments* - The arguments are the AbsValues passed from the caller method.
    * @param callerIndex int32_t
    * 
    * @return void
    */
   virtual void performAbstractInterpretation(IDTNode* node, IDTBuilderVisitor& visitor, AbsArguments* arguments, int32_t callerIndex) { TR_UNIMPLEMENTED(); }

   /**
    * @brief This is one of the two core methods for building the IDT.
    *
    * @param node IDTNode*
    * @param arguments AbsArguments
    * @param callerIndex int32_t
    * @param budget int32_t
    * @param callStack TR_CallStack*
    * 
    * @return void
    */
   void buildIDTHelper(IDTNode* node, AbsArguments* arguments, int32_t callerIndex, int32_t budget, TR_CallStack* callStack);
   
   /**
    * @brief add child to the IDTNode when identifing any call site.
    * This is one of the two core methods for building the IDT.
    * 
    * Note: call targets in the call site may not be added as a new IDTNode due to some reasons.
    *
    * @param node IDTNode*
    * @param callerIndex int32_t
    * @param callSite TR_CallSite*
    * @param arguments AbsArguments* - arguments passed from the caller method.
    * @param callStack TR_CallStack*
    * @param callBlock TR::Block* - the basic block where the call site is in.
    * 
    * @return void
    */
   void addChild(IDTNode* node, int32_t callerIndex, TR_CallSite* callSite, AbsArguments* arguments, TR_CallStack* callStack, TR::Block* callBlock);

   /**
    * @brief compute the call ratio of the call target.
    * 
    * @param callBlock TR::Block* 
    * @param startBlock TR::Block* - The start block of caller's CFG
    * 
    * @return float
    */
   float computeCallRatio(TR::Block* callBlock, TR::Block* startBlock);

   /**
    * @brief compute static benefit of inlining a particular method.
    * 
    * @param summary InliningMethodSummary* 
    * @param arguments AbsArguments*
    * 
    * @return int32_t
    */
   int32_t computeStaticBenefit(InliningMethodSummary* summary, AbsArguments* arguments);

   IDTNode* checkIfMethodIsInterpreted(TR_ResolvedMethod* method);
   void storeInterpretedMethod(TR_ResolvedMethod* method, IDTNode* node);

   
   IDT* _idt;
   TR::ResolvedMethodSymbol* _rootSymbol;

   typedef TR::typed_allocator<std::pair<TR_OpaqueMethodBlock*, IDTNode*>, TR::Region&> InterpretedMethodMapAllocator;
   typedef std::less<TR_OpaqueMethodBlock*> InterpretedMethodMapComparator;
   typedef std::map<TR_OpaqueMethodBlock *, IDTNode*, InterpretedMethodMapComparator, InterpretedMethodMapAllocator> InterpretedMethodMap;
   InterpretedMethodMap _interpretedMethodMap;

   int32_t _rootBudget;
   TR::Region& _region;
   TR::Compilation* _comp;
   TR_InlinerBase* _inliner;
   };
}

class IDTBuilderVisitor : public AbsVisitor
   {
   public:
   IDTBuilderVisitor(OMR::IDTBuilder* idtBuilder, IDTNode* idtNode, TR_CallStack* callStack) :
         _idtBuilder(idtBuilder),
         _idtNode(idtNode),
         _callStack(callStack)
      {}
      
   virtual void visitCallSite(TR_CallSite* callSite, int32_t callerIndex, TR::Block* callBlock, AbsArguments* arguments);

   private:
   OMR::IDTBuilder* _idtBuilder;
   IDTNode* _idtNode;
   TR_CallStack* _callStack;
   };

#endif