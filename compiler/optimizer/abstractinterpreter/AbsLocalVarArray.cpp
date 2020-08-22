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

#include "optimizer/abstractinterpreter/AbsLocalVarArray.hpp"

AbsLocalVarArray::AbsLocalVarArray(TR::Region &region) :
      _array(region)
   {
   }

AbsLocalVarArray::AbsLocalVarArray(AbsLocalVarArray &other, TR::Region& region) :
      _array(region)
   {
   for (size_t i = 0; i < other._array.size(); i ++)
      {
      if (other._array.at(i))
         {
         _array.push_back(AbsValue::create(other._array.at(i), region));
         }
      else
         {
         _array.push_back(NULL);
         }
      }
   }

void AbsLocalVarArray::setToTop()
   {
   size_t size = _array.size();
   for (size_t i = 0; i < size; i ++)
      {
      if (_array.at(i))
         _array.at(i)->setToTop();
      }
   }
   
void AbsLocalVarArray::merge(AbsLocalVarArray &other, OMR::ValuePropagation *vp)
   {
   int32_t otherSize = other.size();
   int32_t selfSize = size();
   int32_t mergedSize = std::max(selfSize, otherSize);

   for (int32_t i = 0; i < mergedSize; i++)
      {
      AbsValue *selfValue = i < size() ? at(i) : NULL;
      AbsValue *otherValue = i < other.size() ? other.at(i) : NULL;

      if (!selfValue && !otherValue) 
         {
         continue;
         }
      else if (selfValue && otherValue) 
         {
         AbsValue* mergedVal = selfValue->merge(otherValue, vp);
         set(i, mergedVal);
         } 
      else if (selfValue) 
         {
         set(i, selfValue);
         } 
      else if (otherValue)
         {
         set(i, otherValue);
         }
      }
   }

void AbsLocalVarArray::set(int32_t index, AbsValue *absValue)
   {
   if (size() > index)
      {
      _array.at(index) = absValue;
      return;
      }

   if (index == size())
      {
      _array.push_back(absValue);
      return;
      }
      
   while (size() < index)
      {
      _array.push_back(NULL);
      }

      _array.push_back(absValue);
   }
   
AbsValue* AbsLocalVarArray::at(int32_t index)
   {
   TR_ASSERT_FATAL(index < size(), "Index out of range!");
   return _array.at(index); 
   }

void AbsLocalVarArray::print(TR::Compilation* comp, OMR::ValuePropagation *vp)
   {
   traceMsg(comp, "Contents of Abstract Local Variable Array:\n");
   int32_t arraySize = size();
   for (int32_t i = 0; i < arraySize; i++)
      {
      traceMsg(comp, "A[%d] = ", i);
      if (!at(i))
         {
         traceMsg(comp, "NULL\n");
         continue;
         }
      at(i)->print(comp, vp);
      traceMsg(comp, "\n");
      }
   traceMsg(comp, "\n");
   }

