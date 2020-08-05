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

/**
 * For holding parameters passed from caller method to callee method during Abstract Interpretation.
 */
typedef TR::deque<AbsValue*, TR::Region&> AbsParameterArray;

class AbsState
    {
    public:
    AbsState(TR::Region &region);
    AbsState(AbsState* other, TR::Region& region);

    /**
     * @brief Set an AbsValue at index i of the local variable array in this AbsState.
     *
     * @param i unsigned int
     * @param value AbsValue*
     * @return void
     */
    void set(unsigned int i, AbsValue* value) { _array.set(i, value); };

    /**
     * @brief Get the AbsValue at index i of the local variable array in this AbsState.
     *
     * @param i unsigned int
     * @return AbsValue*
     */
    AbsValue *at(unsigned int i) { return _array.at(i); };

    /**
     * @brief Push an AbsValue to the operand stack in this AbsState.
     *
     * @param value AbsValue*
     * @return void
     */
    void push(AbsValue* value) { _stack.push(value); };

    /**
     * @brief Get and pop the AbsValue at the top of the operand stack in this AbsState.
     * 
     * @return AbsValue*
     */
    AbsValue* pop() { return _stack.pop(); };

    /**
     * @brief Get the AbsValue at the top of the operand stack in this AbsState.
     *
     * @return AbsValue*
     */
    AbsValue* top() { return _stack.top();  };

    /**
     * @brief Merge with another AbsState. This is in-place merge.
     *
     * @param value AbsValue*
     * @param vpOMR::ValuePropagation*
     * @return void
     */
    void merge(AbsState* value,OMR::ValuePropagation* vp);

    size_t getStackSize() {   return _stack.size();  };
    size_t getArraySize() {  return _array.size();  };
    
    void print(TR::Compilation* comp,OMR::ValuePropagation*vp);

    private:
    AbsLocalVarArray _array;
    AbsOpStack _stack;
    };


#endif