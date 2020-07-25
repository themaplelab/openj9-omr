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

#include "optimizer/AbsLocalVarArray.hpp"

AbsLocalVarArray::AbsLocalVarArray(TR::Region &region) :
      _array(region)
   {
   }

AbsLocalVarArray::AbsLocalVarArray(AbsLocalVarArray &other) :
      _array(other._array)
   {
   }

void AbsLocalVarArray::merge(AbsLocalVarArray &other, TR::ValuePropagation *vp)
   {
   int otherSize = other.size();
   int selfSize = size();
   int mergedSize = std::max(selfSize, otherSize);

   for (int i = 0; i < mergedSize; i++)
      {
      AbsValue *selfValue = i < size() ? at(i) : NULL;
      AbsValue *otherValue = i < other.size() ? other.at(i) : NULL;

      if (!selfValue && !otherValue) 
         {
         continue;
         }
      else if (selfValue && otherValue) 
         {
         selfValue->merge(otherValue, vp);
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

void AbsLocalVarArray::set(unsigned int index, AbsValue *absValue)
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
   
AbsValue* AbsLocalVarArray::at(unsigned int index)
   {
   TR_ASSERT_FATAL(index < size(), "Index out of range!");
   return _array.at(index); 
   }

void AbsLocalVarArray::trace(TR::ValuePropagation *vp)
   {
   TR::Compilation *comp = TR::comp();
   traceMsg(comp, "Contents of Abstract Local Variable Array:\n");
   int arraySize = size();
   for (int i = 0; i < arraySize; i++)
      {
      traceMsg(comp, "A[%d] = ", i);
      if (!at(i))
         {
         traceMsg(comp, "NULL\n");
         continue;
         }
      at(i)->print(vp);
      traceMsg(comp, "\n");
      }
   traceMsg(comp, "\n");
   }
