#include "AbsEnv.hpp"

#include "env/jittypes.h"
#include "infra/J9Cfg.hpp"
#include "infra/Cfg.hpp"                                  // for CFG
#include "infra/TRCfgEdge.hpp"                            // for CFGEdge
#include "infra/TRCfgNode.hpp"                            // for CFGNode
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/OMRBlock.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "il/Symbol.hpp"                                  // for Symbol
#include "compile/SymbolReferenceTable.hpp"
#include "il/SymbolReference.hpp"
#include "env/VMAccessCriticalSection.hpp" // for VMAccessCriticalSection
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"
#include "compiler/optimizer/InlinerPacking.hpp"
#include "infra/List.hpp"

#ifndef TRACE
#define TRACE(COND, M, ...) 
//#define TRACE(COND, M, ...) \
//if ((COND)) { traceMsg(this->_comp, "(%s:%d) " M "", __FILE__, __LINE__, ##__VA_ARGS__); }
#endif

#ifndef TRACEEND
#define TRACEEND(COND, M, ...) 
//#define TRACEEND(COND, M, ...) \
//if ((COND)) { traceMsg(this->_comp, M, ##__VA_ARGS__); }
#endif

typedef TR::typed_allocator<const char*, TR::Region &> strAllocator;

AbsEnv::AbsEnv(TR::Compilation *comp, TR::Region &region) :
    _variableArray(comp, region),
    _operandStack(comp, region),
    _region(region),
    _comp(comp),
    _hunk(NULL)
{
    TR_ASSERT(comp, "comp is null");
    this->_directDependency = -1;
    this->_returns = NULL;
    this->_firstReturn = true;
    this->_vp = NULL;
    TR::OptimizationManager* manager = this->_comp->getOptimizer()->getOptimization(OMR::globalValuePropagation);
    this->_vp = (TR::ValuePropagation*) manager->factory()(manager);
    this->_vp->initialize();
    this->_variableArray._vp = this->_vp;
    this->_newMethodSummary = new (this->_region) MethodSummary(comp, region, this->_vp, _hunk);
    //this->_precomputedMethod = new (*this->_region) std::map<const char*, MethodSummary*, AbsEnv::cmp_str, strAllocator>(AbsEnv::cmp_str(), *this->_region);
}

// for all previously interpreted predecessors
AbsEnv::AbsEnv(AbsEnv& absEnv) : 
    _variableArray(absEnv._comp, absEnv._region),
    _operandStack(absEnv._comp, absEnv._region),
    _region(absEnv._region),
    _comp(absEnv._comp)
{
    this->_directDependency = -1;
    TR_ASSERT(_comp, "comp is null");
    this->_returns = absEnv._returns;
    this->_firstReturn = absEnv._firstReturn;
    this->_vp = absEnv._vp;
    this->_hunk = absEnv._hunk;

        // copy array values...
    for (int i = 0; i < this->_variableArray.size(); i++) {
      this->at(i, absEnv.at(i));
    }

        // copy values in stack
     this->_operandStack.copyStack(absEnv._operandStack);
     this->_variableArray._vp = this->_vp;
     this->_newMethodSummary = absEnv._newMethodSummary;
    //this->_precomputedMethod = absEnv._precomputedMethod;
     
}


AbsEnv::AbsEnv(AbsEnv& absEnv, IDTNode* hunk) :
    _variableArray(absEnv._comp, absEnv._region),
    _operandStack(absEnv._comp, absEnv._region),
    _region(absEnv._region),
    _comp(absEnv._comp)
{
    this->_directDependency = -1;
    TR_ASSERT(_comp, "comp is null");
    this->_returns = NULL;
    this->_firstReturn = true;
    this->_vp = absEnv._vp;
    this->_hunk = hunk;
    // add data from AbsEnv, compare with hunk...
    this->_variableArray._vp = this->_vp;
    this->_newMethodSummary = absEnv._newMethodSummary;

#ifdef TR_PROFITABILITY_VERIFY
    this->_hunk->_methodSummary = this->_newMethodSummary; // printing verbose
#endif
    //this->_precomputedMethod = absEnv._precomputedMethod;
}


// for root
// for root
// for starting again after detecting a cycle
AbsEnv::AbsEnv(TR::Compilation *comp, TR::Region &region, IDTNode* hunk) :
    _variableArray(comp, region),
    _operandStack(comp, region),
    _region(region),
    _comp(comp)
{
    this->_directDependency = -1;
    this->_comp = comp;
    TR_ASSERT(_comp, "comp is null");
    this->_returns = NULL;
    this->_firstReturn = true;
    this->_vp = NULL;
    TR::OptimizationManager* manager = this->_comp->getOptimizer()->getOptimization(OMR::globalValuePropagation);
    this->_vp = (TR::ValuePropagation*) manager->factory()(manager);
    this->_vp->initialize();
    this->_hunk = hunk;

    this->_variableArray._vp = this->_vp;
    const bool isRootContext = hunk->isRoot();
    if (isRootContext) {
          this->addImplicitArgumentConstraint(hunk);
          this->addMethodParameterConstraints(hunk);
    }

    this->_newMethodSummary = new (this->_region) MethodSummary(comp, region, this->_vp, hunk);
#ifdef TR_PROFITABILITY_VERIFY
    this->_hunk->_methodSummary = this->_newMethodSummary; // printing verbose
#endif
}

// for starting new method
// for elements with 1 predecessor
AbsEnv::AbsEnv(TR::Compilation *comp, TR::Region &region, IDTNode* hunk, AbsEnv &absEnv) :
    _variableArray(comp, region),
    _operandStack(comp, region),
    _region(region),
    _comp(comp)
{
    this->_directDependency = -1;
    TR_ASSERT(_comp, "comp is null");
    this->_returns = NULL;
    this->_firstReturn = true;
    this->_vp = NULL;
    TR::OptimizationManager* manager = this->_comp->getOptimizer()->getOptimization(OMR::globalValuePropagation);
    this->_vp = (TR::ValuePropagation*) manager->factory()(manager);
    this->_vp->initialize();
    this->_hunk = hunk;
    this->_variableArray._vp = this->_vp;

    const bool isRootContext = hunk->isRoot();
    if (isRootContext) {
          this->addImplicitArgumentConstraint(hunk);
          this->addMethodParameterConstraints(hunk);
    }

    this->compareInformationAtCallSite(hunk, absEnv);

    this->_newMethodSummary = new (this->_region) MethodSummary(comp, region, this->_vp, hunk);
#ifdef TR_PROFITABILITY_VERIFY
    this->_hunk->_methodSummary = this->_newMethodSummary; // printing verbose
#endif
    //this->_precomputedMethod = absEnv._precomputedMethod;
}


void
AbsEnv::compareInformationAtCallSite(IDTNode *hunk, AbsEnv &absEnv) {
  if (!hunk) { return; }
  TR::ResolvedMethodSymbol *resolvedMethodSymbol = hunk->getResolvedMethodSymbol();
  if (!resolvedMethodSymbol) { return; }
  TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
  if (!resolvedMethod) { return; }
  
  this->compareInformationAtCallSite(resolvedMethod, absEnv);
}

void
AbsEnv::compareInformationAtCallSite(TR_ResolvedMethod *resolvedMethod, AbsEnv &context) {
  TRACEEND(true, "COMPARING INFORMATION AT CALLSITE \n");
  if (!resolvedMethod) { return; }
  auto numberOfParameters = resolvedMethod->numberOfParameters();
  const auto numberOfExplicitParameters = resolvedMethod->numberOfExplicitParameters();
  const auto numberOfImplicitParameters = numberOfParameters - numberOfExplicitParameters;
  const auto hasImplicitParameter = numberOfImplicitParameters > 0;
  TR::VPConstraint *withoutContext[32];
  if (hasImplicitParameter) {
    TR_OpaqueClassBlock *implicitParameterClass = resolvedMethod->containingClass();
    TR::VPConstraint* implicitParameterConstraint = this->getClassConstraint(implicitParameterClass);
    withoutContext[0] = implicitParameterConstraint;
  }
  TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*this->_comp);

  for (int i = numberOfImplicitParameters; !parameterIterator->atEnd(); parameterIterator->advanceCursor(), i++) {

     auto parameter = parameterIterator;
     const bool isClass = parameter->isClass();
     if (!isClass) {
       withoutContext[i] = NULL;
       continue;
     }

     TR_OpaqueClassBlock *parameterClass = parameter->getOpaqueClass();
     if (!parameterClass) {
       withoutContext[i] = NULL;
       continue;
     }

    TR::VPConstraint *explicitParameterConstraint = this->getClassConstraint(parameterClass);
    withoutContext[i] = explicitParameterConstraint;
  }

  // at this point, withoutContext holds the parameter information.
  // we need parameter information from the stack to see if we have a better estimate.
  AbsOpStack inverse(this->_comp, this->_region);
  TR::VPConstraint *buffer[32];
  for (int i = 0; i < numberOfParameters; i++) {
    TR::VPConstraint* constraint = context.topAndPop();
    inverse.pushConstraint(constraint);
    buffer[i] = constraint;
  }

  for (int i = numberOfParameters - 1; i >= 0; i--) {
     context.push(buffer[i]);
  }
  
  for (int i = 0; i < numberOfParameters; i++) {
    TR::VPConstraint* constraintWithContext = inverse.topAndPop();
    TR::VPConstraint* constraintWithoutContext = withoutContext[i];
    TRACEEND(true, "\n");
    TRACEEND(true, "\n");
    TRACE(true, "constraintWithoutContext : ");
    TRACEEND(true, "\n");
    if (constraintWithoutContext) {
      //constraintWithoutContext->print(this->_comp, this->_comp->getOutFile());
      TRACEEND(true, "\n");
    } else {
      TRACEEND(true, "NULL\n");
    }
    TRACE(true, "constraintWithContext : ");
    TRACEEND(true, "\n");
    if (constraintWithContext) {
      //constraintWithContext->print(this->_comp, this->_comp->getOutFile());
      TRACEEND(true, "\n");
    } else {
      TRACEEND(true, "NULL\n");
    }

    if (constraintWithContext) {
      this->at(i, constraintWithContext);
    } else {
      this->at(i, constraintWithoutContext);
    }
  }

}

void
AbsEnv::addMethodParameterConstraints(IDTNode *hunk) {
  if (!hunk) { return; }
  TR::ResolvedMethodSymbol *resolvedMethodSymbol = hunk->getResolvedMethodSymbol();
  if (!resolvedMethodSymbol) { return; }
  TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
  if (!resolvedMethod) { return; }
  
  this->addMethodParameterConstraints(resolvedMethod);
}

void
AbsEnv::addMethodParameterConstraints(TR_ResolvedMethod *resolvedMethod) {
  if (!resolvedMethod) { return; }

  const auto numberOfParameters = resolvedMethod->numberOfParameters();
  const auto numberOfExplicitParameters = resolvedMethod->numberOfExplicitParameters();
  const auto numberOfImplicitParameters = numberOfParameters - numberOfExplicitParameters;
  TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*this->_comp);


  for (int i = numberOfImplicitParameters; !parameterIterator->atEnd(); parameterIterator->advanceCursor(), i++) {

     auto parameter = parameterIterator;
     const bool isClass = parameter->isClass();
     if (!isClass) {
       this->at(i, NULL);
       continue;
     }

     TR_OpaqueClassBlock *parameterClass = parameter->getOpaqueClass();
     if (!parameterClass) {
       this->at(i, NULL);
       continue;
     }

    TR::VPConstraint *explicitParameterConstraint = this->getClassConstraint(parameterClass);
    this->at(i, explicitParameterConstraint);

  }

}

