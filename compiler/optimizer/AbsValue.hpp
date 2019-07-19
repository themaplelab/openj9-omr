#pragma once

#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/il/OMRDataTypes.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"

class AbsValue
{
public:
  AbsValue(TR::VPConstraint *vp, TR::DataType dt) : _vp(vp), _dt(dt) {};
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

  TR::VPConstraint *_vp;
  TR::DataType _dt;
};
