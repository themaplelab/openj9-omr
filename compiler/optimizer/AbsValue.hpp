#ifndef ABS_VALUE_INCL
#define ABS_VALUE_INCL

#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/il/OMRDataTypes.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"

class AbsValue
   {
   public:
      AbsValue(TR::VPConstraint *constraint, TR::DataType dataType);
      AbsValue(AbsValue* other);
      AbsValue *merge(AbsValue *other, TR::Region &region, OMR::ValuePropagation *vp);
      
      void print(OMR::ValuePropagation *vp);
      bool isType2();

      int getParamPosition();
      void setParamPosition(int paramPos);

      TR::DataType getDataType();
      TR::VPConstraint* getConstraint();

   private:
      int _paramPos;
      TR::VPConstraint* _constraint;
      TR::DataType _dataType;
   };

#endif