void
AbsEnv::addImplicitArgumentConstraint(IDTNode *hunk) {
  if (!hunk) { return; }

  TR::ResolvedMethodSymbol *resolvedMethodSymbol = hunk->getResolvedMethodSymbol();
  if (!resolvedMethodSymbol) { return; }

  TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
  if (!resolvedMethod) { return; }

  this->addImplicitArgumentConstraint(resolvedMethod);
}

void
AbsEnv::merge(AbsEnv *absEnv) {
  TRACE((this->_comp->trace(OMR::inlining)), "merge array: ");
  this->_variableArray.merge(absEnv->_variableArray);
  TRACE((this->_comp->trace(OMR::inlining)), "merge operand stack: ");
  this->_operandStack.merge(absEnv->_operandStack, this->_vp);
}

void
AbsEnv::addImplicitArgumentConstraint(TR_ResolvedMethod *resolvedMethod) {
  const auto numberOfParameters = resolvedMethod->numberOfParameters();
  const auto numberOfExplicitParameters = resolvedMethod->numberOfExplicitParameters();
  const auto numberOfImplicitParameters = numberOfParameters - numberOfExplicitParameters;
  const auto hasImplicitParameter = numberOfImplicitParameters > 0;
  if (!hasImplicitParameter) { return; }

  TR_OpaqueClassBlock *implicitParameterClass = resolvedMethod->containingClass();
  TR::VPConstraint* implicitParameterConstraint = this->getClassConstraint(implicitParameterClass);
  this->at(0, implicitParameterConstraint);
}

TR::VPConstraint*
AbsEnv::getClassConstraint(TR_OpaqueClassBlock *implicitParameterClass) {
  if (!implicitParameterClass) {
    return NULL;
  }

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, implicitParameterClass);
  //TR::VPObjectLocation *objectLocation = TR::VPObjectLocation::create(this->_vp, TR::VPObjectLocation::J9ClassObject);
  TR::VPConstraint *implicitParameterConstraint = TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, NULL);
  return implicitParameterConstraint;
}

/*
AbsEnv::AbsEnv(const AbsEnv &absEnv) :
    _variableArray(absEnv._variableArray),
    _operandStack(absEnv._operandStack)
{
  this->_vp = absEnv._vp;
  this->_returns = absEnv._returns;
  this->_firstReturn = absEnv._firstReturn;
  this->_comp = absEnv._comp;
  this->_hunk = absEnv._hunk;
  this->_region = absEnv._region;
}
*/

size_t
AbsEnv::size() const {
  return this->_operandStack.size();
}

TR::VPConstraint*
AbsEnv::top() {
  if (this->_operandStack.empty()) return NULL;
  return this->_operandStack.top();
}

TR::VPConstraint*
AbsEnv::topAndPop() {
  TR::VPConstraint* constraint = this->top();
  this->pop();
  return constraint;
}

void
AbsEnv::pop() {
  TR::VPConstraint* constraint = this->_operandStack.top();
  if (this->_comp->trace(OMR::inlining)) {
    TRACE((this->_comp->trace(OMR::inlining)), "pop: ");
    if (constraint) constraint->print(this->_comp, this->_comp->getOutFile());
    TRACEEND((this->_comp->trace(OMR::inlining)), "\n");
  }
  this->_operandStack.pop();
}

void
AbsEnv::astore0() {
  this->astore(0);
}

void
AbsEnv::astore1() {
  this->astore(1);
}

void
AbsEnv::astore2() {
  this->astore(2);
}

void
AbsEnv::astore3() {
  this->astore(3);
}


void
AbsEnv::aload() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::baload() {
  this->aastore();
}

void
AbsEnv::bastore() {
  this->aastore();
}

void
AbsEnv::bipush(int byte) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->_vp, byte);
  this->push(data);
}

void
AbsEnv::astore(int varIndex) {
  TR::VPConstraint* constraint = this->topAndPop();
  this->at(varIndex, constraint);
  //this->at(varIndex, NULL);
}

