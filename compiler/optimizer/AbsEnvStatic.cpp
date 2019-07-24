#include "compiler/optimizer/AbsEnvStaticBase.hpp"

#include "compile/SymbolReferenceTable.hpp"
#include "compile/J9SymbolReferenceTable.hpp"
#include "compiler/env/j9method.h"
#include "compiler/il/SymbolReference.hpp"
#include "compiler/ilgen/J9ByteCodeIterator.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/infra/ILWalk.hpp"
#include "compiler/il/OMRBlock.hpp"
#include "il/Block.hpp"

AbsEnvStaticBase::AbsEnvStaticBase(TR::Region &region, IDT::Node *node) :
  _region(region),
  _array(region, node->maxLocals()),
  _stack(region, node->maxStack()),
  _node(node),
  _vp(node->getValuePropagation()),
  _rms(node->getResolvedMethodSymbol())
{
}

AbsEnvStaticBase::AbsEnvStaticBase(AbsEnvStaticBase &other) :
  _region(other._region),
  _array(other._array, other._region),
  _stack(other._stack, other._region),
  _node(other._node),
  _vp(other._vp),
  _rms(other._rms)
{
}

AbsEnvStaticBase *AbsEnvStaticBase::getWidened()
  {
  AbsEnvStaticBase *top = new (_region) AbsEnvStaticBase(*this);
  top->_array = *top->_array.getWidened(_region);
  top->_stack = *top->_stack.getWidened(_region);
  return top;
  }

void
AbsEnvStaticBase::merge(AbsEnvStaticBase &other)
{
   traceMsg(TR::comp(), "Merging abstract environment A");
   this->trace();

   
   TR::Region &region = TR::comp()->trMemory()->currentStackRegion();
   // TODO: can I do without copying, maybe if I forgot about const &
   AbsEnvStaticBase copyOfOther(other);
   traceMsg(TR::comp(), "Merging abstract environment B");
   copyOfOther.trace();

   this->_array.merge(copyOfOther._array, this->_region, this->_vp);
   this->_stack.merge(copyOfOther._stack, this->_region, this->_vp);
   this->trace();
   
}

//TODO: get rid of _vp
//TODO: get rid of _rms
//TODO? get rid of _region
//TODO? get rid of _node

AbsValue*
AbsEnvStaticBase::pop()
  {
  AbsValue *absValue = this->_stack.top();
  this->_stack.pop();
  return absValue;
  }

void
AbsEnvStaticBase::push(AbsValue *absValue)
  {
  this->_stack.push(absValue);
  }

void
AbsEnvStaticBase::pushNull()
  {
  this->_stack.push(new (_region) AbsValue(NULL, TR::Address));
  }

void
AbsEnvStaticBase::pushConstInt(int n)
  {
  TR::VPIntConst *intConst = TR::VPIntConst::create(this->_vp, n);
  this->_stack.push(new (_region) AbsValue(intConst, TR::Int32));
  }

void
AbsEnvStaticBase::enterMethod(TR::ResolvedMethodSymbol *rms)
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
        break;

        default:
        //TODO: what about vectors and aggregates?
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
AbsEnvStaticBase::getTopDataType(TR::DataType dt)
  {
  // TODO: Don't allocate new memory and always return the same set of values
  return new (_region) AbsValue(NULL, dt);
  }

AbsValue*
AbsEnvStaticBase::getClassConstraint(TR_OpaqueClassBlock *opaqueClass)
  {
  if (!opaqueClass) return NULL;

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, opaqueClass);
  TR::VPConstraint *classConstraint = TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, NULL);
  return new (_region) AbsValue (classConstraint, TR::Address);
  }

void
AbsEnvStaticBase::at(unsigned int index, AbsValue *constraint)
  {
  this->_array.at(index, constraint);
  }

AbsValue*
AbsEnvStaticBase::at(unsigned int index)
  {
  return this->_array.at(index);
  }

void
AbsEnvStaticBase::aload0getfield(int i, TR_J9ByteCodeIterator &bci) {
  this->aload0();
  //this->getfield(i, bci);
}

