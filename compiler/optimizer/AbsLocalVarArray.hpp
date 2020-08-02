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


#ifndef ABS_VAR_ARRAY_INCL
#define ABS_VAR_ARRAY_INCL

#include "env/Region.hpp"
#include "infra/ReferenceWrapper.hpp"
#include "infra/deque.hpp"
#include "env/Region.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "optimizer/AbsValue.hpp"

/**
.* Local Variable Array Simulation.
 */
class AbsLocalVarArray
   {
   public:
   AbsLocalVarArray(TR::Region &region);
   AbsLocalVarArray(AbsLocalVarArray&, TR::Region& region);

   /**
    * @brief Merge with another AbsLocalVarArray. This is in-place merge.
    *
    * @param other AbsLocalVarArray&
    * @param vp TR::ValuePropagation* 
    * @return void
    */
   void merge(AbsLocalVarArray& other, TR::ValuePropagation* vp);
   
   /**
    * @brief Get the AbsValue at index i
    *
    * @param i unsigned int
    * @return AbsValue*
    */
   AbsValue *at(unsigned int i);

   /**
    * @brief Set the AbsValue at index i
    *
    * @param i unsigned int
    * @param value AbsValue*
    * @return void
    */
   void set(unsigned int i, AbsValue* value);

   size_t size() { return _array.size(); };
   void trace(TR::ValuePropagation *vp);

   private:
   typedef TR::deque<AbsValue*, TR::Region&> AbsValueArray;
   AbsValueArray _array;
   };

#endif