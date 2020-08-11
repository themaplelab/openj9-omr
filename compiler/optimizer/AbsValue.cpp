/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "optimizer/AbsValue.hpp"

AbsValue::AbsValue(TR::VPConstraint* constraint, TR::DataType dataType, bool isDummy) :
      _constraint(constraint),
      _dataType(dataType),
      _paramPos(-1),
      _isDummy(isDummy),
      _isImplicitParameter(false)
   {
   }

AbsValue::AbsValue(AbsValue* other):
      _constraint(other->_constraint),
      _dataType(other->_dataType),
      _paramPos(other->_paramPos),
      _isDummy(other->_isDummy),
      _isImplicitParameter(other->_isImplicitParameter)
   {
   }

AbsValue* AbsValue::create(TR::VPConstraint *constraint, TR::DataType dataType, TR::Region& region)
   {
   return new (region) AbsValue(constraint, dataType);
   }


AbsValue* AbsValue::createClassObject(TR_OpaqueClassBlock* opaqueClass, bool mustBeNonNull, TR::Compilation*comp, TR::Region& region, OMR::ValuePropagation* vp)
   { 
   TR::VPClassPresence *classPresence = mustBeNonNull? TR::VPNonNullObject::create(vp) : NULL;

   if (opaqueClass != NULL)
      {
      TR::VPClassType *classType = comp->fe()->getClassClassPointer(opaqueClass) ? TR::VPFixedClass::create(vp, opaqueClass) : TR::VPFixedClass::create(vp, opaqueClass);
      return AbsValue::create(TR::VPClass::create(vp, classType, classPresence, NULL, NULL, NULL), TR::Address, region);
      }

   return AbsValue::create(TR::VPClass::create(vp, NULL, classPresence, NULL, NULL, NULL), TR::Address, region);
   }

AbsValue* AbsValue::createNullObject(TR::Region& region, OMR::ValuePropagation* vp)
   {
   return AbsValue::create(TR::VPNullObject::create(vp), TR::Address, region);
   }

AbsValue* AbsValue::createStringConst(TR::SymbolReference* symRef, TR::Region& region, OMR::ValuePropagation* vp)
   {
   return AbsValue::create(TR::VPConstString::create(vp, symRef), TR::Address, region);
   }

AbsValue* AbsValue::createArrayObject(TR_OpaqueClassBlock* arrayClass, bool mustBeNonNull, int32_t lengthLow, int32_t lengthHigh, int32_t elementSize, TR::Compilation*comp, TR::Region& region, OMR::ValuePropagation* vp)
   {
   TR::VPClassPresence *classPresence = mustBeNonNull? TR::VPNonNullObject::create(vp) : NULL;;
   TR::VPArrayInfo *arrayInfo = TR::VPArrayInfo::create(vp, lengthLow, lengthHigh, elementSize);

   if (arrayClass)
      {
      TR::VPClassType *arrayType = comp->fe()->getClassClassPointer(arrayClass) ? TR::VPResolvedClass::create(vp, arrayClass) : TR::VPFixedClass::create(vp, arrayClass);;
      return AbsValue::create(TR::VPClass::create(vp, arrayType, classPresence, NULL, arrayInfo, NULL), TR::Address, region);
      }

   return AbsValue::create(TR::VPClass::create(vp, NULL, classPresence, NULL, arrayInfo, NULL), TR::Address, region);      
   }

AbsValue* AbsValue::createIntConst(int32_t value, TR::Region& region, OMR::ValuePropagation* vp)
   {
   return AbsValue::create(TR::VPIntConst::create(vp, value), TR::Int32, region);
   }

 AbsValue* AbsValue::createLongConst(int64_t value, TR::Region& region, OMR::ValuePropagation* vp)
   {
   return AbsValue::create(TR::VPLongConst::create(vp, value), TR::Int64, region);
   }

AbsValue* AbsValue::createIntRange(int32_t low, int32_t high, TR::Region& region, OMR::ValuePropagation* vp)
   {
   return AbsValue::create(TR::VPIntRange::create(vp, low, high), TR::Int32, region);
   }

AbsValue* AbsValue::createLongRange(int64_t low, int64_t high, TR::Region& region, OMR::ValuePropagation* vp)
   {
   return AbsValue::create(TR::VPLongRange::create(vp, low, high), TR::Int64, region);  
   }

AbsValue* AbsValue::createTopInt(TR::Region& region)
   {
   return AbsValue::create(NULL, TR::Int32, region);
   }

AbsValue* AbsValue::createTopLong(TR::Region& region)
   {
   return AbsValue::create(NULL, TR::Int64, region);
   }

AbsValue* AbsValue::createTopObject(TR::Region& region)
   {
   return AbsValue::create(NULL, TR::Address, region);
   }

AbsValue* AbsValue::createTopFloat(TR::Region& region)
   {
   return AbsValue::create(NULL, TR::Float, region);
   }

AbsValue* AbsValue::createTopDouble(TR::Region& region)
   {
   return AbsValue::create(NULL, TR::Int64, region);
   }

AbsValue* AbsValue::createDummyLong(TR::Region& region)
   {
   return new (region) AbsValue(NULL, TR::Int64, true);
   }

AbsValue* AbsValue::createDummyDouble(TR::Region& region)
   {
   return new (region) AbsValue(NULL, TR::Double, true);
   }

AbsValue* AbsValue::merge(AbsValue *other,OMR::ValuePropagation *vp)
   {
   TR_ASSERT_FATAL(other, "Cannot merge with a NULL AbsValue");

   if (other->getDataType() != getDataType()) //when merging with a different DataType.
      return NULL;

   if (other->isDummy() && isDummy()) //Both dummy
      return this;
   
   if (other->isDummy() || isDummy()) //when merging with a dummy absValue.
      return NULL;

   if (!hasConstraint()) //self does not have a constraint. 
      return this;

   if (!other->hasConstraint()) //Other does not have a constraint.
      {
      setToTop();
      return this;
      }

   TR::VPConstraint *mergedConstraint = getConstraint()->merge(other->getConstraint(), vp);

   if (mergedConstraint) //mergedConstaint can be VPMergedIntConstraint or VPMergedLongConstraint. Need to turn them into VPIntRange or VPLongRange
      {
      if (mergedConstraint->asMergedIntConstraints())
         mergedConstraint = TR::VPIntRange::create(vp, mergedConstraint->getLowInt(), mergedConstraint->getHighInt());
      else if (mergedConstraint->asMergedLongConstraints())
         mergedConstraint = TR::VPLongRange::create(vp, mergedConstraint->getLowLong(), mergedConstraint->getHighLong());
      }

   setConstraint(mergedConstraint);
   return this;
   }

void AbsValue::print(TR::Compilation* comp,OMR::ValuePropagation *vp)    
   {
   traceMsg(comp, "AbsValue: Type: %s ", TR::DataType::getName(_dataType));

   if (isDummy())
      {
      traceMsg(comp, "DUMMY");
      }
   else if (isTop())
      {
      traceMsg(comp, "TOP");  
      }
   
   if (hasConstraint())
      {
      traceMsg(comp, "Constraint: ");
      _constraint->print(vp);
      }

   traceMsg(comp, " param position: %d", _paramPos);
   }