void
AbsEnvStaticBase::interpretByteCode(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci)
  {
  TR::Compilation *comp = TR::comp();
  if (comp->trace(OMR::benefitInliner)) {
     bci.printByteCode();
     traceMsg(comp, "has size of = %d\n", bci.size(bc));
  }

  switch(bc)
     {
     //alphabetical order
     case 215: this->aload0getfield(0, bci); break; //aload0getfield
     case J9BCaaload: this->aaload(); break;
     case J9BCaastore: this->aastore(); break;
     case J9BCaconstnull: this->aconstnull(); break;
     case J9BCaload: this->aload(bci.nextByte()); break;
     case J9BCaload0: this->aload0(); break;
     case J9BCaload1: this->aload1(); break;
     case J9BCaload2: this->aload2(); break;
     case J9BCaload3: this->aload3(); break;
     case J9BCanewarray: this->anewarray(bci.next2Bytes()); break;
     //case J9BCareturn: this->pop(); break; // FIXME: own function? other semantics for return;
     case J9BCarraylength: this->arraylength(); break;
     case J9BCastore: this->astore(bci.nextByte()); break;
     case J9BCastorew: this->astore(bci.next2Bytes()); break; // WARN: internal bytecode
     case J9BCastore0: this->astore0(); break;
     case J9BCastore1: this->astore1(); break;
     case J9BCastore2: this->astore2(); break;
     case J9BCastore3: this->astore3(); break;
     //TODO: case J9BCathrow: this->athrow(); break;
     case J9BCbaload: this->baload(); break;
     case J9BCbastore: this->bastore(); break;
     case J9BCbipush: this->bipush(bci.nextByte()); break;
     case J9BCcaload : this->aaload(); break; //TODO: own function
     case J9BCcastore: this->aastore(); break; //TODO: own function
     case J9BCcheckcast: this->checkcast(bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCd2f: this->d2f(); break;
     case J9BCd2i: this->d2i(); break;
     case J9BCd2l: this->d2l(); break;
     case J9BCdadd: this->dadd(); break;
     case J9BCdaload: this->daload(); break;
     case J9BCdastore: this->aastore(); break; //TODO own function
     case J9BCdcmpl: case J9BCdcmpg: this->dcmpl(); break; //TODO: own functions?
     case J9BCdconst0: case J9BCdconst1: this->dconst0(); break; //TODO: own functions?
     case J9BCddiv: this->ddiv(); break;
     case J9BCdload: this->dload(bci.nextByte()); break;
     case J9BCdload0: this->dload0(); break;
     case J9BCdload1: this->dload1(); break;
     case J9BCdload2: this->dload2(); break;
     case J9BCdload3: this->dload3(); break;
     case J9BCdmul: this->dmul(); break;
     case J9BCdneg: /* yeah nothing */ break;
     case J9BCdrem: this->drem(); break;
     //case J9BCdreturn: this->pop(); this->pop(); break; //FIXME: return semantics
     case J9BCdstore: this->dstore(bci.nextByte()); break;
     case J9BCdstorew: this->dstore(bci.next2Bytes()); break; // WARN: internal bytecode
     case J9BCdstore0: this->dstore0(); break;
     case J9BCdstore1: this->dstore1(); break;
     case J9BCdstore2: this->dstore2(); break;
     case J9BCdstore3: this->dstore3(); break;
     case J9BCdsub: this->dsub(); break;
     case J9BCdup: this->dup(); break;
     case J9BCdupx1: this->dupx1(); break;
     case J9BCdupx2: this->dupx2(); break;
     case J9BCdup2: this->dup2(); break;
     case J9BCdup2x1: this->dup2x1(); break;
     case J9BCdup2x2: this->dup2x2(); break;
     case J9BCf2d: this->f2d(); break;
     case J9BCf2i: this->f2i(); break;
     case J9BCf2l: this->f2l(); break;
     case J9BCfadd: this->fadd(); break;
     case J9BCfaload: this->aaload(); break; //TODO: own function
     case J9BCfastore: this->aastore(); break; //TODO own function
     case J9BCfcmpl: case J9BCfcmpg: this->fcmpl(); break; //TODO: own functions?
     case J9BCfconst0: case J9BCfconst1: this->fconst0(); break; //TODO: own functions?
     case J9BCfdiv: this->fdiv(); break;
     case J9BCfload: this->fload(bci.nextByte()); break;
     case J9BCfload0: this->fload0(); break;
     case J9BCfload1: this->fload1(); break;
     case J9BCfload2: this->fload2(); break;
     case J9BCfload3: this->fload3(); break;
     case J9BCfmul: this->fmul(); break;
     case J9BCfneg: /* yeah nothing */ break;
     case J9BCfrem: this->frem(); break;
     //case J9BCfreturn: this->pop(); break; //FIXME: return semantics
     case J9BCfstore: this->fstore(bci.nextByte()); break;
     case J9BCfstorew: this->fstore(bci.next2Bytes()); break; // WARN: internal bytecode
     case J9BCfstore0: this->fstore0(); break;
     case J9BCfstore1: this->fstore1(); break;
     case J9BCfstore2: this->fstore2(); break;
     case J9BCfstore3: this->fstore3(); break;
     case J9BCfsub: this->fsub(); break;
     case J9BCgetfield: this->getfield(bci.next2Bytes(), bci); break;
     case J9BCgetstatic: this->getstatic(bci.next2Bytes(), bci); break;
     case J9BCgoto: bci.next2BytesSigned(); break;
     case J9BCgotow: bci.next4BytesSigned(); break;
     case J9BCi2b: this->i2b(); break;
     case J9BCi2c: this->i2c(); break;
     case J9BCi2d: this->i2d(); break;
     case J9BCi2f: this->i2f(); break;
     case J9BCi2l: this->i2l(); break;
     case J9BCi2s: this->i2s(); break;
     case J9BCiadd: this->iadd(); break;
     case J9BCiaload: this->aaload(); break; //TODO own function
     case J9BCiand: this->iand(); break;
     case J9BCiastore: this->aastore(); break; //TODO own function
     case J9BCiconstm1: this->iconstm1(); break;
     case J9BCiconst0: this->iconst0(); break;
     case J9BCiconst1: this->iconst1(); break;
     case J9BCiconst2: this->iconst2(); break;
     case J9BCiconst3: this->iconst3(); break;
     case J9BCiconst4: this->iconst4(); break;
     case J9BCiconst5: this->iconst5(); break;
     case J9BCidiv: this->idiv(); break;
     case J9BCifacmpeq: this->ificmpeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break; //TODO own function
     case J9BCifacmpne: this->ificmpne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break; //TODO own function
     case J9BCificmpeq: this->ificmpeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmpge: this->ificmpge(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmpgt: this->ificmpgt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmple: this->ificmple(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmplt: this->ificmplt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmpne: this->ificmpne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifeq: this->ifeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifge: this->ifge(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifgt: this->ifgt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifle: this->ifle(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCiflt: this->iflt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifne: this->ifne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifnonnull: this->ifnonnull(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifnull: this->ifnull(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCiinc: this->iinc(bci.nextByte(), bci.nextByteSigned(2)); break;
     case J9BCiload: this->iload(bci.nextByte()); break;
     case J9BCiload0: this->iload0(); break;
     case J9BCiload1: this->iload1(); break;
     case J9BCiload2: this->iload2(); break;
     case J9BCiload3: this->iload3(); break;
     case J9BCimul: this->imul(); break;
     case J9BCineg: this->ineg(); break;
     case J9BCinstanceof: this->instanceof(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     // invokes...
     case J9BCinvokevirtual: this->invokevirtual(bci.currentByteCodeIndex(), bci.next2Bytes(), bci); break;
     case J9BCinvokespecial: this->invokespecial(bci.currentByteCodeIndex(), bci.next2Bytes(), bci); break;
     case J9BCinvokestatic: this->invokestatic(bci.currentByteCodeIndex(), bci.next2Bytes(), bci); break;
     case J9BCinvokeinterface: this->invokeinterface(bci.currentByteCodeIndex(), bci.next2Bytes(), bci); break;
     //case J9BCinvokespecial: this->invokeinterface(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCior: this->ior(); break;
     case J9BCirem: this->irem(); break;
     //case J9BCireturn: this->pop; break; //
     case J9BCgenericReturn: this->_stack.size() != 0 ? this->pop() : 0; break; //FIXME return semantics
     case J9BCishl: this->ishl(); break;
     case J9BCishr: this->ishr(); break;
     case J9BCistore: this->istore(bci.nextByte()); break;
     case J9BCistorew: this->istore(bci.next2Bytes()); break; //WARN: internal bytecode
     case J9BCistore0: this->istore0(); break;
     case J9BCistore1: this->istore1(); break;
     case J9BCistore2: this->istore2(); break;
     case J9BCistore3: this->istore3(); break;
     case J9BCisub: this->isub(); break;
     case J9BCiushr: this->iushr(); break;
     case J9BCixor: this->ixor(); break;
     // jsr
     // jsr_w
     case J9BCl2d: this->l2d(); break;
     case J9BCl2f: this->l2f(); break;
     case J9BCl2i: this->l2i(); break;
     case J9BCladd: this->ladd(); break;
     case J9BClaload: this->laload(); break;
     case J9BCland: this->land(); break;
     case J9BClastore: this->aastore(); break; //TODO own function
     case J9BClcmp: this->lcmp(); break;
     case J9BClconst0: this->lconst0(); break;
     case J9BClconst1: this->lconst1(); break;
     case J9BCldc: this->ldc(bci.nextByte(), bci); break;
     case J9BCldcw: this->ldc(bci.next2Bytes(), bci); break; //TODO own function?
     case J9BCldc2lw: this->ldc(bci.next2Bytes(), bci); break; //WARN: internal bytecode equivalent to ldc2_w
     case J9BCldc2dw: this->ldc(bci.next2Bytes(), bci); break; //WARN: internal bytecode equivalent to ldc2_w
     case J9BCldiv: this->ldiv(); break;
     case J9BClload: this->lload(bci.nextByte()); break;
     case J9BClload0: this->lload0(); break;
     case J9BClload1: this->lload1(); break;
     case J9BClload2: this->lload2(); break;
     case J9BClload3: this->lload3(); break;
     case J9BClmul: this->lmul(); break;
     case J9BClneg: this->lneg(); break;
     case J9BClookupswitch : this->pop(); break; // TODO
     case J9BClor: this->lor(); break;
     case J9BClrem: this->lrem(); break;
     //case J9BClreturn: this->pop(); this->pop(); break; //FIXME: return semantics
     case J9BClshl: this->lshl(); break;
     case J9BClshr: this->lshr(); break;
     case J9BClstore: this->dstore(bci.nextByte()); break; // TODO: own functions?
     case J9BClstorew: this->dstore(bci.next2Bytes()); break; // WARN: internal bytecode
     case J9BClstore0: this->dstore0(); break;
     case J9BClstore1: this->dstore1(); break;
     case J9BClstore2: this->dstore2(); break;
     case J9BClstore3: this->dstore3(); break;
     case J9BClsub: this->lsub(); break;
     case J9BClushr: this->lushr(); break;
     case J9BClxor: this->lxor(); break;
     case J9BCmonitorenter: this->monitorenter(); break;
     case J9BCmonitorexit: this->monitorexit(); break;
     case J9BCmultianewarray: this->multianewarray(bci.next2Bytes(), bci.nextByteSigned(3)); break;
     case J9BCnew: this->_new(bci.next2Bytes()); break;
     case J9BCnewarray: this->newarray(bci.nextByte()); break;
     case J9BCnop: /* yeah nothing */ break;
     case J9BCpop: this->pop(); break;
     case J9BCpop2: this->pop2(); break;
     case J9BCputfield: this->putfield(); break;
     case J9BCputstatic: this->putstatic(); break;
     // ret
     // return
     case J9BCsaload: this->aaload(); break;
     case J9BCsastore: this->aastore(); break;
     case J9BCsipush: this->sipush(bci.next2Bytes()); break;
     case J9BCswap: this->swap(); break;
     case J9BCReturnC: this->pop(); break;
     case J9BCReturnS: this->pop(); break;
     case J9BCReturnB: this->pop(); break;
     case J9BCReturnZ: /*yeah nothing */ break;
     case J9BCtableswitch: this->pop(); break;
     //TODO: clean me
     case J9BCwide:
       {
       //TODO: iincw
       bci.setIndex(bci.currentByteCodeIndex() + 1);
       TR_J9ByteCode bc2 = bci.current();
       switch (bc2)
          {
          case J9BCiload: this->iload(bci.next2Bytes()); break;
          case J9BClload: this->lload(bci.next2Bytes()); break;
          case J9BCfload: this->fload(bci.next2Bytes()); break;
          case J9BCdload: this->dload(bci.next2Bytes()); break;
          case J9BCaload: this->aload(bci.next2Bytes()); break;
          case J9BCistore: this->istore(bci.next2Bytes()); break; 
          case J9BClstore: this->dstore(bci.next2Bytes()); break;  // TODO: own function?
          case J9BCfstore: this->fstore(bci.next2Bytes()); break; 
          case J9BCdstore: this->dstore(bci.next2Bytes()); break; 
          case J9BCastore: this->astore(bci.next2Bytes()); break; 
          }
       }
     break;
     //case J9BCgenericReturn: /* yeah nothing */ break;
     default:
     break;
     }
  if (comp->trace(OMR::benefitInliner))
     {
     traceMsg(comp, "\n");
     this->trace();
     }
  }

//TODO, no strings here...
// We need to unnest IDT::Node
void
AbsEnvStaticBase::trace(const char* methodName)
  {
  if (methodName) traceMsg(TR::comp(), "method %s\n", methodName);
  this->_array.trace(this->_vp);
  this->_stack.trace(this->_vp, this->_region);
  }

void
AbsEnvStaticBase::multianewarray(int cpIndex, int dimensions)
{
  for (int i = 0; i < dimensions; i++)
    {
    this->pop();
    }
  this->pushNull();
}

void
AbsEnvStaticBase::aaload()
  {
  AbsValue *index = this->pop();
  AbsValue *arrayRef = this->pop();
  this->pushNull();
  }

void
AbsEnvStaticBase::laload()
  {
  AbsValue *index = this->pop();
  AbsValue *arrayRef = this->pop();
  AbsValue *value1 = this->getTopDataType(TR::Int64);
  AbsValue *value2 = this->getTopDataType(TR::NoType);
  push(value1);
  push(value2);
  }

void
AbsEnvStaticBase::daload()
  {
  AbsValue *index = this->pop();
  AbsValue *arrayRef = this->pop();
  AbsValue *value1 = this->getTopDataType(TR::Double);
  AbsValue *value2 = this->getTopDataType(TR::NoType);
  push(value1);
  push(value2);
  }

void
AbsEnvStaticBase::aastore()
  {
  //TODO:
  AbsValue *value = this->pop();
  AbsValue *index = this->pop();
  AbsValue *arrayRef = this->pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  }

void
AbsEnvStaticBase::aconstnull() {
  TR::VPConstraint *null = TR::VPNullObject::create(this->_vp);
  AbsValue *absValue = new (_region) AbsValue(null, TR::Address);
  this->push(absValue);
}

void
AbsEnvStaticBase::aload(int n) {
  this->aloadn(n);
}

void
AbsEnvStaticBase::aload0() {
  this->aloadn(0);
}

void
AbsEnvStaticBase::aload1() {
  this->aloadn(1);
}

void
AbsEnvStaticBase::aload2() {
  this->aloadn(2);
}

void
AbsEnvStaticBase::aload3() {
  this->aloadn(3);
}

void
AbsEnvStaticBase::aloadn(int n) {
  AbsValue *constraint = this->at(n);
  this->push(constraint);
}

void
AbsEnvStaticBase::astore(int varIndex) {
  AbsValue* constraint = this->pop();
  this->at(varIndex, constraint);
}

void
AbsEnvStaticBase::astore0() {
  this->astore(0);
}

void
AbsEnvStaticBase::astore1() {
  this->astore(1);
}

void
AbsEnvStaticBase::astore2() {
  this->astore(2);
}

void
AbsEnvStaticBase::astore3() {
  this->astore(3);
}

void
AbsEnvStaticBase::bipush(int byte) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->_vp, byte);
  //TODO: should I use TR::Int32 or something else?
  AbsValue *absValue = new (_region) AbsValue(data, TR::Int32);
  this->push(absValue);
}

void
AbsEnvStaticBase::bastore() {
  this->aastore();
}

void
AbsEnvStaticBase::baload() {
  this->aaload();
}

void
AbsEnvStaticBase::checkcast(int cpIndex, int bytecodeIndex) {
  AbsValue *absValue = this->pop();
  TR::VPConstraint *objectRef = absValue->_vp;
  if (!objectRef)
    {
    this->push(absValue);
    return;
    }

  // TODO: _rms
  TR_ResolvedMethod* method = this->_rms->getResolvedMethod();
  // TODO: can we do this some other way? I don't want to pass TR::comp();
  TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
  TR::VPConstraint *typeConstraint = NULL;
  // If objectRef is not assignable to a class of type type then throw an exception.

  this->push(absValue);
}

void
AbsEnvStaticBase::dup() {
  AbsValue *value = this->pop();
  this->push(value);
  this->push(value); 
}

void
AbsEnvStaticBase::dupx1() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  this->push(value1);
  this->push(value2);
  this->push(value1);
}

void
AbsEnvStaticBase::dupx2() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *value3 = this->pop();
  this->push(value1);
  this->push(value3);
  this->push(value2);
  this->push(value1);
}

void
AbsEnvStaticBase::dup2() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  this->push(value2);
  this->push(value1);
  this->push(value2);
  this->push(value1);
}

void
AbsEnvStaticBase::dup2x1() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *value3 = this->pop();
  this->push(value2);
  this->push(value1);
  this->push(value3);
  this->push(value2);
  this->push(value1);
}

void
AbsEnvStaticBase::dup2x2() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *value3 = this->pop();
  AbsValue *value4 = this->pop();
  this->push(value2);
  this->push(value1);
  this->push(value4);
  this->push(value3);
  this->push(value2);
  this->push(value1);
}

void
AbsEnvStaticBase::getstatic(int cpIndex, TR_J9ByteCodeIterator &bci) {
   void* staticAddress;
   TR::DataType type = TR::NoType;
   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   bool isResolved = bci.method()->staticAttributes(TR::comp(),
         cpIndex,
         &staticAddress,
         &type,
         &isVolatile,
         &isFinal,
         &isPrivate, 
         false, // isStore
         &isUnresolvedInVP,
         false); //needsAOTValidation

   AbsValue *value1 = this->getTopDataType(type);
   //TODO: if datatype is address... then we can probably find the class and get a tighter bound
   this->push(value1);
   if (!value1->isType2()) { return; }
   AbsValue *value2= this->getTopDataType(TR::NoType);
   this->push(value2);
}


void
AbsEnvStaticBase::getfield(int cpIndex, TR_J9ByteCodeIterator &bci) {
   AbsValue *objectref = this->pop();
   uint32_t fieldOffset;
   TR::DataType type = TR::NoType;
   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   bool isResolved = bci.method()->fieldAttributes(TR::comp(),
         cpIndex,
         &fieldOffset,
         &type,
         &isVolatile,
         &isFinal,
         &isPrivate, 
         false, // isStore
         &isUnresolvedInVP,
         false); //needsAOTValidation
   AbsValue *value1 = this->getTopDataType(type);
   //TODO: if datatype is address... then we can probably find the class and get a tighter bound
   this->push(value1);
   if (!value1->isType2()) { return; }
   AbsValue *value2= this->getTopDataType(TR::NoType);
   this->push(value2);
   
}

void
AbsEnvStaticBase::iand() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int result = value1->_vp->asIntConst()->getLow() & value2->_vp->asIntConst()->getLow();
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::instanceof(int cpIndex, int byteCodeIndex, TR_J9ByteCodeIterator &bci) {
  AbsValue *objectRef = this->pop();
  if (!objectRef->_vp)
     {
     this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, 0), TR::Int32));
     return;
     }

  TR_ResolvedMethod* method = bci.method();
  TR_OpaqueClassBlock *block = method->getClassFromConstantPool(TR::comp(), cpIndex);
  if (!block)
     {
     this->push(new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 0, 1), TR::Int32));
     return;
     }

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, block);
  TR::VPObjectLocation *objectLocation = TR::VPObjectLocation::create(this->_vp, TR::VPObjectLocation::J9ClassObject);
  TR::VPConstraint *typeConstraint= TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, objectLocation);
  /*
   * The following rules are used to determine whether an objectref that is
   * not null is an instance of the resolved type: 
   * If S is the class of the object referred to by objectref and 
   *    T is the resolved class, array, or interface type
   * instanceof determines whether objectref is an instance of T as follows:
   */

  /*
   * If S is an ordinary (nonarray) class, then:
   */
  if (objectRef->_vp->asClass())
     {
     /*
      * If T is a class type, then S must be the same class as T, or S
      * must be a subclass of T;
      */
     if(typeConstraint->intersect(objectRef->_vp, this->_vp))
        this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, 1), TR::Int32));
     else
        this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, 0), TR::Int32));
     return;
     }
  else if (false)
     {
     /* If T is an interface type, then S must implement interface T. */
     }

  /* If S is an interface type, then: */
    /* If T is a class type, then T must be Object. */
    /* If T is an interface type, then T must be the same interface as S or a superinterface of S. */
  /* If S is a class representing the array type SC[], that is, an array of components of type SC, then: */
    /* If T is a class type, then T must be Object */
    /* If T is an interface type, then T must be one of the interfaces implemented by arrays */
    /* If T is an array type TC[], that is, an array of components of type TC, then one of the following must be true:
       TC and SC are the same primitive type.
       TC and SC are reference types, and type SC can be cast to TC by these run-time rules. */

  this->push(new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 0, 1), TR::Int32));
}