bool
AbsEnv::switchLoop1(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci) {
            switch(bc) {
              case J9BCmultianewarray: this->multianewarray(bci.next2Bytes(), bci.nextByteSigned(3)); break;
              case J9BCaastore: this->aastore(); break;
              case J9BCaconstnull: this->aconst_null(); break;
              case J9BCaload: this->aload(bci.nextByte()); break;
              case J9BCaload0: this->aload0(); break;
              case J9BCaload1: this->aload1(); break;
              case J9BCaload2: this->aload2(); break;
              case J9BCaload3: this->aload3(); break;
              case J9BCastore: this->astore(bci.nextByte()); break;
              case J9BCastore0: this->astore0(); break;
              case J9BCastore1: this->astore1(); break;
              case J9BCastore2: this->astore2(); break;
              case J9BCastore3: this->astore3(); break;
              case J9BCbipush: this->bipush(bci.nextByte()); break;
              case J9BCbastore: this->bastore(); break;
              case J9BCbaload: this->baload(); break;
              case J9BCcastore: case J9BCfastore: case J9BCdastore: case J9BCiastore: case J9BClastore: case J9BCsastore: this->aastore(); break;
              case J9BCcaload: case J9BCfaload: case J9BCdaload: case J9BCiaload: case J9BClaload: case J9BCsaload: this->aload(); break;
              case J9BCcheckcast: this->checkcast(bci.next2Bytes(), bci.currentByteCodeIndex()); break;
              case J9BCdup: this->dup(); break;
              case J9BCdupx1: this->dup_x1(); break;
              case J9BCdupx2: this->dup_x2(); break;
              case J9BCgetstatic: this->getstatic(bci.next2Bytes()); break;
              case J9BCgetfield: this->getfield(bci.next2Bytes()); break;
              case J9BCiand: this->iand(); break;
              case J9BCinstanceof: this->instanceof(bci.next2Bytes(), bci.currentByteCodeIndex()); break;
              case J9BCior: this->ior(); break;
              case J9BCixor: this->ixor(); break;
              case J9BCirem: this->irem(); break;
              case J9BCishl: this->ishl(); break;
              case J9BCishr: this->ishr(); break;
              case J9BCiushr: this->iushr(); break;
              case J9BCidiv: this->idiv(); break;
              case J9BCimul: this->imul(); break;
              case J9BCiconst0: this->iconst0(); break;
              case J9BCiconst1: this->iconst1(); break;
              case J9BCiconst2: this->iconst2(); break;
              case J9BCiconst3: this->iconst3(); break;
              case J9BCiconst4: this->iconst4(); break;
              case J9BCiconst5: this->iconst5(); break;
              case J9BCgenericReturn: this->ireturn(); return true; break;
              case J9BCifeq: this->ifeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifne: this->ifne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCiflt: this->iflt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifle: this->ifle(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifgt: this->ifgt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifge: this->ifge(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifnull: this->ifnull(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifnonnull: this->ifnonnull(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmpge: this->ificmpge(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmpeq: this->ificmpeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmpne: this->ificmpne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmplt: this->ificmplt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmpgt: this->ificmpgt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmple: this->ificmple(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifacmpeq: this->ificmpeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifacmpne: this->ificmpne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCgoto: this->_goto(bci.next2Bytes()); return true; break;
              case J9BCiload: this->iload(bci.nextByte()); break;
              case J9BCiload1: this->iload1(); break;
              case J9BCiload2: this->iload2(); break;
              case J9BCiload3: this->iload3(); break;
              case J9BCineg: this->ineg(); break;
              case J9BCdadd: this->dadd(); break;
              case J9BCistore0: this->istore0(); break;
              case J9BCistore1: this->istore1(); break;
              case J9BCistore2: this->istore2(); break;
              case J9BCistore3: this->istore3(); break;
              case J9BCistore: this->istore(bci.nextByte()); break;
              case J9BCisub: this->isub(); break;
              case J9BCiadd: this->iadd(); break;
              case J9BCladd: this->ladd(); break;
              case J9BClsub: this->lsub(); break;
              case J9BCdsub: this->dsub(); break;
              case J9BCfsub: this->dsub(); break;
              case J9BCi2d: case J9BCi2f: this->i2f(); break;
              case J9BCi2l: this->i2l(); break;
              case J9BCi2s: this->i2s(); break;
              case J9BCfadd: this->dsub(); break;
              case J9BCinvokevirtual: this->invokevirtual(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
              case J9BCinvokespecial: this->invokespecial(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
              case J9BCinvokestatic: this->invokestatic(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
              case J9BCinvokeinterface: this->invokeinterface(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
              case J9BCldc: this->ldc(bci.nextByte()); break; 
              case J9BCl2i: this->l2i(); break;
              case J9BCland: this->land(); break;
              case J9BCldiv: this->ldiv(); break;
              case J9BClmul: this->lmul(); break;
              case J9BClneg: this->lneg(); break;
              case J9BClor: this->lor(); break;
              case J9BClrem: this->lrem(); break;
              case J9BClshl: this->lshl(); break;
              case J9BClshr: this->lshr(); break;
              case J9BClushr: this->lushr(); break;
              case J9BClxor: this->lxor(); break;
              case J9BClcmp: this->lcmp(); break;
              case J9BCl2d: this->l2d(); break;
              case J9BCd2f: this->d2f(); break;
              case J9BCf2d: this->f2d(); break;
              case J9BCf2i: this->f2i(); break;
              case J9BCf2l: this->f2l(); break;
              case J9BCd2i: this->d2i(); break;
              case J9BCd2l: this->d2l(); break;
              case J9BCl2f: this->l2f(); break;
              case J9BClload: this->lload(bci.nextByte()); break;
              case J9BClload0: this->lload0(); break;
              case J9BClload1: this->lload1(); break;
              case J9BClload2: this->lload2(); break;
              case J9BClload3: this->lload3(); break;
              case J9BCdcmpg: case J9BCfcmpg: this->dcmpg(); break;
              case J9BCdcmpl: case J9BCfcmpl: this->dcmpl(); break;
              case J9BCdconst0: case J9BCfconst0: this->dconst0(); break;
              case J9BCdconst1: case J9BCfconst1: case J9BCfconst2: this->dconst1(); break;
              case J9BCddiv: case J9BCfdiv: this->ddiv(); break;
              case J9BCdload: case J9BCfload: this->dload(bci.nextByte()); break;
              case J9BCdload0: case J9BCdload1: case J9BCdload2: case J9BCdload3: this->dload0(); break;
              case J9BCfload0: case J9BCfload1: case J9BCfload2: case J9BCfload3: this->dload0(); break;
              case J9BCdmul: case J9BCfmul: this->dmul(); break;
              case J9BCdneg: case J9BCfneg: this->dneg(); break;
              case J9BCi2b: this->i2b(); break;
              case J9BCi2c: this->i2c(); break;
              case J9BCdrem: case J9BCfrem: this->drem(); break;
              case J9BCdstore: this->dstore(bci.nextByte()); break;
              case J9BCfstore: this->dstore(bci.nextByte()); break;
              case J9BCdstore0: this->dstore0(); break;
              case J9BCdstore1: this->dstore1(); break;
              case J9BCdstore2: this->dstore2(); break;
              case J9BCdstore3: this->dstore3(); break;
              case J9BCfstore0: this->dstore0(); break;
              case J9BCfstore1: this->dstore1(); break;
              case J9BCfstore2: this->dstore2(); break;
              case J9BCfstore3: this->dstore3(); break;
              case J9BClconst0: this->lconst0(); break;
              case J9BClconst1: this->lconst1(); break;
              case J9BClstore: this->lstore(bci.nextByte()); break;
              case J9BClstore0: this->lstore0(); break;
              case J9BClstore1: this->lstore1(); break;
              case J9BClstore2: this->lstore2(); break;
              case J9BClstore3: this->lstore3(); break;
              case J9BCpop: this->pop(); break;
              case J9BCpop2: this->pop2(); break;
              case J9BCiinc: this->iinc(bci.nextByte(), bci.nextByteSigned(2)); break;
              case J9BCnew: this->_new(bci.next2Bytes()); break;
              case J9BCswap: this->swap(); break;
              case J9BCsipush: this->sipush(bci.next2Bytes()); break;
              
              default:
              break;
            }
            return false;

}

bool
AbsEnv::switchLoop(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci) {
            switch(bc) {
              case J9BCmultianewarray: this->multianewarray(bci.next2Bytes(), bci.nextByteSigned(3)); break;
              case J9BCaastore: this->aastore(); break;
              case J9BCaconstnull: this->aconst_null(); break;
              case J9BCaload: this->aload(bci.nextByte()); break;
              case J9BCaload0: this->aload0(); break;
              case J9BCaload1: this->aload1(); break;
              case J9BCaload2: this->aload2(); break;
              case J9BCaload3: this->aload3(); break;
              case J9BCastore: this->astore(bci.nextByte()); break;
              case J9BCastore0: this->astore0(); break;
              case J9BCastore1: this->astore1(); break;
              case J9BCastore2: this->astore2(); break;
              case J9BCastore3: this->astore3(); break;
              case J9BCbipush: this->bipush(bci.nextByte()); break;
              case J9BCbastore: this->bastore(); break;
              case J9BCbaload: this->baload(); break;
              case J9BCcastore: case J9BCfastore: case J9BCdastore: case J9BCiastore: case J9BClastore: case J9BCsastore: this->aastore(); break;
              case J9BCcaload: case J9BCfaload: case J9BCdaload: case J9BCiaload: case J9BClaload: case J9BCsaload: this->aload(); break;
              case J9BCcheckcast: this->checkcast(bci.next2Bytes(), bci.currentByteCodeIndex()); break;
              case J9BCdup: this->dup(); break;
              case J9BCdupx1: this->dup_x1(); break;
              case J9BCdupx2: this->dup_x2(); break;
              case J9BCgetstatic: this->getstatic(bci.next2Bytes()); break;
              case J9BCgetfield: this->getfield(bci.next2Bytes()); break;
              case J9BCiand: this->iand(); break;
              case J9BCior: this->ior(); break;
              case J9BCixor: this->ixor(); break;
              case J9BCirem: this->irem(); break;
              case J9BCishl: this->ishl(); break;
              case J9BCishr: this->ishr(); break;
              case J9BCiushr: this->iushr(); break;
              case J9BCidiv: this->idiv(); break;
              case J9BCimul: this->imul(); break;
              case J9BCiconst0: this->iconst0(); break;
              case J9BCiconst1: this->iconst1(); break;
              case J9BCiconst2: this->iconst2(); break;
              case J9BCiconst3: this->iconst3(); break;
              case J9BCiconst4: this->iconst4(); break;
              case J9BCiconst5: this->iconst5(); break;
              case J9BCgenericReturn: this->ireturn(); return true; break;
              case J9BCifeq: this->ifeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifne: this->ifne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCiflt: this->iflt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCifgt: this->ifgt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmpge: this->ificmpge(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmpeq: this->ificmpeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmpne: this->ificmpne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmplt: this->ificmplt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmpgt: this->ificmpgt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCificmple: this->ificmple(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); return true; break;
              case J9BCgoto: this->_goto(bci.next2Bytes()); return true; break;
              case J9BCiload: this->iload(bci.nextByte()); break;
              case J9BCiload1: this->iload1(); break;
              case J9BCiload2: this->iload2(); break;
              case J9BCiload3: this->iload3(); break;
              case J9BCineg: this->ineg(); break;
              case J9BCdadd: this->dadd(); break;
              case J9BCistore0: this->istore0(); break;
              case J9BCistore1: this->istore1(); break;
              case J9BCistore2: this->istore2(); break;
              case J9BCistore3: this->istore3(); break;
              case J9BCistore: this->istore(bci.nextByte()); break;
              case J9BCisub: this->isub(); break;
              case J9BCiadd: this->iadd(); break;
              case J9BCladd: this->ladd(); break;
              case J9BClsub: this->lsub(); break;
              case J9BCdsub: this->dsub(); break;
              case J9BCfsub: this->dsub(); break;
              case J9BCi2d: case J9BCi2f: this->i2f(); break;
              case J9BCi2l: this->i2l(); break;
              case J9BCi2s: this->i2s(); break;
              case J9BCfadd: this->dsub(); break;
              case J9BCinvokevirtual: this->invokevirtual(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
              case J9BCinvokespecial: this->invokespecial(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
              case J9BCinvokestatic: this->invokestatic(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
              case J9BCinvokeinterface: this->invokeinterface(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
              case J9BCldc: this->ldc(bci.nextByte()); break; 
              case J9BCl2i: this->l2i(); break;
              case J9BCland: this->land(); break;
              case J9BCldiv: this->ldiv(); break;
              case J9BClmul: this->lmul(); break;
              case J9BClneg: this->lneg(); break;
              case J9BClor: this->lor(); break;
              case J9BClrem: this->lrem(); break;
              case J9BClshl: this->lshl(); break;
              case J9BClshr: this->lshr(); break;
              case J9BClushr: this->lushr(); break;
              case J9BClxor: this->lxor(); break;
              case J9BClcmp: this->lcmp(); break;
              case J9BCl2d: this->l2d(); break;
              case J9BCd2f: this->d2f(); break;
              case J9BCf2d: this->f2d(); break;
              case J9BCf2i: this->f2i(); break;
              case J9BCf2l: this->f2l(); break;
              case J9BCd2i: this->d2i(); break;
              case J9BCd2l: this->d2l(); break;
              case J9BCl2f: this->l2f(); break;
              case J9BClload: this->lload(bci.nextByte()); break;
              case J9BClload0: this->lload0(); break;
              case J9BClload1: this->lload1(); break;
              case J9BClload2: this->lload2(); break;
              case J9BClload3: this->lload3(); break;
              case J9BCdcmpg: case J9BCfcmpg: this->dcmpg(); break;
              case J9BCdcmpl: case J9BCfcmpl: this->dcmpl(); break;
              case J9BCdconst0: case J9BCfconst0: this->dconst0(); break;
              case J9BCdconst1: case J9BCfconst1: case J9BCfconst2: this->dconst1(); break;
              case J9BCddiv: case J9BCfdiv: this->ddiv(); break;
              case J9BCdload: case J9BCfload: this->dload(bci.nextByte()); break;
              case J9BCdload0: case J9BCdload1: case J9BCdload2: case J9BCdload3: this->dload0(); break;
              case J9BCfload0: case J9BCfload1: case J9BCfload2: case J9BCfload3: this->dload0(); break;
              case J9BCdmul: case J9BCfmul: this->dmul(); break;
              case J9BCdneg: case J9BCfneg: this->dneg(); break;
              case J9BCi2b: this->i2b(); break;
              case J9BCi2c: this->i2c(); break;
              case J9BCdrem: case J9BCfrem: this->drem(); break;
              case J9BCdstore: this->dstore(bci.nextByte()); break;
              case J9BCfstore: this->dstore(bci.nextByte()); break;
              case J9BCdstore0: this->dstore0(); break;
              case J9BCdstore1: this->dstore1(); break;
              case J9BCdstore2: this->dstore2(); break;
              case J9BCdstore3: this->dstore3(); break;
              case J9BCfstore0: this->dstore0(); break;
              case J9BCfstore1: this->dstore1(); break;
              case J9BCfstore2: this->dstore2(); break;
              case J9BCfstore3: this->dstore3(); break;
              case J9BClconst0: this->lconst0(); break;
              case J9BClconst1: this->lconst1(); break;
              case J9BClstore: this->lstore(bci.nextByte()); break;
              case J9BClstore0: this->lstore0(); break;
              case J9BClstore1: this->lstore1(); break;
              case J9BClstore2: this->lstore2(); break;
              case J9BClstore3: this->lstore3(); break;
              case J9BCpop: this->pop(); break;
              case J9BCpop2: this->pop2(); break;
              case J9BCiinc: this->iinc(bci.nextByte(), bci.nextByteSigned(2)); break;
              case J9BCnew: this->_new(bci.next2Bytes()); break;
              case J9BCswap: this->swap(); break;
              case J9BCsipush: this->sipush(bci.next2Bytes()); break;
              
              default:
              break;
            }
            return false;

}

void
AbsEnv::sipush(int _short) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->_vp, _short);
  this->push(data);
}

void
AbsEnv::swap() {
  TR::VPConstraint *value1 = this->topAndPop();
  TR::VPConstraint *value2 = this->topAndPop();
  this->push(value1);
  this->push(value2);
}

void
AbsEnv::pop2() {
  TR::VPConstraint *value1 = this->topAndPop();
  bool double1 = !value1;
  bool long1 = value1 && value1->asLongConstraint();
  bool form2 = double1 || long1;
  if (form2) {
    return;
  }

  this->pop();
}

void
AbsEnv::aastore() {
  //TODO: I don't really know how to implement it properly 
  //beause I have not dealt with arrays yet.
  TR::VPConstraint *value = this->topAndPop();
  TR::VPConstraint *index = this->topAndPop();
  TR::VPConstraint *arrayRef = this->topAndPop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
}

void
AbsEnv::aaload() {
  TR::VPConstraint *index = this->topAndPop();
  TR::VPConstraint *arrayRef = this->topAndPop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  // if we know arrayRef, we may know the type...
  this->pushNull();
}

void
AbsEnv::instanceof(int cpIndex, int byteCodeIndex) {
  TR::VPConstraint *objectref = this->topAndPop();
  this->pushNull();

  auto dependsOnArgument = this->_directDependency > -1;
  if (!dependsOnArgument) return;

  TRACEEND((this->_comp->trace(OMR::inlining)), "instanceof: depends on argument\n"); 
  TR_ResolvedMethod* method = this->_hunk->getResolvedMethodSymbol()->getResolvedMethod();
  TR_OpaqueClassBlock *block = method->getClassFromConstantPool(this->_comp, cpIndex);
  if (!block) return;
  TRACEEND((this->_comp->trace(OMR::inlining)), "instanceof: block is non null\n"); 

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, block);
  TR::VPObjectLocation *objectLocation = TR::VPObjectLocation::create(this->_vp, TR::VPObjectLocation::J9ClassObject);
  TR::VPConstraint *typeConstraint= TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, objectLocation);

  int argumentToConstraint = this->_directDependency;
  MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newINOF(_hunk, byteCodeIndex);
  if (!constraintNewAPath) return;
  constraintNewAPath->at(argumentToConstraint, typeConstraint);
  
}

void
AbsEnv::checkcast(int cpIndex, int bytecodeIndex) {
  TR::VPConstraint *objectRef = this->topAndPop();

  TR_ResolvedMethod* method = this->_hunk->getResolvedMethodSymbol()->getResolvedMethod();
  TR_OpaqueClassBlock* type = method->getClassFromConstantPool(_comp, cpIndex);
  TR::VPConstraint *typeConstraint = NULL;
  if (type) {
    TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, type);
    TR::VPObjectLocation *objectLocation = NULL;
    TR::VPNonNullObject *nonNull = TR::VPNonNullObject::create(this->_vp);
    typeConstraint= TR::VPClass::create(this->_vp, fixedClass, nonNull, NULL, NULL, objectLocation);
  }

  bool constraintsAnArgument = this->_directDependency >= 0;
  if (!constraintsAnArgument) { this->push(typeConstraint); return; }

  if (type) {
     MethodSummaryRow* constraintNewAPath = this->_newMethodSummary->newCHECK_CAST(_hunk, bytecodeIndex);
     if (constraintNewAPath) constraintNewAPath->at(this->_directDependency, typeConstraint);
     
  }
  this->push(typeConstraint);

}

void
AbsEnv::dcmpg() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::_new(int cpIndex) {
  TR::ResolvedMethodSymbol *rms = this->getMethodSymbol();
  TR::SymbolReference *symRef = this->_comp->getSymRefTab()->findOrCreateClassSymbol(rms, cpIndex, NULL);
  int32_t length;
  const char *signature = symRef->getTypeSignature(length); 
  TR_OpaqueClassBlock *type = signature ? this->_comp->fe()->getClassFromSignature(signature, length, rms->getResolvedMethod(), true) : 0;
  if (type) {
    TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, type);
    TR::VPObjectLocation *objectLocation = NULL;
    TR::VPNonNullObject *nonNull = TR::VPNonNullObject::create(this->_vp);
    TR::VPConstraint *typeConstraint= TR::VPClass::create(this->_vp, fixedClass, nonNull, NULL, NULL, objectLocation);
    this->push(typeConstraint);
    return;
  }

    TR::VPNonNullObject *nonNull = TR::VPNonNullObject::create(this->_vp);
    TR::VPConstraint *typeConstraint= TR::VPClass::create(this->_vp, NULL, nonNull, NULL, NULL, NULL);
    this->push(typeConstraint);
}

void
AbsEnv::arraylength() {
  TR::VPConstraint *arrayRef = this->topAndPop();
  if (!arrayRef) {
    this->pushNull();
    return;
  }

  TR::VPArrayInfo *arrayInfo = arrayRef->asArrayInfo();
  if (!arrayInfo) {
    this->pushNull();
    return;
  }

  int low = arrayInfo->lowBound();
  int high = arrayInfo->highBound();
  TR::VPIntConstraint *intRange = TR::VPIntRange::create(this->_vp, low, high);
  this->push(intRange);
}

void
AbsEnv::dup2_x1() {
  TR::VPConstraint *value1 = this->topAndPop();
  TR::VPConstraint *value2 = this->topAndPop();
  bool double1 = !value1;
  bool double2 = !value2;
  bool assumeBothDouble = double1 && double2;

  TR::VPLongConstraint *long1 = value1->asLongConstraint();
  TR::VPLongConstraint *long2 = value2->asLongConstraint();
  bool bothLong = long1 && long2;

  bool assumeDoubleAndLong = double1 && long2;
  bool assumeLongAndDouble = long1 && double2;

  bool assumeType2 = assumeBothDouble || bothLong || assumeDoubleAndLong || assumeLongAndDouble;

  if (assumeType2) {
     this->push(value1);
     this->push(value2);
     this->push(value1);
     return;
  }


  TR::VPConstraint *value3 = this->topAndPop();
  this->push(value2);
  this->push(value1);
  this->push(value3);
  this->push(value2);
  this->push(value1);
}

void
AbsEnv::dup2_x2() {
  TR::VPConstraint *value1 = this->topAndPop();
  TR::VPConstraint *value2 = this->topAndPop();
  bool double1 = !value1;
  bool double2 = !value2;
  bool assumeBothDouble = double1 && double2;

  bool long1 = value1 && value1->asLongConstraint();
  bool long2 = value2 && value2->asLongConstraint();
  bool bothLong = long1 && long2;

  bool assumeDoubleAndLong = double1 && long2;
  bool assumeLongAndDouble = long1 && double2;
  bool oneComputationalType1 = !double1 && !long1;
  bool twoComputationalType1 = !double2 && !long2;
  bool oneAndTwoComputationalType1 = oneComputationalType1 && twoComputationalType1;

  bool form4 = !oneComputationalType1 && !twoComputationalType1;
  
  if (form4) {
     this->push(value1);
     this->push(value2);
     this->push(value1);
     return;
  }

  TR::VPConstraint *value3 = this->topAndPop();
  bool double3 = !value3;
  bool long3 = value3 && value3->asLongConstraint();
  bool threeComputationalType1 = !double3 && !long3;

  bool form3 = !threeComputationalType1 && oneAndTwoComputationalType1;
  if (form3) {
     this->push(value2);
     this->push(value1);
     this->push(value3);
     this->push(value2);
     this->push(value1);
     return;
  }

  bool twoAndThreeComputationalType1 = twoComputationalType1 && threeComputationalType1;
  bool form2 = !oneComputationalType1 &&  twoAndThreeComputationalType1;
  if (form2) {
    this->push(value1);
    this->push(value3);
    this->push(value2);
    this->push(value1);
    return;
  }

  TR::VPConstraint *value4 = this->topAndPop();
  this->push(value2);
  this->push(value1);
  this->push(value4);
  this->push(value3);
  this->push(value2);
  this->push(value1);
}

void
AbsEnv::dup2() {
  TR::VPConstraint *value1 = this->topAndPop();
  if (!value1) {
     // assume is category 2 computational type
     this->push(value1);
     this->push(value1);
     return;
  }

  TR::VPLongConstraint *isLong = value1->asLongConstraint();
  // we don't have double constraint so we can only check if it is long...
  if (isLong) {
     this->push(value1);
     this->push(value1);
     return;
  }

  TR::VPConstraint *value2 = this->topAndPop();
  this->push(value1);
  this->push(value2);
  this->push(value1);
  this->push(value2);
  
}

void
AbsEnv::multianewarray(int cpIndex, int dimensions) {
  for (int i = 0; i < dimensions; i++) {
    this->pop();
  }
  this->pushNull();
}

void
AbsEnv::anewarray(int cpIndex) {
  TR::VPConstraint *count = this->topAndPop();
  if (!count) {
     this->pushNull();
     return;
  }

  TR::VPIntConstraint *countInt = count->asIntConstraint();
  if (!countInt) {
     this->pushNull();
     return;
  }

  int elementSize = 4;
  //TODO: elementSize should not always be 4.
 
  TR::VPArrayInfo *arrayRef = TR::VPArrayInfo::create(this->_vp, countInt->getLow(), countInt->getHigh(), elementSize);
  TR::ResolvedMethodSymbol *rms = this->getMethodSymbol();
  TR::SymbolReference *symRef = this->_comp->getSymRefTab()->findOrCreateClassSymbol(rms, cpIndex, NULL);
  int32_t length;
  const char *signature = symRef->getTypeSignature(length); 
  TR_OpaqueClassBlock *clazz = signature ? this->_comp->fe()->getClassFromSignature(signature, length, rms->getResolvedMethod(), true) : 0;
  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, clazz);
  TR::VPConstraint *typeConstraint= TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, arrayRef, NULL);
  this->push(typeConstraint);
}

void
AbsEnv::getfield(int cpIndex) {
  this->pop();
  this->pushNull();
}


void
AbsEnv::putfield(int cpIndex) {
  this->pop();
  this->pop();
}

void
AbsEnv::putstatic(int cpIndex) {
  this->pop();
}

void
AbsEnv::newarray(int atype) {
  TR::VPConstraint *count = this->topAndPop();
  if (!count) {
     this->pushNull();
     return;
  }

  TR::VPIntConstraint *countInt = count->asIntConstraint();
  if (!countInt) {
     this->pushNull();
     return;
  }

  int elementSize = 4;
  //TODO: which types are in the array
  TR::VPConstraint *arrayRef = TR::VPArrayInfo::create(this->_vp, countInt->getLow(), countInt->getHigh(), elementSize);
  this->push(arrayRef);
}

void
AbsEnv::dcmpl() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::lcmp() {
  TR::VPConstraint *value2 = this->topAndPop();
  TR::VPConstraint *value1 = this->topAndPop();
  bool canCompare = value2 && value1;
  if (!canCompare) {
    this->pushNull();
    return;
  }

  bool isVal1GtVal2 = value2->mustBeLessThan(value1, this->_vp);
  bool isVal1EqVal2 = !value2->mustBeNotEqual(value1, this->_vp);
  if (isVal1EqVal2) {
     TR::VPIntConst *zero= TR::VPIntConst::create(this->_vp, 0);
     this->push(zero);
     return;
  }

  if (isVal1GtVal2) {
     TR::VPIntConst *one= TR::VPIntConst::create(this->_vp, 1);
     this->push(one);
     return;
  }

  TR::VPIntConst *mone= TR::VPIntConst::create(this->_vp, -1);
  this->push(mone);
  return;

}

void
AbsEnv::aconst_null() {
  TR::VPConstraint *null = TR::VPNullObject::create(this->_vp);
  this->push(null);
}

void
AbsEnv::_goto(int branchoffset) {
  // This is empty because the abstract interpreter models
  // the changes in the stack and all jumping is
  // handled by the CFG.
}

void
AbsEnv::iinc(int index, int incval) {
  TR::VPConstraint *value1 = this->at(index);
  if (!value1) {
    TRACE(true, "what is the value of index? %d", index);
    this->at(index, NULL);
    return;
  }
  TR::VPIntConstraint *value = value1->asIntConstraint();
  if (!value) {
    TRACE(true, "what is the value of index? %d", index);
    this->at(index, NULL);
    return;
  }

  
  TRACE(true, "what is the value of index? %d", index);
  TR::VPIntConst *inc = TR::VPIntConst::create(this->_vp, incval);
  value->add(inc, TR::Int32, this->_vp);
  this->at(index, value);
}

void
AbsEnv::dup_x2() {
  TR::VPConstraint *value1 = this->topAndPop();
  TR::VPConstraint *value2 = this->topAndPop();
  bool double1 = !value1;
  bool long1 = value1 && value1->asLongConstraint();
  bool oneIsType2 = double1 || long1;
  bool double2 = !value2;
  bool long2 = value2 && value2->asLongConstraint();
  bool twoIsType2 = double2 || long2;

  bool form1 = !oneIsType2 && twoIsType2;
  if (form1) {
    this->push(value1);
    this->push(value2);
    this->push(value1);
    return;
  }

  TR::VPConstraint *value3 = this->topAndPop();
  this->push(value1);
  this->push(value3);
  this->push(value2);
  this->push(value1);
}

void
AbsEnv::dup_x1() {
  TR::VPConstraint *value = this->topAndPop();
  TR::VPConstraint *temp = this->topAndPop();
  this->push(value);
  this->push(temp);
  this->push(value);
}

void
AbsEnv::dup() {
  TR::VPConstraint *value = this->topAndPop();
  this->push(value);
  this->push(value); 
}

void
AbsEnv::ior() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::irem() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::ishl() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::iushr() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::ixor() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::ishr() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::iand() {
  TR::VPIntConstraint* constraint1 = (TR::VPIntConstraint*)this->topAndPop();
  TR::VPIntConstraint* constraint2 = (TR::VPIntConstraint*)this->topAndPop();
  bool unknown = (!constraint1) || (!constraint2);
  if (unknown) {
    this->pushNull();
    return;
  }

  this->pushNull();
}

void
AbsEnv::idiv() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::imul() {
  this->pop();
  this->pop();
  this->pushNull();
}

int
AbsEnv::interpret(int offset, int until) {
    TR::ResolvedMethodSymbol *resolvedMethodSymbol = this->_hunk->getResolvedMethodSymbol();
     if (!resolvedMethodSymbol) { 
          TRACE((this->_comp->trace(OMR::inlining)), "interpret: resolvedMethodSymbol is nnull");
          return -1;
        }
          TR_ResolvedMethod *method = resolvedMethodSymbol->getResolvedMethod();
          TR_J9ByteCodeIterator bci(resolvedMethodSymbol, static_cast<TR_ResolvedJ9Method*>(method), static_cast<TR_J9VMBase*>(this->_comp->fe()), this->_comp);
          TR_J9ByteCode bc = bci.first();
          while (bci.currentByteCodeIndex() < offset) {
             bc = bci.next();
          }
          
          bool ret = false;
          for (; bc != J9BCunknown || bci.currentByteCodeIndex() < until; bc = bci.next()) {
            ret = bc == J9BCgenericReturn;
            //bci.printByteCode(); 
            bool end = switchLoop(bc, bci);
            if (end) break;
          }
          this->_endsWithReturn = ret;
          return bci.currentByteCodeIndex();
}


bool
AbsEnv::wereAllPredecessorsVisited(OMR::Block *block, IndexMap *map) {
        TR::CFGEdgeList &predecessors = block->getPredecessors();
        for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i) {
          TR::CFGEdge *edge = *i;
          TR::CFGNode *predecessor = edge->getFrom();
          OMR::Block *block = predecessor->asBlock();
          const int basicBlockIndex = block->getBlockBCIndex();
          const bool visitedBefore = map->find(basicBlockIndex) != map->end();
          if (!visitedBefore) return false;
        }
        return true;
}

bool
AbsEnv::interpret(int offset, OMR::Block *block, IndexMap *map) {

        // detect whether this is a bad offset or if we had visited this basic block already...
        if (offset < 0) {
           return false;
        }

    TR::ResolvedMethodSymbol *resolvedMethodSymbol = this->_hunk->getResolvedMethodSymbol();
     if (!resolvedMethodSymbol) { 
          TRACE((this->_comp->trace(OMR::inlining)), "interpret: resolvedMethodSymbol is nnull");
          return false;
        }


        TR_ResolvedMethod *method = resolvedMethodSymbol->getResolvedMethod();
        TR_J9ByteCodeIterator bci(resolvedMethodSymbol, static_cast<TR_ResolvedJ9Method*>(method), static_cast<TR_J9VMBase*>(this->_comp->fe()), this->_comp);
        TR_J9ByteCode bc = bci.first();
        while (bci.currentByteCodeIndex() < offset) {
             bc = bci.next();
             if (bc == J9BCunknown) break;
        }
        for (; bc != J9BCunknown ; bc = bci.next()) {
            //bci.printByteCode(); 
            bool end = switchLoop(bc, bci);
            if (end) break;
        }

        return true;
}

void
AbsEnv::interpret2(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci, int end) {
  for (int i = bci.currentByteCodeIndex(); i <  end and bc != J9BCunknown; bc = bci.next(), i = bci.currentByteCodeIndex())
  {
     //bci.printByteCode(); 
     switchLoop1(bc, bci);
     this->print();
  }
}


bool
AbsEnv::interpret1(int offset, OMR::Block *block, IndexMap *map) {
        // detect whether this is a bad offset or if we had visited this basic block already...
        if (offset < 0) {
           return false;
        }

        //block->getGlobalNormalizedFrequency();
    TR::ResolvedMethodSymbol *resolvedMethodSymbol = this->_hunk->getResolvedMethodSymbol();
     if (!resolvedMethodSymbol) { 
          TRACE((this->_comp->trace(OMR::inlining)), "interpret: resolvedMethodSymbol is nnull");
          return false;
        }


        TR_ResolvedMethod *method = resolvedMethodSymbol->getResolvedMethod();
        TR_J9ByteCodeIterator bci(resolvedMethodSymbol, static_cast<TR_ResolvedJ9Method*>(method), static_cast<TR_J9VMBase*>(this->_comp->fe()), this->_comp);
        TR_J9ByteCode bc = bci.first();
        while (bci.currentByteCodeIndex() < offset) {
             bc = bci.next();
             if (bc == J9BCunknown) break;
        }

        bool resetDirectDependency = false;
        for (; bc != J9BCunknown ; bc = bci.next()) {
            //bci.printByteCode(); 
            if (this->_directDependency != -1) { 
               resetDirectDependency = true;
            }
            bool end = switchLoop1(bc, bci);
            if (end) break;
            if (resetDirectDependency) {
              this->_directDependency = -1;
              resetDirectDependency = false;
            }
        }

        return true;

}

void
AbsEnv::interpret1(OMR::Block *block, IndexMap *map) {
  const int32_t basicBlockIndex = block->getBlockBCIndex();
  const bool negBasicBlock = basicBlockIndex < 0;
  if (negBasicBlock) { return; }

  this->_block = block;
  this->interpret1(basicBlockIndex, block, map);

}

void
AbsEnv::interpret(OMR::Block *block, IndexMap *map) {

  const int32_t basicBlockIndex = block->getBlockBCIndex();
  const bool negBasicBlock = basicBlockIndex < 0;
  if (negBasicBlock) { return; }

  this->_block = block;
  this->interpret(basicBlockIndex, block, map);
}

void
AbsEnv::interpret(bool skip) {
        //trfprintf(this->_comp->getOutFile(), "\nHERE: interpreting bool %s \n", this->_hunk->name());
    TR::ResolvedMethodSymbol *resolvedMethodSymbol = this->_hunk->getResolvedMethodSymbol();
     if (!resolvedMethodSymbol) { 
          TRACE((this->_comp->trace(OMR::inlining)), "interpret: resolvedMethodSymbol is null");
          return; 
        }
}


void
AbsEnv::print() {
  TR_ASSERT(_comp, "comp is null");
  this->_operandStack.print();
  this->_variableArray.print();
}

bool
AbsEnv::endsWithReturn() {
  return this->_endsWithReturn;
}

void
AbsEnv::copyStack(AbsEnv &absEnv) {
  this->_operandStack.copyStack(absEnv._operandStack);
}

void
AbsEnv::sub(TR::DataType datatype) {

  TR::VPConstraint *left = this->topAndPop();
  TR::VPConstraint *right = this->topAndPop();
  const bool leftExists = left;
  const bool rightExists = right;
  const bool canAdd = leftExists && rightExists;
  if (!canAdd) {
    this->pushNull();
    return;
  }

  TR::VPConstraint* result = left->subtract(right, datatype, this->_vp);
  //if (result) result->print(this->_comp, this->_comp->getOutFile());
  this->push(result);
  return;
}

void
AbsEnv::dadd() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::daload() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::dastore() {
  this->pop();
  this->pop();
  this->pop();
}

void
AbsEnv::iadd() {
  this->add(TR::Int32);
}

void
AbsEnv::ladd() {
  this->add(TR::Int64);
}

void
AbsEnv::lsub() {
  this->sub(TR::Int64);
}

void
AbsEnv::add(TR::DataType datatype) {
  TR::VPConstraint *left = this->topAndPop();
  TR::VPConstraint *right = this->topAndPop();
  const bool leftExists = left;
  const bool rightExists = right;
  const bool canAdd = leftExists && rightExists;
  if (!canAdd) {
    this->pushNull();
    return;
  }

  TR::VPConstraint* result = left->add(right, datatype, this->_vp);
  //if (result) result->print(this->_comp, this->_comp->getOutFile());
  this->push(result);
  return;

}

void
AbsEnv::isub() {
  this->sub(TR::Int32);
}

void
AbsEnv::ificmpge(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  TR::VPIntConstraint *intConstraint = constraint ? constraint->asIntConstraint() : NULL;
  const auto branchFoldingDetected = intConstraint;
  int possibleValue = intConstraint ? intConstraint->getLow() : INT_MIN;
  int branchAddress = bytecodeIndex + branchOffset;
}

void
AbsEnv::ificmpeq(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  TR::VPIntConstraint *intConstraint = constraint ? constraint->asIntConstraint() : NULL;
  const auto branchFoldingDetected = intConstraint;
  int possibleValue = intConstraint ? intConstraint->getLow() : INT_MIN;
  int branchAddress = bytecodeIndex + branchOffset;
}

void
AbsEnv::ificmpne(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  TR::VPIntConstraint *intConstraint = constraint ? constraint->asIntConstraint() : NULL;
  const auto branchFoldingDetected = intConstraint;
  int possibleValue = intConstraint ? intConstraint->getLow() : INT_MIN;
  int branchAddress = bytecodeIndex + branchOffset;
}

void
AbsEnv::ificmplt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  TR::VPIntConstraint *intConstraint = constraint ? constraint->asIntConstraint() : NULL;
  const auto branchFoldingDetected = intConstraint;
  int possibleValue = intConstraint ? intConstraint->getHigh() : INT_MAX;
  int branchAddress = bytecodeIndex + branchOffset;
}


void
AbsEnv::ificmpgt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  TR::VPIntConstraint *intConstraint = constraint ? constraint->asIntConstraint() : NULL;
  const auto branchFoldingDetected = intConstraint;
  int possibleValue = intConstraint ? intConstraint->getLow() : INT_MIN;
  int branchAddress = bytecodeIndex + branchOffset;
}

void
AbsEnv::ificmple(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  TR::VPIntConstraint *intConstraint = constraint ? constraint->asIntConstraint() : NULL;
  const auto branchFoldingDetected = intConstraint;
  int possibleValue = intConstraint ? intConstraint->getHigh() : INT_MIN;
  int branchAddress = bytecodeIndex + branchOffset;
}


void
AbsEnv::ifgt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator& bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  bool constraintsAnArgument = this->_directDependency >= 0;
  if (!constraintsAnArgument) return;

  TR::VPConstraint * aPath = TR::VPIntRange::create(this->_vp, 1, INT_MAX);
  TR::VPConstraint *bPath = TR::VPIntRange::create(this->_vp, INT_MIN, 0);
  int argumentToConstraint = this->_directDependency;
  MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newIFGT(_hunk, bytecodeIndex);
  if (!constraintNewAPath) return;

  constraintNewAPath->at(argumentToConstraint, aPath);
  MethodSummaryRow *constraintNewBPath = this->_newMethodSummary->newIFGT(_hunk, bytecodeIndex);
  if (!constraintNewBPath) return;

  constraintNewBPath->at(argumentToConstraint, bPath);
  
}

void
AbsEnv::ifle(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  bool constraintsAnArgument = this->_directDependency >= 0;
  if (!constraintsAnArgument) return;

  TR::VPConstraint * aPath = TR::VPIntRange::create(this->_vp, INT_MIN, 0);
  TR::VPConstraint *bPath = TR::VPIntRange::create(this->_vp, 1, INT_MAX);
  int argumentToConstraint = this->_directDependency;
  MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newIFLE(_hunk, bytecodeIndex);
  if (!constraintNewAPath) return;

  constraintNewAPath->at(argumentToConstraint, aPath);
  MethodSummaryRow *constraintNewBPath = this->_newMethodSummary->newIFLE(_hunk, bytecodeIndex);
  if (!constraintNewBPath) return;

  constraintNewBPath->at(argumentToConstraint, bPath);

}

void
AbsEnv::ifnull(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint *sideeffect = this->topAndPop();
  bool constraintsAnArgument = this->_directDependency >= 0;
  if (!constraintsAnArgument) return;

  int argumentToConstraint = this->_directDependency;
  TR::VPNonNullObject *nonNull = TR::VPNonNullObject::create(this->_vp);
  MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newIFNU(_hunk, bytecodeIndex);
  if (!constraintNewAPath) return;

  constraintNewAPath->at(argumentToConstraint, nonNull);
  MethodSummaryRow *constraintNewBPath = this->_newMethodSummary->newIFNU(_hunk, bytecodeIndex);
  if (!constraintNewBPath) return;

  TR::VPConstraint *null = TR::VPNullObject::create(this->_vp);
  constraintNewBPath->at(argumentToConstraint, null);
}

void
AbsEnv::ifnonnull(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint *sideeffect = this->topAndPop();
  bool constraintsAnArgument = this->_directDependency >= 0;
  if (!constraintsAnArgument) return;

  int argumentToConstraint = this->_directDependency;
  TR::VPNonNullObject *nonNull = TR::VPNonNullObject::create(this->_vp);
  MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newIFNN(_hunk, bytecodeIndex);
  if (!constraintNewAPath) return;

  constraintNewAPath->at(argumentToConstraint, nonNull);
  TR::VPConstraint *null = TR::VPNullObject::create(this->_vp);
  MethodSummaryRow *constraintNewBPath = this->_newMethodSummary->newIFNU(_hunk, bytecodeIndex);
  if (!constraintNewBPath) return;

  constraintNewBPath->at(argumentToConstraint, null);
}

void
AbsEnv::ifge(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  bool constraintsAnArgument = this->_directDependency >= 0;
  if (!constraintsAnArgument) return;

  TR::VPConstraint * aPath = TR::VPIntRange::create(this->_vp, INT_MIN, -1);
  TR::VPConstraint *bPath = TR::VPIntRange::create(this->_vp, 0, INT_MAX);
  int argumentToConstraint = this->_directDependency;
  MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newIFGE(_hunk, bytecodeIndex);
  if (!constraintNewAPath) return;

  constraintNewAPath->at(argumentToConstraint, aPath);
  MethodSummaryRow *constraintNewBPath = this->_newMethodSummary->newIFGE(_hunk, bytecodeIndex);
  if (!constraintNewBPath) return;

  constraintNewBPath->at(argumentToConstraint, bPath);

}


void
AbsEnv::iflt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator& bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  bool constraintsAnArgument = this->_directDependency >= 0;
  if (!constraintsAnArgument) return;

  TR::VPConstraint * aPath = TR::VPIntRange::create(this->_vp, INT_MIN, -1);
  TR::VPConstraint *bPath = TR::VPIntRange::create(this->_vp, 0, INT_MAX);
  int argumentToConstraint = this->_directDependency;
  // what do I need to store here? The bytecode information...
  MethodSummaryRow* constraintNewAPath = this->_newMethodSummary->newIFLT(_hunk, bytecodeIndex);
  if (!constraintNewAPath) return;

  constraintNewAPath->at(argumentToConstraint, aPath);
  MethodSummaryRow *constraintNewBPath = this->_newMethodSummary->newIFLT(_hunk, bytecodeIndex);
  if (!constraintNewBPath) return;

  constraintNewBPath->at(argumentToConstraint, bPath);
}

void
AbsEnv::ifne(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  bool constraintsAnArgument = this->_directDependency >= 0;
  if (!constraintsAnArgument) return;

  TR::VPConstraint * aPath = TR::VPIntConst::create(this->_vp, 0);
  int argumentToConstraint = this->_directDependency;
  MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newIFNE(_hunk, bytecodeIndex);
  constraintNewAPath->at(argumentToConstraint, aPath);
}

void
AbsEnv::ifeq(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  TR::VPConstraint* constraint = (TR::VPConstraint*)this->topAndPop();
  bool constraintsAnArgument = this->_directDependency >= 0;
  if (!constraintsAnArgument) return;

  TR::VPConstraint* aPath = TR::VPIntConst::create(this->_vp, 0);
  int argumentToConstraint = this->_directDependency;
  MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newIFEQ(_hunk, bytecodeIndex);
  if (!constraintNewAPath) return;

  constraintNewAPath->at(argumentToConstraint, aPath);
}

void
AbsEnv::getstatic(int cpIndex) {
        
        TR::ResolvedMethodSymbol *methodSymbol = this->getMethodSymbol();
        TR::SymbolReference *symRef =  this->comp()->getSymRefTab()->findOrCreateStaticSymbol(methodSymbol, cpIndex, false);
        TR::StaticSymbol *symbol = symRef->getSymbol()->castToStaticSymbol();
        TR::DataType datatype = symbol->getDataType();
        switch(datatype) {
           case TR::Int32: 
            TRACE((this->_comp->trace(OMR::inlining)), "getstatic: Int32");
           break;
           case TR::Int64:
            TRACE((this->_comp->trace(OMR::inlining)), "getstatic: Int64");
           break;
           default:
            TRACE((this->_comp->trace(OMR::inlining)), "getstatic: Something");
           break;
        }
        
        this->pushNull();
}

void
AbsEnv::lstore0() {
  this->lstore(0);
}

void
AbsEnv::lstore1() {
  this->lstore(1);
}

void
AbsEnv::lstore2() {
  this->lstore(2);
}

void
AbsEnv::lstore3() {
  this->lstore(3);
}

void
AbsEnv::lstore(int n) {
  this->store(n);
}

void
AbsEnv::store(int n) {
  TR::VPConstraint * constraint = this->topAndPop();
  if (this->_comp->trace(OMR::inlining)) {
    TRACE((this->_comp->trace(OMR::inlining)), "[%d] = ", n);
    //if (constraint) constraint->print(this->_comp, this->_comp->getOutFile());
    TRACEEND((this->_comp->trace(OMR::inlining)), "\n");
  }
  this->at(n, constraint);
}

void
AbsEnv::istore(int n) {
  this->store(n);
}

void
AbsEnv::istore0() {
  this->istore(0);
}

void
AbsEnv::istore1() {
  this->istore(1);
}

void
AbsEnv::istore2() {
  this->istore(2);
}

void
AbsEnv::istore3() {
  this->istore(3);
}

void
AbsEnv::push(TR::VPConstraint* constraint) {
  if (this->_comp->trace(OMR::inlining)) {
    TRACE(true, "push: ");
    if (constraint) constraint->print(this->_comp, this->_comp->getOutFile());
    TRACEEND(true, "\n");
  }
  this->_operandStack.pushConstraint(constraint);
}

void
AbsEnv::iconst(int n) {
  this->_operandStack.pushConstInt(n);
}

void
AbsEnv::lconst(long n) {
  TR::VPLongConst *val = TR::VPLongConst::create( this->_vp, n);
  this->push(val);
}

void
AbsEnv::dconst0() {
  this->pushNull();
}

void
AbsEnv::dconst1() {
  this->pushNull();
}


void
AbsEnv::ddiv() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::dmul() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::drem() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::dstore3() {
  this->dstore(3);
}

void
AbsEnv::dstore2() {
  this->dstore(2);
}

void
AbsEnv::dstore1() {
  this->dstore(1);
}

void
AbsEnv::dstore0() {
  this->dstore(0);
}

void
AbsEnv::i2b() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::i2c() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::i2f() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::i2l() {
  TR::VPConstraint *integer = this->topAndPop();
  if (!integer) {
    this->pushNull();
    return;
  }

  TR::VPIntConst *constInt = integer->asIntConst();
  if (constInt) {
    int value = constInt->getLow();
    TR::VPLongConst *constLong = TR::VPLongConst::create(this->_vp, value);
    this->push(constLong);
    return;
  }

  TR::VPIntConstraint *intConstraint = integer->asIntConstraint();
  if (!intConstraint) {
    this->pushNull();
    return;
  }
  int low = intConstraint->getLow();
  int high = intConstraint->getHigh();
  TR::VPLongConstraint *longRange = TR::VPLongRange::create(this->_vp, low, high);
  this->push(longRange);
}

void
AbsEnv::i2s() {
  TR::VPConstraint *integer = this->topAndPop();
  if (!integer) {
    this->pushNull();
    return;
  }

  TR::VPIntConst *constInt = integer->asIntConst();
  if (constInt) {
    int value = constInt->getLow();
    TR::VPShortConst *constLong = TR::VPShortConst::create(this->_vp, value);
    this->push(constLong);
    return;
  }

  TR::VPIntConstraint *intConstraint = integer->asIntConstraint();
  if (!intConstraint) {
    this->pushNull();
    return;
  }

  int low = intConstraint->getLow();
  int high = intConstraint->getHigh();
  TR::VPShortConstraint *range = TR::VPShortRange::create(this->_vp, low, high);
  this->push(range);
}

void
AbsEnv::dsub() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::dstore(int index) {
  this->pop();
  this->at(index, NULL);
}

void
AbsEnv::dneg() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::dload0() {
  this->pushNull();
}

void
AbsEnv::dload(int index) {
  this->pushNull();
}

void
AbsEnv::lconst0() {
  this->lconst(0);
}

void
AbsEnv::lconst1() {
  this->lconst(1);
}

void
AbsEnv::iconst0() {
  this->iconst(0);
}

void
AbsEnv::iconst1() {
  this->iconst(1);
}

void
AbsEnv::iconst2() {
  this->iconst(2);
}

void
AbsEnv::iconst3() {
  this->iconst(3);
}

void
AbsEnv::iconst4() {
  this->iconst(4);
}

void
AbsEnv::iconst5() {
  this->iconst(5);
}

TR::VPConstraint*
AbsEnv::at(int i) {
  if (i >= 32) { return NULL; }
  if (i < 0) { return NULL; }
  return this->_variableArray.at(i);
}

void
AbsEnv::at(int i, TR::VPConstraint *constraint) {
  if (i >= 32) { return; }
  if (i < 0) { return; }
  this->_variableArray.at(i, constraint);
}

void
AbsEnv::updateReturn(TR::VPConstraint* constraint) {
  if (this->_firstReturn) {
    this->_returns = constraint;
    this->_firstReturn = false;
    return;
  }

  if (this->_returns && constraint) {
    this->_returns->merge(constraint, this->_vp);
  }
}


void
AbsEnv::ireturn() {
  TR::VPConstraint* constraint = this->topAndPop();
  this->updateReturn(constraint);
}

void
AbsEnv::handleGetClass(int bcIndex, IDTNode * hunk) {
  TR::VPConstraint* constraint = this->topAndPop();
  this->pushNull();

  int argumentToConstraint = this->_directDependency;
  TRACE((this->_comp->trace(OMR::inlining)), "handleGetClass: arg constrained %d", argumentToConstraint);
  if (argumentToConstraint < 0) return;
  TR::VPNonNullObject *nonNull = TR::VPNonNullObject::create(this->_vp);
  MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newNLCK(_hunk, bcIndex);
  constraintNewAPath->at(argumentToConstraint, nonNull);

}


void
AbsEnv::handleLengthInternal(int bytecodeIndex, IDTNode* hunk) {
#ifdef TR_PROFITABILITY_VERIFY
  if (hunk) {
      hunk->_visited = true;
  }
#endif
  TRACE((this->_comp->trace(OMR::inlining)), "handleLengthInternal");
  auto dependsOnArgument = this->_directDependency > -1;
  TRACE((this->_comp->trace(OMR::inlining)), "handleLengthInternal on argument constraint");

  if (!dependsOnArgument) {
    this->pushNull();
    return;
  }

  TR::VPConstraint* constraint = this->topAndPop();
  if (!constraint) {
    this->pushNull();
    return;
  }

  bool isConstString = constraint->isConstString();
  if (!isConstString) {
    this->pushNull();
    return;
  }

  TR::VPConstString *constString = constraint->asConstString();
  int32_t size = -1; //constString->length(this->_comp);
  if (size == -1) {
    this->pushNull();
    if (hunk) {
      MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newSTR_LEN(hunk, bytecodeIndex);
    } else {
      MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newSTR_LEN(this->_hunk, bytecodeIndex);
    }
    return;
  }
  TR::VPIntConst *length = TR::VPIntConst::create( this->_vp, size);
  this->push(length);
  if (hunk) {
      MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newSTR_LEN(hunk, bytecodeIndex);
  } else {
      MethodSummaryRow *constraintNewAPath = this->_newMethodSummary->newSTR_LEN(this->_hunk, bytecodeIndex);
  }
  return;
}

bool
AbsEnv::isLengthInternal(const IDTNode* hunk) const {
  return false;
  char *nameChars= "lengthInternal";
  int nameLength = strlen(nameChars);
  char *signatureChars = "()I";
  int signatureLength = strlen(signatureChars);
  char *className = "java/lang/String";
  int classNameLength = strlen(className);
  
  //return hunk->isSameMethod(nameLength, nameChars, signatureLength, signatureChars, classNameLength, className);
  return false;
}

bool
AbsEnv::isLengthInternal(int cpIndex) {
  return false;
  TR::ResolvedMethodSymbol *callerResolvedMethodSymbol = this->getMethodSymbol();
  TR_ResolvedMethod *callerResolvedMethod = callerResolvedMethodSymbol->getResolvedMethod();
  TR::Method *calleeMethod = this->_comp->fej9()->createMethod(this->_comp->trMemory(), callerResolvedMethod->containingClass(), cpIndex);
  const char* methodName = calleeMethod->signature(this->_comp->trMemory());
  const char* lengthInternal = "java/lang/String.lengthInternal()I";
  const size_t strlen_lengthInternal = strlen(lengthInternal);
  bool isLengthInternal = strncmp(methodName, lengthInternal, strlen_lengthInternal) == 0;
  return isLengthInternal;
}

bool
AbsEnv::isGetClass(int cpIndex) {
  return false;
  TR::ResolvedMethodSymbol *callerResolvedMethodSymbol = this->getMethodSymbol();
  TR_ResolvedMethod *callerResolvedMethod = callerResolvedMethodSymbol->getResolvedMethod();
  TR::Method *calleeMethod = this->_comp->fej9()->createMethod(this->_comp->trMemory(), callerResolvedMethod->containingClass(), cpIndex);
  const char* methodName = calleeMethod->signature(this->_comp->trMemory());
  const char* lengthInternal = "java/lang/Object.getClass()Ljava/lang/Class;";
  const size_t strlen_lengthInternal = strlen(lengthInternal);
  bool isLengthInternal = strncmp(methodName, lengthInternal, strlen_lengthInternal) == 0;
  return isLengthInternal;
}

bool
AbsEnv::isGetClass(const IDTNode* hunk) const {
  return false;
  char *nameChars= "getClass";
  int nameLength = strlen(nameChars);
  char *signatureChars = "()Ljava/lang/Class;";
  int signatureLength = strlen(signatureChars);
  char *className = "java/lang/Object";
  int classNameLength = strlen(className);
  
  return false;
  //return hunk->isSameMethod(nameLength, nameChars, signatureLength, signatureChars, classNameLength, className);
}

void
AbsEnv::invokeinterface(int bcIndex, int cpIndex) {
  this->invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Interface);
}

void
AbsEnv::invokespecial(int bcIndex, int cpIndex) {
  this->invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Special);
}

