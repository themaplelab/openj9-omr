#pragma once

#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/il/OMRDataTypes.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"

// TODO refactor, see comment for fields at bottom of class
class DependencyNode; 

  // This should be a static method in DependencyNode
    static DependencyNode *dmerge(DependencyNode *a, DependencyNode *b)
        {
        if (a == b)
            {
            return a;
            }
        return NULL;
        }

class AbsValue;
class MergeOperation
    {
    public:
    virtual AbsValue *merge(AbsValue *a, AbsValue *b) = 0;
    };

class AbsValue
{
public:
  AbsValue(TR::VPConstraint *vp, TR::DataType dt) : _vp(vp), _dt(dt) {};
  static AbsValue *getBottom()
    {
    static AbsValue bottom(nullptr, TR::NoType);
    return &bottom;
    }
  AbsValue *merge(MergeOperation *op, AbsValue *other, TR::Region &region, OMR::ValuePropagation *vp)
    {
    return op->merge(this, other);
    }
  AbsValue *merge(AbsValue *other, TR::Region &region, OMR::ValuePropagation *vp)
    {
    // This is required for narrowing abs values form backedges
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
    //TR_ASSERT(other->_dt == this->_dt, "different data types");
    // when merging array this isn't true
    if (!this->_vp) return this;
    if (!other->_vp) return other;
    TR::VPConstraint *constraint = this->_vp->merge(other->_vp, vp);
    // TODO refactor depends logic elsewhere
    AbsValue *v = new (region) AbsValue(constraint, this->_dt);
    v->setDependsOn(dmerge(this->_dependency, other->_dependency));
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
  // TODO refactor following members into own class when absvalue is templated in semantics
  DependencyNode *_dependency = nullptr;
  void setDependsOn(DependencyNode *d)
    {
    // Top does not properly track dependencies
    if (_vp != NULL)
      {
      _dependency = d;
      }
    }
};