void
AbsEnvStaticBase::ior() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int result = value1->_vp->asIntConst()->getLow() | value2->_vp->asIntConst()->getLow();
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::ixor() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int result = value1->_vp->asIntConst()->getLow() ^ value2->_vp->asIntConst()->getLow();
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::irem() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow();
  int result = int1 - (int1/ int2) * int2;
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::ishl() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  int result = int1 << int2;
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::ishr() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  //arithmetic shift.
  int result = int1 >> int2;
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::iushr() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  int result = int1 >> int2;
  //logical shift, gets rid of the sign.
  result &= 0x7FFFFFFF;
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::idiv() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow();
  if (int2 == 0)
    {
     // this should throw an exception.
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
    }
  int result = int1 / int2;
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::imul() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow();
  int result = int1 * int2;
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::ineg() {
  AbsValue *value1 = this->pop();
  bool allConstants = value1->_vp && value1->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  //TODO: more precision for ranges, subtract VPIntConst 0 from value1
  int int1 = value1->_vp->asIntConst()->getLow();
  int result = -int1;
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStaticBase::iconstm1() {
  this->iconst(-1);
}

void
AbsEnvStaticBase::iconst0() {
  this->iconst(0);
}

void
AbsEnvStaticBase::iconst1() {
  this->iconst(1);
}

void
AbsEnvStaticBase::iconst2() {
  this->iconst(2);
}

void
AbsEnvStaticBase::iconst3() {
  this->iconst(3);
}

void
AbsEnvStaticBase::iconst4() {
  this->iconst(4);
}

void
AbsEnvStaticBase::iconst5() {
  this->iconst(5);
}

void
AbsEnvStaticBase::iconst(int n) {
  this->pushConstInt(n);
}

void
AbsEnvStaticBase::ifeq(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStaticBase::ifne(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStaticBase::iflt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStaticBase::ifle(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStaticBase::ifgt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStaticBase::ifge(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStaticBase::ifnull(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStaticBase::ifnonnull(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStaticBase::ificmpge(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStaticBase::ificmpeq(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStaticBase::ificmpne(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStaticBase::ificmplt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStaticBase::ificmpgt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStaticBase::ificmple(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStaticBase::ifacmpeq(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStaticBase::ifacmpne(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStaticBase::iload(int n) {
  this->aloadn(n);
}

void
AbsEnvStaticBase::iload0() {
  this->iload(0);
}

void
AbsEnvStaticBase::iload1() {
  this->iload(1);
}

void
AbsEnvStaticBase::iload2() {
  this->iload(2);
}

void
AbsEnvStaticBase::iload3() {
  this->iload(3);
}

void
AbsEnvStaticBase::istore(int n) {
  this->at(n, this->pop());
}

void
AbsEnvStaticBase::istore0() {
  this->at(0, this->pop());
}

void
AbsEnvStaticBase::istore1() {
  this->at(1, this->pop());
}

void
AbsEnvStaticBase::istore2() {
  this->at(2, this->pop());
}

void
AbsEnvStaticBase::istore3() {
  this->at(3, this->pop());
}

void
AbsEnvStaticBase::isub() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
    {
    AbsValue *result = this->getTopDataType(TR::Int32);
    this->push(result);
    return;
    }

  TR::VPConstraint *result_vp = value1->_vp->subtract(value2->_vp, value2->_dt, this->_vp);
  AbsValue *result = new (_region) AbsValue(result_vp, value2->_dt);
  this->push(result);
}

void
AbsEnvStaticBase::iadd() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
    {
    AbsValue *result = this->getTopDataType(TR::Int32);
    this->push(result);
    return;
    }

  TR::VPConstraint *result_vp = value1->_vp->add(value2->_vp, value2->_dt, this->_vp);
  AbsValue *result = new (_region) AbsValue(result_vp, value2->_dt);
  this->push(result);
}

void
AbsEnvStaticBase::i2d() {
  AbsValue *value = this->pop();
  AbsValue *result = this->getTopDataType(TR::Double);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::i2f() {
  AbsValue *value = this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStaticBase::i2l() {
  AbsValue *value = this->pop();
  AbsValue *result = this->getTopDataType(TR::Int64);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::i2s() {
  //empty?
}

void
AbsEnvStaticBase::i2c() {
  //empty?
}

void
AbsEnvStaticBase::i2b() {
  // empty?
}

void
AbsEnvStaticBase::dadd() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *value3 = this->pop();
  AbsValue *value4 = this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStaticBase::dsub() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *value3 = this->pop();
  AbsValue *value4 = this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStaticBase::fsub() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStaticBase::fadd() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStaticBase::ladd() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *value3 = this->pop();
  AbsValue *value4 = this->pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }

  TR::VPConstraint *result_vp = value2->_vp->add(value4->_vp, value4->_dt, this->_vp);
  AbsValue *result = new (_region) AbsValue(result_vp, TR::Int64);
  this->push(result);
  this->getTopDataType(TR::NoType);
}

void
AbsEnvStaticBase::lsub() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *value3 = this->pop();
  AbsValue *value4 = this->pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }

  TR::VPConstraint *result_vp = value2->_vp->subtract(value4->_vp, value4->_dt, this->_vp);
  AbsValue *result = new (_region) AbsValue(result_vp, TR::Int64);
  this->push(result);
  this->getTopDataType(TR::NoType);
}

void
AbsEnvStaticBase::l2i() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  bool nonnull = value2->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int32);
    this->push(result1);
    return;
    }

  bool canCompute = value2->_vp && value2->_vp->asLongConstraint();
  if (!canCompute)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     }

  TR::VPConstraint *intConst = TR::VPIntRange::create(this->_vp, value2->_vp->asLongConstraint()->getLow(), value2->_vp->asLongConstraint()->getHigh());
  AbsValue *result = new (_region) AbsValue(intConst, TR::Int32);
  this->push(result);
}

void
AbsEnvStaticBase::land() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue *value3 = this->pop();
  AbsValue *value4 = this->pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
     }

  int result = value2->_vp->asLongConst()->getLow() & value4->_vp->asLongConst()->getLow();
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::ldiv() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
     }

  long int1 = value2->_vp->asLongConst()->getLow();
  long int2 = value4->_vp->asLongConst()->getLow();
  if (int2 == 0)
    {
     // this should throw an exception.
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
    }
  long result = int1 / int2;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::lmul() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
     }

  long int1 = value1->_vp->asLongConst()->getLow();
  long int2 = value2->_vp->asLongConst()->getLow();
  long result = int1 * int2;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::lneg() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  bool nonnull = value2->_vp;
  if (!nonnull)
    {
    this->push(value2);
    this->push(value1);
    return;
    }
  bool allConstants = value2->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
     }

  long int1 = value1->_vp->asLongConst()->getLow();
  long result = -int1;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::lor() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }
  bool allConstants = value2->_vp->asLongConst() && value2->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
     }

  long result = value1->_vp->asLongConst()->getLow() | value2->_vp->asLongConst()->getLow();
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::lrem() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
     }

  long int1 = value2->_vp->asLongConst()->getLow();
  long int2 = value4->_vp->asLongConst()->getLow();
  long result = int1 - (int1/ int2) * int2;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::lshl() {
  AbsValue *value2 = this->pop(); // int
  AbsValue* value0 = this->pop(); // nothing
  AbsValue* value1 = this->pop(); // long
  bool nonnull = value2->_vp && value1->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }
  bool allConstants = value2->_vp->asIntConst() && value1->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
     }

  long int1 = value1->_vp->asLongConst()->getLow();
  long int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  long result = int1 << int2;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::lshr() {
  AbsValue *value2 = this->pop(); // int
  AbsValue* value0 = this->pop(); // nothing
  AbsValue* value1 = this->pop(); // long
  bool nonnull = value2->_vp && value1->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }
  bool allConstants = value2->_vp->asIntConst() && value1->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
     }

  long int1 = value1->_vp->asLongConst()->getLow();
  long int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  long result = int1 >> int2;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::lushr() {
  this->lshr();
}

