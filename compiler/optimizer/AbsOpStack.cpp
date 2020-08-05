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

#include "optimizer/AbsOpStack.hpp"
#include "env/Region.hpp"
#include "infra/deque.hpp"

AbsOpStack::AbsOpStack(TR::Region &region) :
      _stack(region)
   {
   }


AbsOpStack::AbsOpStack(AbsOpStack &other, TR::Region &region) :
      _stack(region)
   {
   for (size_t i = 0; i < other.size(); i ++)
      {
      push(new (region) AbsValue(other._stack.at(i)));
      }
   }

void AbsOpStack::push(AbsValue* value)
   {
   TR_ASSERT_FATAL(value, "Push a NULL value");
   _stack.push_back(value);
   }

AbsValue* AbsOpStack::pop()
   {
   TR_ASSERT_FATAL(size() > 0, "Pop an empty stack!");
   AbsValue *value = _stack.back();
   _stack.pop_back();
   return value;
   }

AbsValue* AbsOpStack::top()
   {
   TR_ASSERT_FATAL(size() > 0, "Top an empty stack!");
   return _stack.back(); 
   }

void AbsOpStack::merge(AbsOpStack &other,OMR::ValuePropagation *vp)
   {
   TR_ASSERT_FATAL(other._stack.size() == _stack.size(), "Stacks have different sizes!");

   int size = _stack.size();

   AbsValue* selfArray[size];
   AbsValue* otherArray[size];

   for (int i = 0; i < size; i++)
      {
      selfArray[i] = pop();
      otherArray[i] = other.pop();
      }

   for (int i = 0; i < size; i++)
      {
      AbsValue *valueSelf = selfArray[size-i-1];
      AbsValue *valueOther = otherArray[size-i-1];
      
      valueSelf->merge(valueOther, vp);

      push(valueSelf);
      other.push(valueOther);
      }
   }

void AbsOpStack::print(TR::Compilation* comp,OMR::ValuePropagation *vp)
   {
   traceMsg(comp, "Contents of Abstract Operand Stack:\n");
   int stackSize = size();
   if (stackSize == 0)
      {
      traceMsg(comp, "<empty>\n");
      traceMsg(comp, "\n");
      return;
      }
   
   traceMsg(comp, "<top>\n");
   for (int i = 0; i < stackSize; i++) 
      {
      AbsValue *value = _stack.at(stackSize - i - 1);
      traceMsg(comp, "S[%d] = ", stackSize - i - 1);
      if (value)
         value->print(comp, vp);
      traceMsg(comp, "\n");
      }
   traceMsg(comp, "<bottom>\n");
   traceMsg(comp, "\n");
   }
