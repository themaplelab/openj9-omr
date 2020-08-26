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

#include "optimizer/abstractinterpreter/InliningMethodSummary.hpp"

bool BranchFolding::predicate(AbsValue* param, OMR::ValuePropagation *vp)
   {
   TR::VPConstraint *other = param->getConstraint();

   if (!other)
      return false;

   //Constraint's range cover other' range => safe to fold the branch
   if (_constraint->getLowInt() <= other->getLowInt()
      && _constraint->getHighInt() >= other->getHighInt()) 
      return true;
      
   return false;
   }

bool NullBranchFolding::predicate(AbsValue* param, OMR::ValuePropagation* vp)
   {
   TR::VPConstraint* other = param->getConstraint();

   if (!other)
      return false;

   //null -> branch can be folded
   if (_constraint->isNullObject() && other->isNullObject()) 
      return true; 

   //non-null -> branch can be folded
   if (_constraint->isNonNullObject() && other->isNonNullObject() ) 
      return true;

   return false;
   }
   
bool NullCheckFolding::predicate(AbsValue* param, OMR::ValuePropagation* vp)
   {
   TR::VPConstraint* other = param->getConstraint();

   if (!other)
      return false;

   //Null check an null object -> can be folded into an exception
   if ( _constraint->isNullObject() && other->isNullObject()) 
      return true; 

   //Null check an non-null object -> null check can be folded away
   if (_constraint->isNonNullObject() && other->isNonNullObject()) 
      return true;
      
   return false;
   }

bool InstanceOfFolding::predicate(AbsValue* param, OMR::ValuePropagation* vp)
   {
   TR::VPConstraint* other = param->getConstraint();

   if (!other)
      return false;

   //instance of null object -> can be folded to false
   if (_constraint->isNullObject() && other->isNullObject())
      return true;

   //instance of non-null object. If we know the relationship at compile time, they can be folded.
   if (_constraint->getClass() && other->isNonNullObject() && other->getClass() )
      {
      TR_YesNoMaybe yesNoMaybe = vp->fe()->isInstanceOf(other->getClass(), _constraint->getClass(), other->isFixedClass(), true);

      if (yesNoMaybe == TR_yes || yesNoMaybe == TR_no)
         return true;
      }

   return false;
   }

bool CheckCastFolding::predicate(AbsValue* param, OMR::ValuePropagation* vp)
   {
   TR::VPConstraint* other = param->getConstraint();

   if (!other)
      return false;

   //Checkcast null object -> always success
   if (_constraint->isNullObject() && other->isNullObject())
      return true;
   
   //Checkcast non-null object. Can be folded if we know the relationship at compile time.
   if (_constraint->getClass() && other->isNonNullObject() && other->getClass())
      {
      TR_YesNoMaybe yesNoMaybe = vp->fe()->isInstanceOf(other->getClass(), _constraint->getClass(), other->isFixedClass(), true);

      if (yesNoMaybe == TR_yes || yesNoMaybe == TR_no)
         return true;
      }
      
   return false;
   }

int32_t InliningMethodSummary::predicates(AbsValue* param, int32_t paramPosition)
   {
   TR_ASSERT_FATAL(param, "Param cannot be NULL");

   if (!_potentialOpts.getSize() )  
      return 0;

   if (param->isTop())
      {
      if (comp()->getOption(TR_TraceBISummary))
         traceMsg(comp(), "Constraint to be compared:\nTOP (unknown)\n");
      return 0;
      }

   TR::VPConstraint* constraint = param->getConstraint();

   int32_t i = 0;
   ListIterator<PotentialOptimization> iter(&_potentialOpts);
   PotentialOptimization *opt = iter.getFirst();
   while (opt)
      {
      if (opt->getParamPosition() == paramPosition)
         {
         if (comp()->getOption(TR_TraceBISummary))
            {
            traceMsg(comp(), ">>>Predicate Parameter at Position %d\n", paramPosition);
            traceMsg(comp(), "Param: ");
            constraint->print(vp());
            traceMsg(comp(), "\nVS \n");
            opt->trace(vp(), comp());
            }
         
         bool succuess =  opt->predicate(param, vp());

         if (comp()->getOption(TR_TraceBISummary))
            {
            if (succuess)
               traceMsg(comp(), "Predicate: TRUE\n");
            else
               traceMsg(comp(), "Predicate: FALSE\n");
            traceMsg(comp(), "\n");
            }

         i += succuess ? opt->getWeight() : 0;

         }
      opt = iter.getNext();
      }
   return i;
   }

void InliningMethodSummary::trace()
   {
   if (_potentialOpts.getSize() == 0)
      {
      traceMsg(comp(), "Inlining Method Summary EMPTY\n");
      return;
      }

   traceMsg(comp(), "Inlining Method Summaries:\n\n");

   ListIterator<PotentialOptimization> iter(&_potentialOpts);
   PotentialOptimization *opt = iter.getFirst();

   while (opt) 
      {
      opt->trace(vp(), comp());
      opt = iter.getNext();
      }
    traceMsg(comp(), "----------------------------------------------\n\n");
   }