void
AbsEnvStaticBase::lxor() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    this->push(result1);
    this->push(result2);
    return;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     this->push(result1);
     this->push(result2);
     return;
     }

  long result = value1->_vp->asLongConst()->getLow() ^ value2->_vp->asLongConst()->getLow();
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
}

void
AbsEnvStaticBase::l2d() {
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Double);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::l2f() {
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStaticBase::d2f() {
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStaticBase::f2d() {
  this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStaticBase::f2i() {
  this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Int32);
  this->push(result1);
}

void
AbsEnvStaticBase::f2l() {
  this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Int64);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStaticBase::d2i() {
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  this->push(result);
}

void
AbsEnvStaticBase::d2l() {
  this->pop();
  this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Int64);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStaticBase::lload(int n) {
  aload(n);
  aload(n + 1);
}

void
AbsEnvStaticBase::lload0() {
  aload(0);
  aload(1);
}

void
AbsEnvStaticBase::lload1() {
  aload(1);
  aload(2);
}

void
AbsEnvStaticBase::lload2() {
  aload(2);
  aload(3);
}

void
AbsEnvStaticBase::lload3() {
  aload(3);
  aload(4);
}

void
AbsEnvStaticBase::dconst0() {
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStaticBase::fconst0() {
  AbsValue *result1 = this->getTopDataType(TR::Float);
  this->push(result1);
}

void
AbsEnvStaticBase::dload(int n) {
  aload(n);
  aload(n + 1);
}

void
AbsEnvStaticBase::dload0() {
  aload(0);
  aload(1);
}

void
AbsEnvStaticBase::dload1() {
  aload(1);
  aload(2);
}

void
AbsEnvStaticBase::dload2() {
  aload(2);
  aload(3);
}

void
AbsEnvStaticBase::dload3() {
  aload(3);
  aload(4);
}

void
AbsEnvStaticBase::dstore(int n) {
  AbsValue *top = this->pop();
  AbsValue *bottom = this->pop();
  this->at(n, bottom);
  this->at(n + 1, top);
}

void
AbsEnvStaticBase::dstore0() {
  this->dstore(0);
}

void
AbsEnvStaticBase::dstore1() {
  this->dstore(1);
}

void
AbsEnvStaticBase::dstore2() {
  this->dstore(2);
}

void
AbsEnvStaticBase::dstore3() {
  this->dstore(3);
}

void
AbsEnvStaticBase::lconst0() {
  AbsValue *result = this->getTopDataType(TR::Int64);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::lconst1() {
  AbsValue *result = this->getTopDataType(TR::Int64);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStaticBase::lcmp() {
  this->pop();
  this->pop();
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  this->push(result);
}

void
AbsEnvStaticBase::pop2() {
  this->pop();
  this->pop();
}

void
AbsEnvStaticBase::fload(int n) {
  aload(n);
}

void
AbsEnvStaticBase::fload0() {
  this->fload(0);
}

void
AbsEnvStaticBase::fload1() {
  this->fload(1);
}

void
AbsEnvStaticBase::fload2() {
  this->fload(2);
}

void
AbsEnvStaticBase::fload3() {
  this->fload(3);
}

void
AbsEnvStaticBase::swap() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  this->push(value1);
  this->push(value2);
}

void
AbsEnvStaticBase::fstore(int n) {
  this->at(n, this->pop());
}


void
AbsEnvStaticBase::fstore0() {
  this->fstore(0);
}

void
AbsEnvStaticBase::fstore1() {
  this->fstore(1);
}

void
AbsEnvStaticBase::fstore2() {
  this->fstore(2);
}

void
AbsEnvStaticBase::fstore3() {
  this->fstore(3);
}

void
AbsEnvStaticBase::fmul() {
  AbsValue *value1 = this->pop();
}

void
AbsEnvStaticBase::dmul() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
}

void
AbsEnvStaticBase::dcmpl() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue *value3 = this->pop();
  AbsValue* value4 = this->pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  this->push(result);
}


void
AbsEnvStaticBase::fcmpl() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  this->push(result);
}

void
AbsEnvStaticBase::ddiv() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
}

