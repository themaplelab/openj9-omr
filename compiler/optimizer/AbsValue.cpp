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

AbsValue::AbsValue(TR::VPConstraint* constraint, TR::DataType dataType) :
      _constraint(constraint),
      _dataType(dataType),
      _paramPos(-1)
   {
   }

AbsValue::AbsValue(AbsValue* other):
      _constraint(other->_constraint),
      _dataType(other->_dataType),
      _paramPos(other->_paramPos)
   {
   }

void AbsValue::merge(AbsValue *other, TR::ValuePropagation *vp)
   {
   TR_ASSERT_FATAL(other, "Cannot merge with a NULL AbsValue");
   
   //If two values to be merged are not the same type. Set as TOP with dataType to be TR::NoType
   //This can happen when merging two basic blocks in two different scopes.
   if (other->getDataType() != getDataType())
      {
      setConstraint(NULL);
      setDataType(TR::NoType);
      return;
      }

   if (isTOP())
      return;

   if (other->isTOP())
      {
      setConstraint(NULL);
      return;
      }
     
   TR::VPConstraint *mergedConstraint = getConstraint()->merge(other->getConstraint(), vp);
   setConstraint(mergedConstraint);
   }

void AbsValue::print(TR::ValuePropagation *vp)    
   {
   traceMsg(TR::comp(), "AbsValue: type: %s ", TR::DataType::getName(_dataType));
   if (isTOP())
      {
      traceMsg(TR::comp(), "TOP");
      }
   else 
      {
      traceMsg(TR::comp(), "Constraint: ");
      _constraint->print(vp);
      }

   traceMsg(TR::comp(), " param position: %d", _paramPos);
   }