void BranchFolding::trace(OMR::ValuePropagation *vp, TR::Compilation* comp)
   {
   char* branchFoldingName;

   switch (_kind)
      {
      case Kinds::IfEq:
         branchFoldingName = "IfEq";
         break;

      case Kinds::IfGe:
         branchFoldingName = "IfGe";
         break;

      case Kinds::IfGt:
         branchFoldingName = "IfGt";
         break;
      
      case Kinds::IfLe:
         branchFoldingName = "IfLe";
         break;

      case Kinds::IfLt:
         branchFoldingName = "IfLt";
         break;

      case Kinds::IfNe:
         branchFoldingName = "IfNe";
         break;

      case Kinds::IfNonNull:
         branchFoldingName = "IfNonNull";
         break;

      case Kinds::IfNull:
         branchFoldingName = "IfNull";
         break;
      
      default:
         branchFoldingName = "Unknown";
         break;
      }

   traceMsg(comp, "Branch Folding %s for argument %d constraint: ", branchFoldingName, _paramPosition);
   _constraint->print(vp);
   traceMsg(comp, "\n");
   }

void NullCheckFolding::trace(OMR::ValuePropagation *vp, TR::Compilation* comp)
   {
   traceMsg(comp, "Null Check Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(comp, "\n");
   }

void InstanceOfFolding::trace(OMR::ValuePropagation *vp, TR::Compilation* comp)
   {
   traceMsg(comp, "Instanceof Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(comp, "\n");
   }

void CheckCastFolding::trace(OMR::ValuePropagation *vp, TR::Compilation* comp)
   {
   traceMsg(comp, "Checkcast Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(comp, "\n");
   }

void InliningMethodSummary::addIfEq(int32_t paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntConst::create(vp(), 0), paramPosition, BranchFolding::Kinds::IfEq);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfEq);
   BranchFolding* p3 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfEq);

   add(p1);
   add(p2);
   add(p3);
   }

void InliningMethodSummary::addIfNe(int32_t paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntConst::create(vp(), 0), paramPosition, BranchFolding::Kinds::IfNe);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfNe);
   BranchFolding* p3 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfNe);

   add(p1);
   add(p2);
   add(p3);
   }

void InliningMethodSummary::addIfLt(int32_t paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfLt);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), 0, INT_MAX), paramPosition,  BranchFolding::Kinds::IfLt);

   add(p1);
   add(p2);
   }

void InliningMethodSummary::addIfLe(int32_t paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), INT_MIN, 0), paramPosition, BranchFolding::Kinds::IfLe);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfLe);

   add(p1);
   add(p2);
   }

void InliningMethodSummary::addIfGt(int32_t paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), INT_MIN, 0), paramPosition, BranchFolding::Kinds::IfGt);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfGt);

   add(p1);
   add(p2);
   }

void InliningMethodSummary::addIfGe(int32_t paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfGe);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(vp(), 0, INT_MAX), paramPosition, BranchFolding::Kinds::IfGe);

   add(p1);
   add(p2);
   }

void InliningMethodSummary::addIfNull(int32_t paramPosition)
   {
   NullBranchFolding* p1 = new (region()) NullBranchFolding(TR::VPNullObject::create(vp()), paramPosition, BranchFolding::Kinds::IfNull);
   NullBranchFolding* p2 = new (region()) NullBranchFolding(TR::VPNonNullObject::create(vp()), paramPosition, BranchFolding::Kinds::IfNull);

   add(p1);
   add(p2);
   }

void InliningMethodSummary::addIfNonNull(int32_t paramPosition)
   {
   NullBranchFolding* p1 = new (region()) NullBranchFolding(TR::VPNullObject::create(vp()), paramPosition, BranchFolding::Kinds::IfNonNull);
   NullBranchFolding* p2 = new (region()) NullBranchFolding(TR::VPNonNullObject::create(vp()), paramPosition, BranchFolding::Kinds::IfNonNull);

   add(p1);
   add(p2);
   }

void InliningMethodSummary::addNullCheck(int32_t paramPosition)
   {
   NullCheckFolding* p1 = new (region()) NullCheckFolding(TR::VPNullObject::create(vp()), paramPosition);
   NullCheckFolding* p2 = new (region()) NullCheckFolding(TR::VPNonNullObject::create(vp()), paramPosition);

   add(p1);
   add(p2);
   }

void InliningMethodSummary::addInstanceOf(int32_t paramPosition, TR_OpaqueClassBlock* classBlock)
   {
   if (classBlock == NULL)
      {
      InstanceOfFolding* p1 = new (region()) InstanceOfFolding(TR::VPNullObject::create(vp()), paramPosition);
      add(p1);
      }
   else
      {
      InstanceOfFolding* p1 = new (region()) InstanceOfFolding(TR::VPNullObject::create(vp()), paramPosition);
      InstanceOfFolding* p2 = new (region()) InstanceOfFolding(TR::VPFixedClass::create(vp(), classBlock), paramPosition);
      add(p1);
      add(p2);
      }
   }

void InliningMethodSummary::addCheckCast(int32_t paramPosition, TR_OpaqueClassBlock* classBlock)
   {
   if (classBlock == NULL)
      {
      CheckCastFolding* p1 = new (region()) CheckCastFolding(TR::VPNullObject::create(vp()), paramPosition);
      add(p1);
      }
   else
      {
      CheckCastFolding* p1 = new (region()) CheckCastFolding(TR::VPNullObject::create(vp()), paramPosition);
      CheckCastFolding* p2 = new (region()) CheckCastFolding(TR::VPFixedClass::create(vp(), classBlock), paramPosition);
      add(p1);
      add(p2);
      }
   }

void InliningMethodSummary::add(PotentialOptimization *potentialOpt)
   {
   _potentialOpts.add(potentialOpt); 
   }