void
AbsEnvStaticBase::fdiv() {
  AbsValue *value1 = this->pop();
}

void
AbsEnvStaticBase::drem() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
}

void
AbsEnvStaticBase::frem() {
  AbsValue *value = this->pop();
}

void
AbsEnvStaticBase::sipush(int16_t _short) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->_vp, _short);
  AbsValue *result = new (_region) AbsValue(data, TR::Int16);
  this->push(result);
}

void
AbsEnvStaticBase::iinc(int index, int incval) {
  AbsValue *value1 = this->at(index);
  TR::VPIntConstraint *value = value1->_vp ? value1->_vp->asIntConstraint() : nullptr;
  if (!value)
    {
    return;
    }

  TR::VPIntConst *inc = TR::VPIntConst::create(this->_vp, incval);
  TR::VPConstraint *result = value->add(inc, TR::Int32, this->_vp);
  AbsValue *result2 = new (_region) AbsValue(result, TR::Int32);
  this->at(index, result2);
}

void
AbsEnvStaticBase::putfield() {
  // WONTFIX we do not model the heap
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  if(value2->isType2()) {
    AbsValue *value3 = this->pop();
  }
}

void
AbsEnvStaticBase::putstatic() {
  // WONTFIX we do not model the heap
  AbsValue *value1 = this->pop();
  if (!value1->_dt == TR::NoType) { // category type 2
    AbsValue *value2 = this->pop();
  }
}