void
AbsEnv::invokestatic(int bcIndex, int cpIndex) {
  this->invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Static);
}

void
AbsEnv::invoke(int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind) {

  static const char* avoidInvoke = feGetEnv("TR_AvoidInvoke");
  if (avoidInvoke) {
    IDTNode *next = this->_hunk->findChildWithBytecodeIndex(bcIndex);
    if (!next) {
      this->handleInvokeWithoutHunk(bcIndex, cpIndex, kind);
      return;
    }
    int paramsInCall = next->numberOfParameters();
    for (int i = 0; i < paramsInCall; i++) {
      TR::VPConstraint *constraint = this->topAndPop();
    }
    this->push(NULL);
    return;
  }

  bool multipleImplementors = kind == TR::MethodSymbol::Kinds::Virtual || kind == TR::MethodSymbol::Kinds::Interface;
  IDTNode *next = this->_hunk->findChildWithBytecodeIndex(bcIndex);
  if (!next) {
    this->handleInvokeWithoutHunk(bcIndex, cpIndex, kind);
    return;
  }


  bool isLengthInternal = this->isLengthInternal(cpIndex);
  if (isLengthInternal) {
   TRACE((this->_comp->trace(OMR::inlining)), "IS LENGTH INTERNAL\n");
   this->handleLengthInternal(bcIndex, next);
   return;
  }

  bool isGetClass = this->isGetClass(cpIndex);
  if (isGetClass) {
    TRACE((this->_comp->trace(OMR::inlining)), "IS GET CLASS \n");
    this->handleGetClass(bcIndex, next);
    return;
  }

    static const char * avoidMap = feGetEnv("TR_AvoidMap");

     TRACE((this->_comp->trace(OMR::inlining)), "invoking %s callee_idx = %d\n", next->name(), next->callee_idx());
    if (multipleImplementors) {
        TR::ResolvedMethodSymbol *rms = next->getResolvedMethodSymbol();
        TR_ResolvedMethod *rm = rms->getResolvedMethod();
        TR_OpaqueClassBlock *implicitParameterClass = rm->containingClass();
        TR::VPConstraint* parameterClass = this->getClassConstraint(implicitParameterClass);
    }

    static const char * recomputeMethodSummaries = feGetEnv("TR_RecomputeSummaries");
    MethodSummary *methodSummary= avoidMap ? NULL : this->getSummaryFromBuffer(next);
    bool found = false;
    // compare information at callsite...
    if (!methodSummary || recomputeMethodSummaries) {
       methodSummary = NULL;
       //methodSummary = analyzeCFG(next, this->_comp, this->_region, this);
       found = methodSummary != NULL;
    }

#ifdef TR_PROFITABILITY_VERIFY
    next->_methodSummary = methodSummary;
#endif


     if (found && !avoidMap) {
      //this->_precomputedMethod->insert({next->name(), methodSummary});
     }

     int paramsInCall = next->numberOfParameters();
     AbsVarArray* temp = new (this->_region) AbsVarArray(this->_comp, this->_region);

      for (int i = 0; i < paramsInCall; i++) {
        TR::VPConstraint *constraint = this->topAndPop();
        //if (constraint) constraint->print(this->_comp, this->_comp->getOutFile());
        TRACE((this->_comp->trace(OMR::inlining)), "\n");
        temp->at(paramsInCall - 1 - i, constraint);
      }

      int benefit = methodSummary ? methodSummary->applyVerbose(temp, next) : 0;
#ifdef TR_PROFITABILITY_VERIFY
      next->_argumentConstraints = temp;
#endif
      //TODO: move this to a row
      //TODO: this 15 comes from where?
      static const char * ignoreSize = feGetEnv("TR_ProfitabilityIgnoreSize");
      int smallSize = ignoreSize ? 0 : 15 - next->getCost();
      benefit = smallSize > 0 ? benefit + smallSize : benefit;
      TRACE((this->_comp->trace(OMR::inlining)), "AND THE BENEFIT FOR %s id=%d IS: %d\n", next->name(), next->callee_idx(), benefit);
      static const char * profitabilityDiscard = feGetEnv("TR_ProfitabilityDiscard");
      benefit = profitabilityDiscard ? 0 : benefit;
      next->setBenefit(benefit + 1);
      this->push(NULL);
  return;
  //List<IDTNode> nexts = this->_hunk->findChildrenWithBytecodeIndex(bcIndex);
/*
  ListIterator<IDTNode> hunkIt(&nexts);
  for (IDTNode* next = hunkIt.getFirst(); next != NULL; next = hunkIt.getNext()) {

    if (multipleImplementors) {
        TRACE((this->_comp->trace(OMR::inlining)), "Potentially multiple implementors for method %s\n", next->name());
        TR::ResolvedMethodSymbol *rms = next->getResolvedMethodSymbol();
        TR_ResolvedMethod *rm = rms->getResolvedMethod();
        TR_OpaqueClassBlock *implicitParameterClass = rm->containingClass();
        TR::VPConstraint* parameterClass = this->getClassConstraint(implicitParameterClass);
        if (!parameterClass) break;
        TRACE((this->_comp->trace(OMR::inlining)), "I have constraint\n");
        
        if (this->_comp->trace(OMR::inlining)) {
           //parameterClass->print(this->_comp, this->_comp->getOutFile());
        }
    }
    const bool isLengthInternal = this->isLengthInternal(next);
    if (isLengthInternal) {
      this->handleLengthInternal(bcIndex, next);
      return;
    }

*/
/*
    const bool isGetClass = this->isGetClass(next);
    if (isGetClass) {
      this->handleGetClass(next);
      return;
    }
*/

/*
    MethodSummary *methodSummary= this->getSummaryFromBuffer(next);
    bool found = false;
    if (!methodSummary) {
       methodSummary = NULL;
       //methodSummary = analyzeCFG(next, this->_comp, this->_region, this);
       found = methodSummary != NULL;
    }

#ifdef TR_PROFITABILITY_VERIFY
    next->_methodSummary = methodSummary;
#endif


     if (found) {
      //this->_precomputedMethod->insert({next->name(), methodSummary});
     }

     int paramsInCall = next->numberOfParameters();
     AbsVarArray* temp = new (*this->_region) AbsVarArray(this->_comp, this->_region);

      for (int i = 0; i < paramsInCall; i++) {
        TR::VPConstraint *constraint = this->topAndPop();
        //if (constraint) constraint->print(this->_comp, this->_comp->getOutFile());
        TRACE((this->_comp->trace(OMR::inlining)), "\n");
        temp->at(paramsInCall - 1 - i, constraint);
      }

      int benefit = methodSummary ? methodSummary->applyVerbose(temp, next) : 0;
#ifdef TR_PROFITABILITY_VERIFY
      next->_argumentConstraints = temp;
#endif
      //TODO: move this to a row
      //TODO: this 15 comes from where?
      int smallSize = 15 - next->cost();
      benefit = smallSize > 0 ? benefit + smallSize : benefit;
      TRACE((this->_comp->trace(OMR::inlining)), "AND THE BENEFIT FOR %s id=%d IS: %d\n", next->name(), next->callee_idx(), benefit);
      next->benefit(benefit + 1);
      this->push(NULL);
  }



  if (nexts.getSize() == 0) { 
    this->handleInvokeWithoutHunk(bcIndex, cpIndex, kind);
    return;
  }
*/
}

