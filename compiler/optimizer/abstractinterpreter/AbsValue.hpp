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
    * @brief Merge with another AbsValue. 
    * Note: This is in-place merge. 
    * Other should have the exactly SAME dataType as self. 
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
    * @brief Check whether the AbsValue has no constraint
    *
    * @return bool
    */
   bool isTop() { return _constraint == NULL; }

   /**
    * @brief Set the constriant to unknown.
    *
    * @return void
    */
   void setToTop() { _constraint = NULL; }

   /**
    * @brief Check if the AbsValue is only used to take a slot as the second word of the 64-bit data types
    *
    * @return bool
    */
   bool isDummy() { return _isDummy; }

   /**
    * @brief Check if the AbsValue is a parameter.
    *
    * @return bool
    */
   bool isParameter() { return _paramPos >= 0; }

   /**
    * @brief Check if the AbsValue is 'this' (the implicit parameter.)
    *
    * @return bool
    */   
   bool isImplicitParameter() { return _paramPos == 0 && _isImplicitParameter; }

   /**
    * A set of methods for creating different kinds of AbsValue.
    * OMR::ValuePropagation is required as a parameter since we are using TR::VPConstraint as the constraint.
    * Though it has nothing to do with the Value Propagation optimization.
    */
   static AbsValue* create(TR::VPConstraint *constraint, TR::DataType dataType, TR::Region& region);

   static AbsValue* create(AbsValue* other, TR::Region& region);
   
   static AbsValue* createClassObject(TR_OpaqueClassBlock* opaqueClass, bool mustBeNonNull, TR::Region& region, OMR::ValuePropagation* vp);

   static AbsValue* createNullObject(TR::Region& region, OMR::ValuePropagation* vp);
   static AbsValue* createArrayObject(TR_OpaqueClassBlock* arrayClass, bool mustBeNonNull, int32_t lengthLow, int32_t lengthHigh, int32_t elementSize, TR::Region& region, OMR::ValuePropagation* vp);
   
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

   bool isNullObject() { return _constraint && _constraint->isNullObject(); }
   bool isNonNullObject() { return _constraint && _constraint->isNonNullObject(); }

   bool isArrayObject() { return _constraint && _constraint->asClass() && _constraint->getArrayInfo(); }
   bool isClassObject() { return _constraint && _constraint->asClass() && !_constraint->getArrayInfo(); }
   bool isStringConst() { return _constraint && _constraint->asConstString(); }
   bool isObject() { return _constraint && (_constraint->asClass() || _constraint->asConstString()); }

   bool isIntConst() { return _constraint && _constraint->asIntConst(); }
   bool isIntRange() { return _constraint && _constraint->asIntRange(); }
   bool isInt() { return _constraint && _constraint->asIntConstraint(); }

   bool isLongConst() { return _constraint && _constraint->asLongConst(); }
   bool isLongRange() { return _constraint && _constraint->asLongRange(); }
   bool isLong() { return _constraint && _constraint->asLongConstraint(); }

   /* Getter and setter methods */

   int32_t getParamPosition() { return _paramPos; }
   void setParamPosition(int paramPos) { _paramPos = paramPos; }

   void setImplicitParam() { TR_ASSERT_FATAL(_paramPos == 0, "Cannot set as implicit param"); _isImplicitParameter = true; }

   TR::DataType getDataType() { return _dataType; }
   
   TR::VPConstraint* getConstraint() { return _constraint; }

   void print(TR::Compilation* comp,OMR::ValuePropagation *vp);

   private:

   bool _isImplicitParameter;
   bool _isDummy;
   int _paramPos; 
   TR::VPConstraint* _constraint;
   TR::DataType _dataType;
   };


#endif