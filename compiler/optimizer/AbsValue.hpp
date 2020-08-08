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
    * A set of methods for creating different kinds of AbsValue.
    * OMR::ValuePropagation is required as a parameter since we are using TR::VPConstraint as the constraint.
    * Though it has nothing to do with the Value Propagation optimization.
    */
   static AbsValue* create(TR::VPConstraint *constraint, TR::DataType dataType, TR::Region& region);
   
   static AbsValue* createClassObject(TR_OpaqueClassBlock* opaqueClass, bool mustBeNonNull, TR::Compilation*comp, TR::Region& region, OMR::ValuePropagation* vp);

   static AbsValue* createNullObject(TR::Region& region, OMR::ValuePropagation* vp);
   static AbsValue* createArrayObject(TR_OpaqueClassBlock* arrayClass, bool mustBeNonNull, int32_t lengthLow, int32_t lengthHigh, int32_t elementSize, TR::Compilation*comp, TR::Region& region, OMR::ValuePropagation* vp);
   
   static AbsValue* createStringConst(TR::SymbolReference* symRef, TR::Region& region, OMR::ValuePropagation* vp);
   static AbsValue* createIntConst(int32_t value, TR::Region& region, OMR::ValuePropagation* vp);
   static AbsValue* createLongConst(int64_t value, TR::Region& region, OMR::ValuePropagation* vp);
   static AbsValue* createIntRange(int32_t low, int32_t high, TR::Region& region, OMR::ValuePropagation* vp);
   static AbsValue* createLongRange(int64_t low, int64_t high, TR::Region& region, OMR::ValuePropagation* vp);

   static AbsValue* createTopInt(TR::Region& region);
   static AbsValue* createTopLong(TR::Region& region);

   static AbsValue* createTopObject(TR::Region& region);

   static AbsValue* createTopDouble(TR::Region& region);
   static AbsValue* createTopFloat(TR::Region& region);

   static AbsValue* createDummyDouble(TR::Region& region);
   static AbsValue* createDummyLong(TR::Region& region);

   
   /**
    * @brief Merge with another AbsValue. 
    * Note: This is in-place merge. 
    * Other should have the exact SAME dataType as self. 
    * Any type promotion, such as Int16 => Int32 should be handled outside before calling merge() since this is not handled inside this method.
    *
    * @param other AbsValue*
    * @param vp OMR::ValuePropagation* 
    * @return AbsValue*
    * 
    * The return value is self or NULL.
    * When succeeding to merge, it returns self.
    * NULL denotes the merge fails (when merging with different dataTypes or merging with a Dummy AbsValue).
    */
   AbsValue* merge(AbsValue *other, OMR::ValuePropagation *vp);

   /**
    * @brief Check if the AbsValue is Top.
    * 
    * Top is a notion in lattice theory, denoting the 'maximum'.
    * Though this method seems doing the same thing as hasConstriant().
    * 
    * A Top value, theoretically, can still have a constraint.
    * For example, a Top Int has the constraint <INT_MIN, INT_MAX>
    * However, by the implementation of TR::VPConstaint, we are not allowed to create an Int constraint <INT_MIN, INT_MAX>.
    * 
    * This method should be re-implmented in the future in case we are not using TR::VPConstaint as the constraint.
    *
    * @return bool
    */
   bool isTop() { return _constraint == NULL; };

   /**
    * @brief Check if the AbsValue has a constraint
    *
    * @return bool
    */
   bool hasConstraint() { return _constraint != NULL; };

   /**
    * @brief Set the constriant to unknown.
    *
    * @return void
    */
   void setToTop() { setConstraint(NULL); };

   /**
    * @brief Check if the AbsValue is 64-bit sized data. (Two 32-bit words)
    *
    * @return bool
    */
   bool isType2() { return _dataType.isDouble() || _dataType.isInt64(); };

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

   /**
    * @brief Check if the AbsValue is 'this' (the implicit parameter.)
    *
    * @return bool
    */   
   bool isImplicitParam() { return _paramPos == 0 && _isImplicitParam; };

   /* Getter and setter methods */

   int getParamPosition() { return _paramPos; };
   void setParamPosition(int paramPos) { _paramPos = paramPos; };

   void setImplicitParam() { TR_ASSERT_FATAL(_paramPos == 0, "Cannot set as implicit param"); _isImplicitParam = true; };

   TR::DataType getDataType() { return _dataType; };
   
   TR::VPConstraint* getConstraint() { return _constraint; };


   void print(TR::Compilation* comp,OMR::ValuePropagation *vp);

   private:

   void setDataType(TR::DataType dataType) { _dataType = dataType; };
   void setConstraint(TR::VPConstraint *constraint) { _constraint = constraint; };

   bool _isImplicitParam;
   bool _isDummy;
   int _paramPos; 
   TR::VPConstraint* _constraint;
   TR::DataType _dataType;
   };


#endif