MethodSummary*
AbsEnv::getSummaryFromBuffer(IDTNode* aHunk) {
  //return this->getSummaryFromBuffer(aHunk->name());
  return NULL;
}

MethodSummary*
AbsEnv::getSummaryFromBuffer(const char *name) {
  //auto iterPos = this->_precomputedMethod->find(name);
  //auto hasBeenSeen = iterPos != this->_precomputedMethod->end();
  //if (hasBeenSeen) {
  // return iterPos->second;
  //}
  return NULL;
}

void
AbsEnv::handleInvokeWithoutHunk(int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind) {
  TR::ResolvedMethodSymbol *callerResolvedMethodSymbol = this->getMethodSymbol();
  TR_ResolvedMethod *callerResolvedMethod = callerResolvedMethodSymbol->getResolvedMethod();
  TR::SymbolReference *symRef;
  switch(kind) {
    case TR::MethodSymbol::Kinds::Virtual:
      symRef = this->_comp->getSymRefTab()->findOrCreateVirtualMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
    case TR::MethodSymbol::Kinds::Interface:
      symRef = this->_comp->getSymRefTab()->findOrCreateInterfaceMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
    case TR::MethodSymbol::Kinds::Static:
      symRef = this->_comp->getSymRefTab()->findOrCreateStaticMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
    case TR::MethodSymbol::Kinds::Special:
      symRef = this->_comp->getSymRefTab()->findOrCreateSpecialMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
  }
  TR::Method *calleeMethod = this->_comp->fej9()->createMethod(this->_comp->trMemory(), callerResolvedMethod->containingClass(), cpIndex);
  const char* signature = calleeMethod->signature(this->_comp->trMemory());
  TRACE((this->_comp->trace(OMR::inlining)), "unknown invoke : signature %s\n", signature);
  
  bool isLengthInternal = this->isLengthInternal(cpIndex);
  if (isLengthInternal) {
   TRACE((this->_comp->trace(OMR::inlining)), "IS LENGTH INTERNAL\n");
   this->handleLengthInternal(bcIndex);
   return;
  }
  TRACE((this->_comp->trace(OMR::inlining)), "IS NOT LENGTH INTERNAL\n");

  bool isGetClass = this->isGetClass(cpIndex);
  if (isGetClass) {
    this->handleGetClass(bcIndex);
    return;
  }
  
  int numberOfExplicitParameters = calleeMethod->numberOfExplicitParameters();
  int isStatic = kind == TR::MethodSymbol::Kinds::Static;
  int numberOfImplicitParameters = isStatic ? 0 : 1;
  int numberOfParameters = numberOfExplicitParameters + numberOfImplicitParameters;

  for (int i = 0; i < numberOfParameters; i++) {
    this->pop();
  }

  TR::DataType returnType = calleeMethod->returnType();
  TR::DataType noType = TR::NoType;
  bool returnTypeIsVoid = returnType == noType;
  if (returnTypeIsVoid) {
     return;
  }

  TR::DataType address = TR::Address;
  bool returnTypeIsPrimitive = returnType != address;
  if (returnTypeIsPrimitive) {
    this->pushNull();
    return;
  }

  const auto isResolved = !symRef->isUnresolved();
  const auto canKnowReturnType = isResolved;
  if (!canKnowReturnType) {
    this->pushNull();
    return;
  }

  TR_ResolvedMethod *calleeResolvedMethod = NULL;
  switch(kind) {
    case TR::MethodSymbol::Kinds::Static :
      calleeResolvedMethod = callerResolvedMethod->getResolvedStaticMethod(this->_comp, cpIndex); 
    break;
    case TR::MethodSymbol::Kinds::Special :
      calleeResolvedMethod = callerResolvedMethod->getResolvedSpecialMethod(this->_comp, cpIndex); 
    break;
    case TR::MethodSymbol::Kinds::Virtual :
      calleeResolvedMethod = callerResolvedMethod->getResolvedVirtualMethod(this->_comp, cpIndex); 
    break;
    default:
      this->pushNull();
      return;
    break;
  }

  //TODO: is it calleeResolvedMethod or are you returning the type of the callee? is classOfMethod same as you expect
  TR_OpaqueClassBlock *returnClass = calleeResolvedMethod->classOfMethod();
  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, returnClass);
  TR::VPObjectLocation *objectLocation = NULL;
  TR::VPConstraint *constraint = TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, objectLocation);
  this->push(constraint);
}

