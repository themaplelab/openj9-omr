#include "optimizer/AbsValue.hpp"

AbsValue::AbsValue(TR::VPConstraint* constraint, TR::DataType dataType) :
      _constraint(constraint),
      _dataType(dataType),
      _paramPos(-1)
   {
   }

AbsValue* AbsValue::merge(AbsValue *other, TR::Region &region, OMR::ValuePropagation *vp)
   {
   //TR_ASSERT(other->_dt == this->_dt, "different data types");
   // when merging array this isn't true
   if (!this || !other)
      return NULL; // This is necessary :/
   if (_constraint)
      return this;
   if (!other->_constraint)
      return other;
   TR::VPConstraint *constraint = _constraint->merge(other->_constraint, vp);
   AbsValue *absVal = new (region) AbsValue(constraint, _dataType);
   absVal->_paramPos = -1; // modified;
   return absVal;
   }

void AbsValue::print(OMR::ValuePropagation *vp)    
   {
   traceMsg(TR::comp(), "type: %s ", TR::DataType::getName(_dataType));
   if (!_constraint) return;
   traceMsg(TR::comp(), "constraint: ");
   _constraint->print(vp);
   traceMsg(TR::comp(), "param: %d\n", _paramPos);
   }

bool AbsValue::isType2()
   {
   if (!this)
      return false;
   return _dataType == TR::Double || _dataType == TR::Int64;
   }

int AbsValue::getParamPosition()
   {
   return _paramPos;
   }

void AbsValue::setParamPosition(int paramPos)
   {
   _paramPos = paramPos;
   }

TR::VPConstraint* AbsValue::getConstraint()
   {
   return _constraint;
   }