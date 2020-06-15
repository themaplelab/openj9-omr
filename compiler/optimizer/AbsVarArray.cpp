#include "AbsVarArray.hpp"

#include "AbsEnv.hpp"
#include <algorithm>

#include "compiler/optimizer/VPConstraint.hpp"
#include "compile/Compilation.hpp"

#ifndef TRACE
//#define TRACE(COND, M, ...) 
#define TRACE(COND, M, ...) \
if ((COND)) { traceMsg(this->_comp, M, ##__VA_ARGS__); }
#endif


AbsVarArray::AbsVarArray(TR::Compilation *comp, TR::Region& region) :
  _region(region),
  _comp(comp)
{
        //this->_base = new(*this->_region) TR::VPConstraint*[32];
        for (int i = 0; i < 32; i++) {
          this->_base[i] = NULL;
        }
}

AbsVarArray::AbsVarArray(const AbsVarArray &absVarArray) :
  _region(absVarArray._region),
  _comp(absVarArray._comp)
{
  //this->_base = new(*this->_region) TR::VPConstraint*[32];
  for (int i = 0; i < 32; i++) {
    TR::VPConstraint *constraint = absVarArray.at(i);
    this->_base[i] = constraint;
  }
}

void
AbsVarArray::at(int pos, TR::VPConstraint* constraint)
{
  if (pos >= 0) this->_base[pos] = constraint;
}

int
AbsVarArray::size() const {
  return 32;
}

TR::VPConstraint*
AbsVarArray::at(int pos) const
{
  return pos >= 0 ? this->_base[pos] : NULL;
}

void
AbsVarArray::merge(AbsVarArray &absVarArray) {
  for (int i = 0; i < this->size(); i++) {
    TR::VPConstraint *hereConstraint = this->at(i);
    if (hereConstraint == NULL) continue;

    TR::VPConstraint *constraint = absVarArray.at(i);
    if (constraint != NULL) {
      hereConstraint->merge(constraint, this->_vp);
      continue;
    }

    this->at(i, NULL);
  }
  return;
}

void
AbsVarArray::initialize(IDTNode* hunk, AbsEnv* context, TR::ValuePropagation *vp) {
  this->_vp = vp;
  const auto resolvedMethodSymbol = hunk->getResolvedMethodSymbol();
  if (!resolvedMethodSymbol) { return; }
  
  TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
  const auto numberOfParameters = resolvedMethod->numberOfParameters();
  const auto insufficientContext = numberOfParameters > context->size();

  if (insufficientContext) {
    TRACE(true, "INSUFFICIENT CONTEXT");
  }

  AbsOpStack inverse(this->_comp, this->_region);
  for (int i = 0; i < numberOfParameters; i++) {
    TR::VPConstraint* constraint = context->topAndPop();
    inverse.pushConstraint(constraint);
  }

  for (int i = 0; i < numberOfParameters; i++) {
    TR::VPConstraint* constraint = inverse.topAndPop();
    this->at(i, constraint);
    TR::VPConstraint* copy = this->at(i);
  }
  
}

void
AbsVarArray::print() {
  for (int i = 0; i < this->size(); i++) {
     TR::VPConstraint *constraint = this->at(i);
     if (!constraint) {
        TRACE(true, "array[%d] = NULL\n", i);
        continue;
     }
     TRACE(true, "array[%d] = ", i);
     constraint->print(this->_comp, this->_comp->getOutFile());
     TRACE(true, "\n");
  }
}

/*
void
AbsVarArray::initialize(IDT::Node* rootHunk, TR::ValuePropagation *vp) {
  this->_vp = vp;
  const auto rootResolvedMethodSymbol = rootHunk->getResolvedMethodSymbol();
  if (!rootResolvedMethodSymbol) { return; }
  if (rootHunk->isVirtual()) 
  this->initialize(rootResolvedMethodSymbol);
  
  const auto numberOfImplicitParameters = rootHunk->isVirtual() ? 1 : 0;
  TR_ResolvedMethod *rootResolvedMethod = rootResolvedMethodSymbol->getResolvedMethod();
  const auto numberOfParameters = rootResolvedMethod->numberOfExplicitParameters();
  TR_MethodParameterIterator *iter = rootResolvedMethod->getParameterIterator(*this->_comp);
  int i;
  for (i = numberOfImplicitParameters; !iter->atEnd(); iter->advanceCursor(), i++) {
    if (iter->isClass()) {
      TR_OpaqueClassBlock *parameterClass = iter->getOpaqueClass();
      const bool paramClassIsNull = !parameterClass;
      if (paramClassIsNull) { 
         this->at(i, NULL); 
         trfprintf(this->_comp->getOutFile(), "Cannot get parameter %d\n", i);
         continue;
      }
      TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, parameterClass);
      TR::VPObjectLocation *objectLocation = TR::VPObjectLocation::create(this->_vp, TR::VPObjectLocation::J9ClassObject);
      TR::VPConstraint *explicitParameterConstraint = TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, objectLocation);
      trfprintf(this->_comp->getOutFile(), "Could get parameter %d\n", i);
      this->at(i, explicitParameterConstraint);
      if (explicitParameterConstraint) explicitParameterConstraint->print(this->_comp, this->_comp->getOutFile());
      else 
      trfprintf(this->_comp->getOutFile(), "False alarm");
      continue;
    }
    trfprintf(this->_comp->getOutFile(), "Cannot get parameter %d\n", i);
    this->at(i, NULL);
  }
  trfprintf(this->_comp->getOutFile(), "Pushed %d constraints to %s\n", i, rootResolvedMethodSymbol->signature(this->_comp->trMemory()));
}
void
AbsVarArray::initialize(TR::ResolvedMethodSymbol *rootResolvedMethodSymbol) {
        TR_ResolvedMethod *rootResolvedMethod = rootResolvedMethodSymbol->getResolvedMethod();
        TR_OpaqueClassBlock *implicitParameterClass = rootResolvedMethod->containingClass();
        if (!implicitParameterClass) {
           trfprintf(this->_comp->getOutFile(), "Cannot get implicit parameter\n");
           this->at(0, NULL);
           return;
        }
    TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, implicitParameterClass);
        TR::VPObjectLocation *objectLocation = TR::VPObjectLocation::create(this->_vp, TR::VPObjectLocation::J9ClassObject);
        TR::VPConstraint *implicitParameterConstraint = TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, objectLocation);
        trfprintf(this->_comp->getOutFile(), "Could get implicit parameter\n");
        if (implicitParameterConstraint) implicitParameterConstraint->print(this->_comp, this->_comp->getOutFile());
        else 
        trfprintf(this->_comp->getOutFile(), "False alarm");
        this->at(0, implicitParameterConstraint);
}
*/