void
AbsEnv::invokevirtual(int bcIndex, int cpIndex) {
  this->invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Virtual);
}

TR::ResolvedMethodSymbol*
AbsEnv::getMethodSymbol() {
  return this->_hunk->getResolvedMethodSymbol();
}

TR_ResolvedMethod*
AbsEnv::getMethod() {
  return this->_hunk->getResolvedMethodSymbol()->getResolvedMethod();
}

void
AbsEnv::l2d() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::d2f() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::f2d() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::f2l() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::f2i() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::d2i() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::d2l() {
  this->pop();
  this->pushNull();
}


void
AbsEnv::l2f() {
  this->pop();
  this->pushNull();
}

void
AbsEnv::lor() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::lrem() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::lshl() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::lshr() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::lushr() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::lxor() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::land() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::lmul() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::ldiv() {
  this->pop();
  this->pop();
  this->pushNull();
}

void
AbsEnv::l2i() {
  TR::VPConstraint *_long = this->topAndPop();
  if (!_long) {
    this->pushNull();
    return;
  }

  TR::VPLongConst *_longConst = _long->asLongConst();
  if (!_longConst) {
    this->pushNull();
    return;
  }

  long _longVal = _longConst->getLow();
  TR::VPIntConst *constraint = TR::VPIntConst::create(this->_vp, (int)_longVal);
  this->push(constraint);
}

