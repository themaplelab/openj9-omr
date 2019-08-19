#pragma once

#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/il/OMRDataTypes.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"

class AbsValue
{
public:
  enum CompareResult
    {
     Narrower = 0,
     Equal,
     Wider,
     Disjoint,
    };
  AbsValue(TR::VPConstraint *vp, TR::DataType dt) : _vp(vp), _dt(dt) {};
  AbsValue *merge(AbsValue *other, TR::Region &region, OMR::ValuePropagation *vp)
    {
    //TR_ASSERT(other->_dt == this->_dt, "different data types");
    // when merging array this isn't true
    if (!this->_vp) return this;
    if (!other->_vp) return other;
    TR::VPConstraint *constraint = this->_vp->merge(other->_vp, vp);
    return new (region) AbsValue(constraint, this->_dt);
    }
  // TODO implement
  CompareResult compareWith(AbsValue *other)
    {
    return CompareResult::Narrower;
    }
  CompareResult mergeComparison(CompareResult a, CompareResult b)
    {
    if (a == b)
      {
      return a;
      }
    if (a == Disjoint || b == Disjoint)
      {
      return Disjoint;
      }
    if (a == Equal)
      {
      return b;
      }
    if (b == Equal)
      {
      return a;
      }
    return Disjoint;
    }
  AbsValue *getWidened(TR::Region &region)
    {
      // TODO use this instead of AbsEnvStatic::getTopDataType
      return new (region) AbsValue(nullptr, _dt);
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

  TR::VPConstraint *_vp;
  TR::DataType _dt;
};
