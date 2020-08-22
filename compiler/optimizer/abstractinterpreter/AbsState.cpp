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

#include "optimizer/abstractinterpreter/AbsState.hpp"

AbsState::AbsState(TR::Region &region) :
      _array(region),
      _stack(region)
   {
   }

AbsState::AbsState(AbsState* other, TR::Region& region ) :
      _array(other->_array, region),
      _stack(other->_stack, region)
   {
   }

void AbsState::merge(AbsState* other, OMR::ValuePropagation *vp)
   {
   _array.merge(other->_array, vp);
   _stack.merge(other->_stack, vp);
   }


void AbsState::print(TR::Compilation* comp, OMR::ValuePropagation *vp)
   {
   traceMsg(comp, "\n|| Contents of AbsState ||\n");
   _array.print(comp,vp);
   _stack.print(comp,vp);
   }