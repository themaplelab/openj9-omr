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

#ifndef INLINING_METHOD_SUMMARY_INCL
#define INLINING_METHOD_SUMMARY_INCL

#include "optimizer/VPConstraint.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "optimizer/abstractinterpreter/AbsValue.hpp"

class PotentialOptimization;

/**
 * The Inlining Method Summary captures potential optimization opportunities of inlining one particular method 
 * and also specifies the constraints that are the maximal safe values to make the optimizations happen.
 * 
 */
class InliningMethodSummary
   {
   public:

   /**
    * @brief calculate the total static benefit coming from a particular parameter after inlining. 
    *
    * @param param AbsValue*
    * @param paramPosition int32_t
    * 
    * @return int32_t
    */
   int32_t predicates(AbsValue* param, int32_t paramPosition);

   InliningMethodSummary(TR::Region& region, OMR::ValuePropagation* vp, TR::Compilation* comp) :
      _region(region),
      _potentialOpts(region),
      _vp(vp),
      _comp(comp)
   {}

   /** Add different kinds of potential optimization opportunities to the summary **/

   void addIfEq(int32_t paramPosition);
   void addIfNe(int32_t paramPosition);
   void addIfGt(int32_t paramPosition);
   void addIfGe(int32_t paramPosition);
   void addIfLe(int32_t paramPosition);
   void addIfLt(int32_t paramPosition);
   void addIfNull(int32_t paramPosition);
   void addIfNonNull(int32_t paramPosition);

   void addNullCheck(int32_t paramPosition);

   void addInstanceOf(int32_t paramPosition, TR_OpaqueClassBlock* classBlock);

   void addCheckCast(int32_t paraPostion, TR_OpaqueClassBlock* classBlock);
   
   void trace();

   private:
   void add(PotentialOptimization* opt);

   TR::Compilation* comp() { return _comp; }
   TR::Region& region() { return _region; }
   OMR::ValuePropagation* vp() { return _vp; }

   List<PotentialOptimization> _potentialOpts;
   TR::Region &_region;
   OMR::ValuePropagation *_vp;
   TR::Compilation* _comp;
   };

class PotentialOptimization
   {
   public:
   PotentialOptimization(TR::VPConstraint *constraint, int32_t paramPosition) :
      _constraint(constraint),
      _paramPosition(paramPosition)
   {}
   
   virtual void trace(OMR::ValuePropagation *vp, TR::Compilation*comp)=0; 
   
   /**
    * @brief Test whether the given parameter's constraint is a safe value in terms of the optimization's constraint.
    *
    * @param param AbsValue*
    * @param vp OMR::ValuePropagation*
    * 
    * @return bool
    */
   virtual bool predicate(AbsValue* param, OMR::ValuePropagation* vp) { return false; }
   
   virtual int32_t getWeight() { return 1; }

   TR::VPConstraint* getConstraint()  { return _constraint; } 
   int32_t getParamPosition() { return _paramPosition; }

   protected:
   TR::VPConstraint *_constraint;
   int32_t _paramPosition;
   };

class BranchFolding : public PotentialOptimization
   {
   public:
   enum Kinds
      {
      IfEq,
      IfNe,
      IfGt,
      IfGe,
      IfLt, 
      IfLe,
      IfNull,
      IfNonNull,
      };

   BranchFolding(TR::VPConstraint *constraint, int32_t paramPosition, Kinds kind) :
      PotentialOptimization(constraint, paramPosition),
      _kind(kind)
   {}

   virtual bool predicate(AbsValue* param, OMR::ValuePropagation* vp);

   virtual void trace(OMR::ValuePropagation *vp, TR::Compilation*comp); 
   virtual int32_t getWeight() { return 1; };

   private:
   Kinds _kind;

   };

class NullBranchFolding : public BranchFolding
   {
   public:
   NullBranchFolding(TR::VPConstraint* constraint, int32_t paramPosition, Kinds kind):
      BranchFolding(constraint, paramPosition, kind)
   {}

   virtual bool predicate(AbsValue* param, OMR::ValuePropagation* vp);
   virtual int32_t getWeight() { return 1; }
   };

class NullCheckFolding : public PotentialOptimization
   {
   public:
   NullCheckFolding(TR::VPConstraint* constraint, int32_t paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {}

   virtual bool predicate(AbsValue* param, OMR::ValuePropagation* vp);

   virtual void trace(OMR::ValuePropagation *vp, TR::Compilation*comp);
   virtual int32_t getWeight() { return 1; }
   };

class InstanceOfFolding : public PotentialOptimization
   {
   public:
   InstanceOfFolding(TR::VPConstraint* constraint, int32_t paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {};

   virtual bool predicate(AbsValue* param, OMR::ValuePropagation* vp);

   virtual int32_t getWeight() { return 1; }
   virtual void trace(OMR::ValuePropagation* vp, TR::Compilation*comp);
   };

class CheckCastFolding : public PotentialOptimization
   {
   public:
   CheckCastFolding(TR::VPConstraint* constraint, int32_t paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {}

   virtual bool predicate(AbsValue* param, OMR::ValuePropagation* vp);

   virtual int32_t getWeight() { return 1; }
   virtual void trace(OMR::ValuePropagation* vp, TR::Compilation* comp);
   };

#endif