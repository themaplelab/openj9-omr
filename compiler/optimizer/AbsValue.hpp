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

#ifndef ABS_VALUE_INCL
#define ABS_VALUE_INCL

#include "optimizer/VPConstraint.hpp"
#include "il/OMRDataTypes.hpp"
#include "optimizer/ValuePropagation.hpp"

/**
 * AbsValue is the basic unit used to perform abstract interpretation.
 */
class AbsValue
   {
   public:
   AbsValue(TR::VPConstraint *constraint, TR::DataType dataType, bool isDummy=false);
   AbsValue(AbsValue* other);
   
   /**
    * @brief Merge with another AbsValue. This is in-place merge.
    *
    * @param other AbsValue*
    * @param vp TR::ValuePropagation* 
    * @return void
    */
   void merge(AbsValue *other, TR::ValuePropagation *vp);

   /**
    * @brief Check if the AbsValue is TOP. 
    * 'TOP' is a notion in lattice theory, denoting the 'maximum' in the lattice. 
    *
    * @return bool
    */
   bool isTOP() { return _constraint == NULL; }; 

   bool hasConstraint() { return _constraint != NULL; };

   /**
    * @brief Check if the AbsValue is 64-bit sized data. (Two 32-bit words)
    *
    * @return bool
    */
   bool isType2() { return _dataType == TR::Double || _dataType == TR::Int64; };

   /**
    * @brief Check if the AbsValue is only used to take a slot as the second 32-bit part of the 64-bit data types
    *
    * @return bool
    */
   bool isDummy() { return _isDummy; }

   /**
    * @brief Check if the AbsValue is a parameter.
    *
    * @return bool
    */
   bool isParameter() { return _paramPos >= 0; };

   /* Getter and setter methods */

   int getParamPosition() { return _paramPos; };
   void setParamPosition(int paramPos) { _paramPos = paramPos; };

   TR::DataType getDataType() { return _dataType; };
   void setDataType(TR::DataType dataType) { _dataType = dataType; };

   TR::VPConstraint* getConstraint() { return _constraint; };
   void setConstraint(TR::VPConstraint *constraint) { _constraint = constraint; };

   void print(TR::ValuePropagation *vp);

   private:
   bool _isDummy;
   int _paramPos; 
   TR::VPConstraint* _constraint;
   TR::DataType _dataType;
   };


#endif