void
AbsEnv::ldc(int cpIndex) {
  static char *floatInCP = feGetEnv("TR_FloatInCP");
  TR_ResolvedMethod* method = this->_hunk->getResolvedMethodSymbol()->getResolvedMethod();
  auto datatype = this->getMethod()->getLDCType(cpIndex);
  switch(datatype) {
    case TR::Int32: 
    {
       int32_t a =  method->intConstant(cpIndex) ; 
       TRACE((this->_comp->trace(OMR::inlining)), "getAbstractOperandStack : intConstant %u %d", a, a);
       
       if (this->_vp) { 
          TR::VPIntConst *constraint = TR::VPIntConst::create(this->_vp, a);
          this->push(constraint);
       } else { this->pushNull() ; }
    }
    break;
    case TR::Int64: 
    {
       auto a = method->longConstant(cpIndex); 
       TRACE((this->_comp->trace(OMR::inlining)), "getAbstractOperandStack : longConstant %lu %ld", a, a);
       TR::VPLongConst *constraint = TR::VPLongConst::create(this->_vp, a);
       this->push(constraint);
    }
    break;
    case TR::Float:
    {
        TR::ResolvedMethodSymbol *methodSymbol = this->_hunk->getResolvedMethodSymbol();
        float *a = method->floatConstant(cpIndex);
        int low = (int)(*a);
        int high = (int)(*a+1);
        TR::VPIntConstraint *constraint = TR::VPIntRange::create(this->_vp, low, high);
        this->push(constraint);
        TRACE((this->_comp->trace(OMR::inlining)), "getAbstractOperandStack : floatConstant %f", *a);
    }
    break;
    case TR::Double:
    {
      if (!floatInCP) {
        auto a = method->doubleConstant(cpIndex, this->comp()->trMemory()); 
        TRACE((this->comp()->trace(OMR::inlining)), "getAbstractOperandStack : doubleConstant %f", *a);
      } else {
        TR::ResolvedMethodSymbol *methodSymbol = this->_hunk->getResolvedMethodSymbol();
        auto a = this->comp()->getSymRefTab()->findOrCreateDoubleSymbol(methodSymbol, cpIndex);
        TRACE((this->comp()->trace(OMR::inlining)), "getAbstractOperandStack : doubleConstant %f", *a);
      }
        this->pushNull();
    }
    break; 
    case TR::Address:
    {
      TR_ResolvedMethod* method = this->_hunk->getResolvedMethodSymbol()->getResolvedMethod();
      if (method->isStringConstant(cpIndex)) {
        TR::ResolvedMethodSymbol *methodSymbol = this->_hunk->getResolvedMethodSymbol();
         this->ldcString(cpIndex, methodSymbol);
      }
      else if (method->isClassConstant(cpIndex))
      {
          TR_OpaqueClassBlock* type = method->getClassFromConstantPool(_comp, cpIndex);
          if (!type) { pushNull(); return; }
          TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, type);
          TR::VPObjectLocation *objectLocation = NULL;
          TR::VPNonNullObject *nonNull = TR::VPNonNullObject::create(this->_vp);
          TR::VPConstraint *typeConstraint= TR::VPClass::create(this->_vp, fixedClass, nonNull, NULL, NULL, objectLocation);
          this->push(typeConstraint);
      } else {
        this->pushNull();
      }
    }
    break;
    default:
    break;
  }
}