void
AbsEnvStaticBase::ldcInt32(int cpIndex, TR_J9ByteCodeIterator &bci) {
  int32_t value =  bci.method()->intConstant(cpIndex); 
  TR::VPIntConst *constraint = TR::VPIntConst::create(this->_vp, value);
  AbsValue *result = new (_region) AbsValue(constraint, TR::Int32);
  this->push(result);
}

void
AbsEnvStaticBase::ldcInt64(int cpIndex, TR_J9ByteCodeIterator &bci) {
   auto value =  bci.method()->longConstant(cpIndex); 
   TR::VPLongConst *constraint = TR::VPLongConst::create(this->_vp, value);
   AbsValue *result = new (_region) AbsValue(constraint, TR::Int64);
   this->push(result);
   AbsValue *result2 = this->getTopDataType(TR::NoType);
   this->push(result2);
}

void
AbsEnvStaticBase::ldcFloat() {
   AbsValue *result = this->getTopDataType(TR::Float);
   this->push(result);
}

void
AbsEnvStaticBase::ldcDouble() {
   AbsValue *result = this->getTopDataType(TR::Double);
   AbsValue *result2 = this->getTopDataType(TR::NoType);
   this->push(result);
   this->push(result2);
}

void
AbsEnvStaticBase::ldcAddress(int cpIndex, TR_J9ByteCodeIterator &bci) {
  bool isString = bci.method()->isStringConstant(cpIndex);
  if (isString) { ldcString(cpIndex); return; }
  //TODO: non string case
  AbsValue *result = this->getTopDataType(TR::Address);
  this->push(result);
}

