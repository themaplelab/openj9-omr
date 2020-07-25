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


#ifndef ABS_OP_STACK_INCL
#define ABS_OP_STACK_INCL

#include "env/Region.hpp"
#include "infra/ReferenceWrapper.hpp"
#include "infra/deque.hpp"
#include "env/Region.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "optimizer/AbsValue.hpp"

/**
.* Operand Stack simulation.
 */
class AbsOpStack
   {
   public:
   AbsOpStack(TR::Region& region);
   AbsOpStack(AbsOpStack&);

   /**
    * @brief Merge with another AbsOpStack. This is in-place merge.
    *
    * @param other AbsOpStack&
    * @param vp TR::ValuePropagation* 
    * @return void
    */
   void merge(AbsOpStack&, TR::ValuePropagation *);

   /**
    * @brief Push an AbsValue to the AbsOpStack.
    * Note: AbsValue must be non-NULL.
    *
    * @param value AbsValue*
    * @return void
    */
   void push(AbsValue* value);

   /**
    * @brief Get the value on the top of the AbsOpStack.
    *
    * @return AbsValue*
    */
   AbsValue* top();

   /**
    * @brief Get and pop the value on the top of the AbsOpStack.
    *
    * @return AbsValue*
    */
   AbsValue* pop();

   bool empty()  {  return _stack.empty();  };
   size_t size()  {  return _stack.size();  };
  
   void trace(TR::ValuePropagation *);

   private:
   typedef TR::deque<AbsValue*, TR::Region&> StackContainer;
   typedef std::stack<AbsValue*, StackContainer> AbsValueStack;
   
   AbsValueStack _stack; 
   };

#endif