void
AbsEnv::ldcString(int cpIndex, TR::ResolvedMethodSymbol* methodSymbol) {

        TRACE((this->_comp->trace(OMR::inlining)), "ldc string");
        TR::SymbolReference *symRef =  this->comp()->getSymRefTab()->findOrCreateStringSymbol(methodSymbol, cpIndex);
        if (symRef->isUnresolved()) { this->pushNull(); return; }

        TRACE((this->_comp->trace(OMR::inlining)), "ldc string is resolved");
        TR::VPConstraint *constraint = TR::VPConstString::create(this->_vp, symRef);
        this->push(constraint);
}

void
AbsEnv::pushNull() {
  TRACE((this->_comp->trace(OMR::inlining)), "push: NULL");
  this->_operandStack.pushNullConstraint();
}

void
AbsEnv::aload(int n) {
  this->aloadn(n);
}

void
AbsEnv::aload0getfield(int n) {
  this->aload0();
  this->pushNull();
  //this->getfield(n);
}

void
AbsEnv::aload0() {
  this->aloadn(0);
}

void
AbsEnv::aload1() {
  this->aloadn(1);
}

void
AbsEnv::aload2() {
  this->aloadn(2);
}

void
AbsEnv::aload3() {
  this->aloadn(3);
}

void
AbsEnv::lload(int n) {
  this->aloadn(n);
}

void
AbsEnv::lload0() {
  this->lload(0);
}

void
AbsEnv::lload1() {
  this->lload(1);
}

void
AbsEnv::lload2() {
  this->lload(2);
}

void
AbsEnv::lload3() {
  this->lload(3);
}

void
AbsEnv::iload1() {
  this->iload(1);
}

void
AbsEnv::iload2() {
  this->iload(2);
}

void
AbsEnv::iload3() {
  this->iload(3);
}

void
AbsEnv::lneg() {
  TR::VPConstraint *value1 = this->topAndPop();
  if (!value1) {
    this->pushNull();
    return;
  }

  TR::VPLongConstraint *value = value1->asLongConstraint();
  if (!value) {
   this->pushNull();
   return;
  }
  TR::VPLongConst *zero= TR::VPLongConst::create(this->_vp, 0);
  TR::VPConstraint *res = zero->subtract(value, TR::Int64, this->_vp);
  this->push(res);
}

void
AbsEnv::ineg() {
  TR::VPConstraint *value = this->topAndPop();
  if (!value) {
    this->pushNull();
    return;
  }
  TR::VPIntConst *zero= TR::VPIntConst::create(this->_vp, 0);
  TR::VPConstraint *res = zero->subtract(value, TR::Int32, this->_vp);
  this->push(res);
}

void
AbsEnv::iload(int n) {
  this->aloadn(n);
}

int
AbsEnv::numberOfParametersInCurrentMethod() {
  TR::ResolvedMethodSymbol *resolvedMethodSymbol = this->_hunk->getResolvedMethodSymbol();
  TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
  const auto numberOfParameters = resolvedMethod->numberOfParameters();
  return numberOfParameters;
}

void
AbsEnv::aloadn(int n) {
  if (n > 31) {
    this->pushNull();
    return;
  }
  TR::ResolvedMethodSymbol *resolvedMethodSymbol = this->_hunk->getResolvedMethodSymbol();
  TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
  const auto numberOfParameters = resolvedMethod->numberOfParameters();

  TR::VPConstraint* constraint = this->_variableArray.at(n);
  if (n < numberOfParameters) {
     this->_directDependency = n;
  } else {
     this->_directDependency = -1;
  }

  if (this->_comp->trace(OMR::inlining)) {
    TRACE((this->_comp->trace(OMR::inlining)), "[%d]: ", n);
    //if (constraint) constraint->print(this->_comp, this->_comp->getOutFile());
    TRACEEND((this->_comp->trace(OMR::inlining)), "\n");
  }
  this->push(constraint);
}
