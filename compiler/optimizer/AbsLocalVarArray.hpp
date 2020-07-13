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


#ifndef ABS_VAR_ARRAY_INCL
#define ABS_VAR_ARRAY_INCL

#include "env/Region.hpp"
#include "infra/ReferenceWrapper.hpp"
#include "infra/deque.hpp"
#include "env/Region.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "optimizer/AbsValue.hpp"

//TODO: can we inherit instead of encapsulating?
//TODO: use AbsValue instead of VPConstraint
class AbsLocalVarArray
   {
   public:
   AbsLocalVarArray(TR::Region &region);
   AbsLocalVarArray(AbsLocalVarArray&, TR::Region&);
   void merge(AbsLocalVarArray&, TR::Region&, TR::ValuePropagation* );
   void trace(TR::ValuePropagation *vp);
   

   size_t size() { return _array.size(); };
   AbsValue *at(unsigned int index) {  TR_ASSERT_FATAL(index < size(), "Index out of range!"); return _array.at(index);  };
   void set(unsigned int, AbsValue*);
   
   private:
   typedef TR::deque<AbsValue*, TR::Region&> AbsValueArray;
   AbsValueArray _array;
   };

#endif