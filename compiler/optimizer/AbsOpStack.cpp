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

#include "optimizer/AbsOpStack.hpp"
#include "env/Region.hpp"
#include "infra/deque.hpp"

//TODO: can we use maxSize somehow to make memory allocation more efficient?
AbsOpStack::AbsOpStack(TR::Region &region) :
      _stack(StackContainer(0, nullptr, region))
   {
   }


AbsOpStack::AbsOpStack(AbsOpStack &other, TR::Region &region) :
      _stack(other._stack)
   {
   }

void AbsOpStack::push(AbsValue* value)
   {
   TR_ASSERT_FATAL(value, "Push a NULL value");
   _stack.push(value);
   }

void AbsOpStack::pop()
   {
   _stack.pop();
   }

AbsValue* AbsOpStack::top()
   {
   TR_ASSERT_FATAL(size() > 0, "Top an empty stack!");
   return _stack.top(); 
   }

void AbsOpStack::merge(AbsOpStack &other, TR::Region &region, TR::ValuePropagation *valuePropagation)
   {

   TR_ASSERT_FATAL(other._stack.size() == _stack.size(), "stacks are different sizes!");

   StackContainer dequeSelf(region);
   StackContainer dequeOther(region);
  
   //TODO: is there an easier way to merge to stacks?
   int size = _stack.size();
   for (int i = 0; i < size; i++)
      {
      dequeSelf.push_back(top());
      dequeOther.push_back(other.top());
      pop();
      other.pop();
      }
   // cool so now we have the contents of both of these stack in a deque.
   // top is at front and bottom is at back.
   // we want to start merging from the bottom.
   for (int i = 0; i < size; i++)
      {
      AbsValue *valueSelf = dequeSelf.back();
      AbsValue *valueOther = dequeOther.back();
      
      AbsValue *merge = valueSelf->merge(valueOther, region, valuePropagation);
      other.push(valueOther);
      push(merge);
      dequeSelf.pop_back();
      dequeOther.pop_back();
      }
   }

void AbsOpStack::trace(TR::ValuePropagation *vp)
   {
   TR::Compilation *comp = TR::comp();
   traceMsg(comp, "Contents of Abstract Operand Stack:\n");
   int stackSize = size();
   if (stackSize == 0)
      {
      traceMsg(comp, "<empty>\n");
      traceMsg(comp, "\n");
      return;
      }
   AbsValueStack copy(_stack);
   traceMsg(comp, "<top>\n");
   for (int i = 0; i < stackSize; i++) 
      {
      AbsValue *value = copy.top();
      copy.pop();
      traceMsg(comp, "fp[%d] = ", stackSize - i - 1);
      if (value) value->print(vp);
      traceMsg(comp, "\n");
      }
   traceMsg(comp, "<bottom>\n");
   traceMsg(comp, "\n");
   }
