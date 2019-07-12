#include "compiler/optimizer/AbsEnvStatic.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/env/j9method.h"

AbsEnvStatic::AbsEnvStatic(TR::Region &region, UDATA maxStack, IDATA maxLocals, TR::ValuePropagation *vp) :
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
     TR::VPConstraint* implicitParameterConstraint = this->getClassConstraint(implicitParameterClass);
     this->at(0, implicitParameterConstraint);
     }

  TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*TR::comp());
  TR::ParameterSymbol *p = nullptr;
  for (int i = numberOfImplicitParameters; !parameterIterator->atEnd(); parameterIterator->advanceCursor(), i++)
     {
     auto parameter = parameterIterator;
     TR::DataType dataType = parameter->getDataType();
     switch (dataType) {
        case TR::Int64:
        this->at(i, NULL);
        i = i+1;
        this->at(i, NULL);
        return;
        break;
        case TR::Double:
        this->at(i, NULL);
        i = i+1;
        this->at(i, NULL);
        return;
        break;
        default:
        break;
     }
     const bool isClass = parameter->isClass();
     if (!isClass)
       {
       this->at(i, NULL);
       continue;
       }

     TR_OpaqueClassBlock *parameterClass = parameter->getOpaqueClass();
     if (!parameterClass)
       {
       this->at(i, NULL);
       continue;
       }

      TR::VPConstraint *explicitParameterConstraint = this->getClassConstraint(parameterClass);
      this->at(i, explicitParameterConstraint);
     }
  }

TR::VPConstraint*
AbsEnvStatic::getClassConstraint(TR_OpaqueClassBlock *opaqueClass)
  {
  if (!opaqueClass) return NULL;

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, opaqueClass);
  TR::VPConstraint *classConstraint = TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, NULL);
  return classConstraint;
  }

void
AbsEnvStatic::at(unsigned int index, TR::VPConstraint *constraint)
  {
  this->_array.at(index, constraint);
  }

void
AbsEnvStatic::trace()
  {
  this->_array.trace(this->_vp);
  this->_stack.trace(this->_vp);
  }
