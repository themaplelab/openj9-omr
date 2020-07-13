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


#ifndef ABS_STATE_INCL
#define ABS_STATE_INCL

#include "env/Region.hpp"
#include "AbsLocalVarArray.hpp"
#include "AbsOpStack.hpp"
#include "AbsValue.hpp"

class AbsState
    {
    public:
    AbsState(TR::Region &region);
    AbsState(AbsState* other);

    //For AbsArray
    void set(unsigned int, AbsValue*);
    AbsValue *at(unsigned int);

    //For AbsOpStack
    void push(AbsValue *);
    AbsValue* pop();
    AbsValue* top();
    void merge(AbsState*, TR::ValuePropagation*);
    size_t getStackSize();
    size_t getArraySize();
    void trace(TR::ValuePropagation*);

    private:
    TR::Region& _region;
    AbsLocalVarArray _array;
    AbsOpStack _stack;
    };


#endif