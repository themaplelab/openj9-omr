#pragma once

#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/il/OMRDataTypes.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"

class AbsValue
{
public:
  AbsValue(TR::VPConstraint *vp, TR::DataType dt) : _vp(vp), _dt(dt), _param(-1) {};
  AbsValue *merge(AbsValue *other, TR::Region &region, OMR::ValuePropagation *vp)
    {
    //TR_ASSERT(other->_dt == this->_dt, "different data types");
    // when merging array this isn't true
    if (!this || !other) return NULL; // This is necessary :/
    if (!this->_vp) return this;
    if (!other->_vp) return other;
    TR::VPConstraint *constraint = this->_vp->merge(other->_vp, vp);
    AbsValue *absVal = new (region) AbsValue(constraint, this->_dt);
    absVal->_param = -1; // modified;
    return absVal;
    }
  void print(OMR::ValuePropagation *vp)
  {
    traceMsg(TR::comp(), "type: %s ", TR::DataType::getName(this->_dt));
    if (!this->_vp) return;
    traceMsg(TR::comp(), "constraint: ");
    this->_vp->print(vp);
    traceMsg(TR::comp(), "param: %d\n", _param);
  };
  bool isType2() {
    if (!this) return false;
    return this->_dt == TR::Double || this->_dt == TR::Int64;
  }

  int _param;
  TR::VPConstraint *_vp;
  TR::DataType _dt;
};