void
AbsEnvStaticBase::ldcString(int cpIndex) {
   // TODO: we might need the resolved method symbol here
   // TODO: avoid _rms    
   TR::SymbolReference *symRef = TR::comp()->getSymRefTab()->findOrCreateStringSymbol(_rms, cpIndex);
   if (symRef->isUnresolved())
        {
        AbsValue *result = this->getTopDataType(TR::Address);
        this->push(result);
        return;
        }
   TR::VPConstraint *constraint = TR::VPConstString::create(_vp, symRef);
   AbsValue *result = new (_region) AbsValue(constraint, TR::Address);
   this->push(result);
}

void
AbsEnvStaticBase::ldc(int cpIndex, TR_J9ByteCodeIterator &bci) {
   TR::DataType datatype = bci.method()->getLDCType(cpIndex);
   switch(datatype) {
     case TR::Int32: this->ldcInt32(cpIndex, bci); break;
     case TR::Int64: this->ldcInt64(cpIndex, bci); break;
     case TR::Float: this->ldcFloat(); break;
     case TR::Double: this->ldcDouble(); break;
     case TR::Address: this->ldcAddress(cpIndex, bci); break;
     default: {
       //TODO: arrays and what nots.
       AbsValue *result = this->getTopDataType(TR::Address);
      this->push(result);
      
     } break;
   }
}

void
AbsEnvStaticBase::monitorenter() {
  // TODO: possible optimization
  this->pop();
}

void
AbsEnvStaticBase::monitorexit() {
  // TODO: possible optimization
  this->pop();
}

void
AbsEnvStaticBase::anewarray(int cpIndex) {
  //TODO: actually make an array
  AbsValue *count = this->pop();
  AbsValue *result = this->getTopDataType(TR::Address);
  this->push(result);
}

void
AbsEnvStaticBase::arraylength() {
  //TODO: actually make use of the value
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  this->push(result);
}

void
AbsEnvStaticBase::_new(int cpIndex) {
  //TODO: actually look at the semantics
  AbsValue *result = this->getTopDataType(TR::Address);
  this->push(result);
}

void
AbsEnvStaticBase::newarray(int atype) {
  //TODO: actually impement the sematncis
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Address);
  this->push(result);
}

