#include "compiler/optimizer/AbsEnvStatic.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/env/j9method.h"
#include "compiler/optimizer/AbsValue.hpp"

AbsEnvStatic::AbsEnvStatic(TR::Region &region, UDATA maxStack, IDATA maxLocals, TR::ValuePropagation *vp) :
  _region(region),
  _array(region, maxLocals),
  _stack(region, maxStack),
  _vp(vp)
{
}

void
AbsEnvStatic::enterMethod(TR::ResolvedMethodSymbol *rms)
  {
  TR_ResolvedMethod *resolvedMethod = rms->getResolvedMethod();
  const auto numberOfParameters = resolvedMethod->numberOfParameters();
  const auto numberOfExplicitParameters = resolvedMethod->numberOfExplicitParameters();
  const auto numberOfImplicitParameters = numberOfParameters - numberOfExplicitParameters;
  const auto hasImplicitParameter = numberOfImplicitParameters > 0;

  if (hasImplicitParameter)
     {
     TR_OpaqueClassBlock *implicitParameterClass = resolvedMethod->containingClass();
     AbsValue* value = this->getClassConstraint(implicitParameterClass);
     this->at(0, value);
     }

  TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*TR::comp());
  TR::ParameterSymbol *p = nullptr;
  for (int i = numberOfImplicitParameters; !parameterIterator->atEnd(); parameterIterator->advanceCursor(), i++)
     {
     auto parameter = parameterIterator;
     TR::DataType dataType = parameter->getDataType();
     switch (dataType) {
        case TR::Int8:
          this->at(i, new (_region) AbsValue(NULL, TR::Int8));
          continue;
        break;

        case TR::Int16:
          this->at(i, new (_region) AbsValue(NULL, TR::Int16));
          continue;
        break;

        case TR::Int32:
          this->at(i, new (_region) AbsValue(NULL, TR::Int32));
          continue;
        break;

        case TR::Int64:
          this->at(i, new (_region) AbsValue(NULL, TR::Int64));
          i = i+1;
          this->at(i, new (_region) AbsValue(NULL, TR::NoType));
          continue;
        break;

        case TR::Float:
          this->at(i, new (_region) AbsValue(NULL, TR::Float));
          continue;
        break;

        case TR::Double:
          this->at(i, new (_region) AbsValue(NULL, TR::Double));
          i = i+1;
          this->at(i, new (_region) AbsValue(NULL, TR::NoType));
        continue;

        //TODO: what about vectors and aggregates?
        break;
        default:
        break;
     }
     const bool isClass = parameter->isClass();
     if (!isClass)
       {
       this->at(i, new (_region) AbsValue(NULL, TR::Address));
       continue;
       }

     TR_OpaqueClassBlock *parameterClass = parameter->getOpaqueClass();
     if (!parameterClass)
       {
       this->at(i, new (_region) AbsValue(NULL, TR::Address));
       continue;
       }

      AbsValue *value = this->getClassConstraint(parameterClass);
      this->at(i, value);
     }
  }

AbsValue*
AbsEnvStatic::getClassConstraint(TR_OpaqueClassBlock *opaqueClass)
  {
  if (!opaqueClass) return NULL;

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, opaqueClass);
  TR::VPConstraint *classConstraint = TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, NULL);
  return new (_region) AbsValue (classConstraint, TR::Address);
  }

void
AbsEnvStatic::at(unsigned int index, AbsValue *constraint)
  {
  this->_array.at(index, constraint);
  }

//TODO, no strings here...
// We need to unnest IDT::Node
void
AbsEnvStatic::trace(const char* methodName)
  {
  traceMsg(TR::comp(), "method %s\n", methodName);
  this->_array.trace(this->_vp);
  this->_stack.trace(this->_vp);
  }
