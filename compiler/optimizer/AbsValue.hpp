#pragma once

#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/il/OMRDataTypes.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"


class AbsValue
{
public:
  AbsValue(TR::VPConstraint *vp, TR::DataType dt) : _vp(vp), _dt(dt) {};
  static AbsValue *getBottom()
    {
    static AbsValue bottom(nullptr, TR::NoType);
    return &bottom;
    }
  AbsValue *merge(AbsValue *other, TR::Region &region, OMR::ValuePropagation *vp)
    {
    if (this == other)
      {
      return this;
      }
    if (this == getBottom())
      {
      return other;
      }
    if (other == getBottom())
      {
      return this;
      }
    if (!this->_vp) return this;
    if (!other->_vp) return other;
    TR::VPConstraint *constraint = this->_vp->merge(other->_vp, vp);
    AbsValue *v = new (region) AbsValue(constraint, this->_dt);
    return v;
    }
  void print(OMR::ValuePropagation *vp)
  {
    traceMsg(TR::comp(), "type: %s ", TR::DataType::getName(this->_dt));
    if (!this->_vp) return;
    traceMsg(TR::comp(), "constraint: ");
    this->_vp->print(vp);
  };
  bool isType2() {
    return this->_dt == TR::Double || this->_dt == TR::Int64;
  }
  AbsValue *getWidened(TR::Region &region)
    {
      // TODO only create these values once
      return new (region) AbsValue(NULL, _dt);
    }
  TR::VPConstraint *_vp;
  TR::DataType _dt;
};