void
AbsEnvStaticBase::invokevirtual(int bcIndex, int cpIndex, TR_J9ByteCodeIterator &bci) {
  invoke(bcIndex, cpIndex, bci, TR::MethodSymbol::Kinds::Virtual);
}

void
AbsEnvStaticBase::invokestatic(int bcIndex, int cpIndex, TR_J9ByteCodeIterator &bci) {
  invoke(bcIndex, cpIndex, bci, TR::MethodSymbol::Kinds::Static);
}

void
AbsEnvStaticBase::invokespecial(int bcIndex, int cpIndex, TR_J9ByteCodeIterator &bci) {
  invoke(bcIndex, cpIndex, bci, TR::MethodSymbol::Kinds::Special);
}

void
AbsEnvStaticBase::invokedynamic(int bcIndex, int cpIndex, TR_J9ByteCodeIterator &bci) {
  invoke(bcIndex, cpIndex, bci, TR::MethodSymbol::Kinds::ComputedVirtual);
}

void
AbsEnvStaticBase::invokeinterface(int bcIndex, int cpIndex, TR_J9ByteCodeIterator &bci) {
  invoke(bcIndex, cpIndex, bci, TR::MethodSymbol::Kinds::Interface);
}

void
AbsEnvStaticBase::invoke(int bcIndex, int cpIndex, TR_J9ByteCodeIterator &bci, TR::MethodSymbol::Kinds kind) {
  auto callerResolvedMethodSymbol = this->_rms;
  auto callerResolvedMethod = callerResolvedMethodSymbol->getResolvedMethod();
  TR::Compilation *comp = TR::comp();
  TR::SymbolReference *symRef;
  switch(kind) {
    case TR::MethodSymbol::Kinds::Virtual:
      symRef = comp->getSymRefTab()->findOrCreateVirtualMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
    case TR::MethodSymbol::Kinds::Interface:
      symRef =comp->getSymRefTab()->findOrCreateInterfaceMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
    case TR::MethodSymbol::Kinds::Static:
      symRef = comp->getSymRefTab()->findOrCreateStaticMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
    case TR::MethodSymbol::Kinds::Special:
      symRef = comp->getSymRefTab()->findOrCreateSpecialMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
  }
  TR_Method *method = comp->fej9()->createMethod(comp->trMemory(), callerResolvedMethod->containingClass(), cpIndex);
  // how many pops?
  uint32_t params = method->numberOfExplicitParameters();
  int isStatic = kind == TR::MethodSymbol::Kinds::Static;
  int numberOfImplicitParameters = isStatic ? 0 : 1;
  if (numberOfImplicitParameters == 1) this->pop();
  for (int i = 0; i < params; i++) {
    TR::DataType datatype = method->parmType(i);
    this->pop();
    switch (datatype) {
      case TR::Double:
      case TR::Int64:
        this->pop();
        break;
      default:
        break;
    }
  }

  //pushes?
  if (method->returnTypeWidth() == 0) return;

  //how many pushes?
  TR::DataType datatype = method->returnType();
  switch(datatype) {
      case TR::Float:
      case TR::Int32:
      case TR::Int16:
      case TR::Int8:
      case TR::Address:
        {
        AbsValue *result = this->getTopDataType(datatype);
        this->push(result);
        }
        break;
      case TR::Double:
      case TR::Int64:
        {
        AbsValue *result = this->getTopDataType(datatype);
        this->push(result);
        AbsValue *result2 = this->getTopDataType(TR::NoType);
        this->push(result2);
        }
        break;
      default:
        {
        //TODO: 
        AbsValue *result = this->getTopDataType(TR::Address);
        this->push(result);
        }
        break;
  }
  
}

void
AbsEnvStaticBase::interpretBlock(OMR::Block * block)
{
  //TODO: avoid TR::comp();
  TR::Compilation *comp = TR::comp();
  TR::ResolvedMethodSymbol *resolvedMethodSymbol = this->_rms;
  this->enterMethod(resolvedMethodSymbol);
  this->trace(this->_node->getName());
  TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
  TR_ResolvedJ9Method *resolvedJ9Method = static_cast<TR_ResolvedJ9Method*>(resolvedMethod);
  TR_J9VMBase *vm = static_cast<TR_J9VMBase*>(comp->fe());
  TR_J9ByteCodeIterator bci(resolvedMethodSymbol, resolvedJ9Method, vm, comp);

  block->_absEnv = this;


  int start = block->getBlockBCIndex();
  int end = start + block->getBlockSize();
  if (start < 0 || end < 1) return;
  bci.setIndex(start);
  if (comp->trace(OMR::benefitInliner))
     {
     traceMsg(comp, "basic block %d start = %d end = %d\n", block->getNumber(), start, end);
     }
  for (TR_J9ByteCode bc = bci.current(); bc != J9BCunknown && bci.currentByteCodeIndex() < end; bc = bci.next())
     {
     if (block->_absEnv)
     block->_absEnv->interpretByteCode(bc, bci);
     }
}

/*
AbsEnvStaticBase *
AbsEnvStaticBase::mergeAllPredecessors(TR::Region &region, OMR::Block *block) {
  TR::CFGEdgeList &predecessors = block->getPredecessors();
  AbsEnvStaticBase *absEnv = nullptr;
  bool first = true;
  for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i)
  {
     auto *edge = *i;
     TR::Block *aBlock = edge->getFrom()->asBlock();
     TR::Block *check = edge->getTo()->asBlock();
     if (check != block) {
        continue;
     }
     //TODO: can aBlock->_absEnv be null?
     TR_ASSERT(aBlock->_absEnv, "absEnv is null");
     if (first) {
        first = false;
        // copy the first one
        absEnv = new (_region) AbsEnvStaticBase(*aBlock->_absEnv);
        continue;
     }
     // merge with the rest;
     absEnv->merge(*aBlock->_absEnv);
  }
  return absEnv;
}
*/

// TODO
AbsEnvStaticBase::CompareResult AbsEnvStaticBase::compareWith(AbsEnvStaticBase *other)
  {
  return CompareResult::Narrower;
  }

bool
AbsEnvStaticBase::isNarrowerThan(AbsEnvStaticBase *other)
  {
  return compareWith(other) == CompareResult::Narrower;
  }