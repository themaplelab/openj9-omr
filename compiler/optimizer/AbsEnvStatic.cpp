#include "compiler/optimizer/AbsEnvStatic.hpp"

#include "compile/SymbolReferenceTable.hpp"
#include "compile/J9SymbolReferenceTable.hpp"
#include "compiler/env/j9method.h"
#include "compiler/il/SymbolReference.hpp"
#include "compiler/ilgen/J9ByteCodeIterator.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"

AbsEnvStatic::AbsEnvStatic(TR::Region &region, UDATA maxStack, IDATA maxLocals, TR::ValuePropagation *vp, TR::ResolvedMethodSymbol *rms) :
  _region(region),
  _array(region, maxLocals),
  _stack(region, maxStack),
  _vp(vp),
  _rms(rms)
{
}

AbsValue*
AbsEnvStatic::pop()
  {
  AbsValue *absValue = this->_stack.top();
  this->_stack.pop();
  return absValue;
  }

void
AbsEnvStatic::push(AbsValue *absValue)
  {
  this->_stack.push(absValue);
  }

void
AbsEnvStatic::pushNull()
  {
  this->_stack.push(new (_region) AbsValue(NULL, TR::Address));
  }

void
AbsEnvStatic::pushConstInt(int n)
  {
  TR::VPIntConst *intConst = TR::VPIntConst::create(this->_vp, n);
  this->_stack.push(new (_region) AbsValue(intConst, TR::Int32));
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
AbsEnvStatic::getTopDataType(TR::DataType dt)
  {
  // TODO: Don't allocate new memory and always return the same set of values
  return new (_region) AbsValue(NULL, dt);
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

AbsValue*
AbsEnvStatic::at(unsigned int index)
  {
  return this->_array.at(index);
  }

void
AbsEnvStatic::interpret(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci)
  {
  TR::Compilation *comp = TR::comp();
  if (comp->trace(OMR::benefitInliner))
     bci.printByteCode();

  switch(bc)
     {
     case J9BCmultianewarray: this->multianewarray(bci.next2Bytes(), bci.nextByteSigned(3)); break;
     case J9BCaaload: this->aaload(); break;
     case J9BCaastore: this->aastore(); break;
     case J9BCaconstnull: this->aconstnull(); break;
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
     case J9BCcaload: case J9BCfaload: case J9BCdaload: case J9BCiaload: case J9BClaload: case J9BCsaload: this->aaload(); break;
     case J9BCcheckcast: this->checkcast(bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCd2f: this->d2f(); break;
     case J9BCd2i: this->d2i(); break;
     case J9BCd2l: this->d2l(); break;
     case J9BCdadd: this->dadd(); break;
     case J9BCdsub: this->dsub(); break;
     case J9BCdup: this->dup(); break;
     case J9BCdupx1: this->dupx1(); break;
     case J9BCdupx2: this->dupx2(); break;
     case J9BCdup2: this->dup2(); break;
     case J9BCdup2x1: this->dup2x1(); break;
     case J9BCdconst0: case J9BCdconst1: this->dconst0(); break;
     case J9BCfconst0: case J9BCfconst1: this->fconst0(); break;
     case J9BCf2l: this->f2l(); break;
     case J9BCf2i: this->f2i(); break;
     case J9BCf2d: this->f2d(); break;
     case J9BCfsub: this->fsub(); break;
     case J9BCfadd: this->fadd(); break;
     case J9BCgetstatic: this->getstatic(bci.next2Bytes(), bci); break;
     case J9BCgetfield: this->getfield(bci.next2Bytes(), bci); break;
     case J9BCiand: this->iand(); break;
     case J9BCinstanceof: this->instanceof(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCior: this->ior(); break;
     case J9BCixor: this->ixor(); break;
     case J9BCirem: this->irem(); break;
     case J9BCishl: this->ishl(); break;
     case J9BCishr: this->ishr(); break;
     case J9BCiushr: this->iushr(); break;
     case J9BCidiv: this->idiv(); break;
     case J9BCimul: this->imul(); break;
     case J9BCineg: this->ineg(); break;
     case J9BCiconst0: this->iconst0(); break;
     case J9BCiconst1: this->iconst1(); break;
     case J9BCiconst2: this->iconst2(); break;
     case J9BCiconst3: this->iconst3(); break;
     case J9BCiconst4: this->iconst4(); break;
     case J9BCiconst5: this->iconst5(); break;
     case J9BCifeq: this->ifeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifne: this->ifne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCiflt: this->iflt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifle: this->ifle(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifgt: this->ifgt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifge: this->ifge(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifnull: this->ifnull(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifnonnull: this->ifnonnull(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmpge: this->ificmpge(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmpeq: this->ificmpeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmpne: this->ificmpne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmplt: this->ificmplt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmpgt: this->ificmpgt(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCificmple: this->ificmple(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifacmpeq: this->ificmpeq(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCifacmpne: this->ificmpne(bci.next2Bytes(), bci.currentByteCodeIndex(), bci); break;
     case J9BCiload: this->iload(bci.nextByte()); break;
     case J9BCiload0: this->iload0(); break;
     case J9BCiload1: this->iload1(); break;
     case J9BCiload2: this->iload2(); break;
     case J9BCiload3: this->iload3(); break;
     case J9BCistore: this->istore(bci.nextByte()); break;
     case J9BCistore0: this->istore0(); break;
     case J9BCistore1: this->istore1(); break;
     case J9BCistore2: this->istore2(); break;
     case J9BCistore3: this->istore3(); break;
     case J9BCisub: this->isub(); break;
     case J9BCiadd: this->iadd(); break;
     case J9BCi2d: this->i2d(); break;
     case J9BCi2f: this->i2f(); break;
     case J9BCi2l: this->i2l(); break;
     case J9BCi2s: this->i2s(); break;
     case J9BCi2b: this->i2b(); break;
     case J9BCi2c: this->i2c(); break;
     case J9BCladd: this->ladd(); break;
     case J9BClsub: this->lsub(); break;
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
     case J9BCl2d: this->l2d(); break;
     case J9BCl2f: this->l2f(); break;
     case J9BClload: this->lload(bci.nextByte()); break;
     case J9BClload0: this->lload0(); break;
     case J9BClload1: this->lload1(); break;
     case J9BClload2: this->lload2(); break;
     case J9BClload3: this->lload3(); break;
     case J9BCdload: this->dload(bci.nextByte()); break;
     case J9BCdload0: this->dload0(); break;
     case J9BCdload1: this->dload1(); break;
     case J9BCdload2: this->dload2(); break;
     case J9BCdload3: this->dload3(); break;
     case J9BCdstore: this->dstore(bci.nextByte()); break;
     case J9BCdstore0: this->dstore0(); break;
     case J9BCdstore1: this->dstore1(); break;
     case J9BCdstore2: this->dstore2(); break;
     case J9BCdstore3: this->dstore3(); break;
     case J9BCfstore0: this->fstore0(); break;
     case J9BCfstore1: this->fstore1(); break;
     case J9BCfstore2: this->fstore2(); break;
     case J9BCfstore3: this->fstore3(); break;
     case J9BClconst0: this->lconst0(); break;
     case J9BClconst1: this->lconst1(); break;
     case J9BClstore: this->dstore(bci.nextByte()); break;
     case J9BClstore0: this->dstore0(); break;
     case J9BClstore1: this->dstore1(); break;
     case J9BClstore2: this->dstore2(); break;
     case J9BClstore3: this->dstore3(); break;
     case J9BClcmp: this->lcmp(); break;
     case J9BCpop: this->pop(); break;
     case J9BCpop2: this->pop2(); break;
     case J9BCfload: this->fload(bci.nextByte()); break;
     case J9BCfload0: this->fload0(); break;
     case J9BCfload1: this->fload1(); break;
     case J9BCfload2: this->fload2(); break;
     case J9BCfload3: this->fload3(); break;
     case J9BCswap: this->swap(); break;
     case J9BCfstore: this->fstore(bci.nextByte()); break;
     case J9BCdmul: case J9BCfmul: this->dmul(); break;
     case J9BCdcmpl: case J9BCdcmpg: this->dcmpl(); break;
     case J9BCfcmpl: case J9BCfcmpg: this->fcmpl(); break;
     case J9BCdneg: case J9BCfneg: /* yeah nothing */ break;
     case J9BCddiv: this->ddiv(); break;
     case J9BCfdiv: this->fdiv(); break;
     case J9BCgoto: bci.next2Bytes(); break;
     case J9BCdrem: this->drem(); break;
     case J9BCfrem: this->frem(); break;
     case J9BCsipush: this->sipush(bci.next2Bytes()); break;
     case J9BCgenericReturn: /* yeah nothing */ break;
     case J9BCiinc: this->iinc(bci.nextByte(), bci.nextByteSigned(2)); break;
/*
     case J9BCinvokevirtual: this->invokevirtual(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokespecial: this->invokespecial(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokestatic: this->invokestatic(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokeinterface: this->invokeinterface(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCldc: this->ldc(bci.nextByte()); break; 
     case J9BCnew: this->_new(bci.next2Bytes()); break;
*/
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
AbsEnvStatic::trace(const char* methodName)
  {
  if (methodName) traceMsg(TR::comp(), "method %s\n", methodName);
  this->_array.trace(this->_vp);
  this->_stack.trace(this->_vp, this->_region);
  }

void
AbsEnvStatic::multianewarray(int cpIndex, int dimensions)
{
  for (int i = 0; i < dimensions; i++)
    {
    this->pop();
    }
  this->pushNull();
}

void
AbsEnvStatic::aaload()
  {
  AbsValue *index = this->pop();
  AbsValue *arrayRef = this->pop();
  this->pushNull();
  }

void
AbsEnvStatic::aastore()
  {
  //TODO:
  AbsValue *value = this->pop();
  AbsValue *index = this->pop();
  AbsValue *arrayRef = this->pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  }

void
AbsEnvStatic::aconstnull() {
  TR::VPConstraint *null = TR::VPNullObject::create(this->_vp);
  AbsValue *absValue = new (_region) AbsValue(null, TR::Address);
  this->push(absValue);
}

void
AbsEnvStatic::aload(int n) {
  this->aloadn(n);
}

void
AbsEnvStatic::aload0() {
  this->aloadn(0);
}

void
AbsEnvStatic::aload1() {
  this->aloadn(1);
}

void
AbsEnvStatic::aload2() {
  this->aloadn(2);
}

void
AbsEnvStatic::aload3() {
  this->aloadn(3);
}

void
AbsEnvStatic::aloadn(int n) {
  AbsValue *constraint = this->at(n);
  this->push(constraint);
}

void
AbsEnvStatic::astore(int varIndex) {
  AbsValue* constraint = this->pop();
  this->at(varIndex, constraint);
}

void
AbsEnvStatic::astore0() {
  this->astore(0);
}

void
AbsEnvStatic::astore1() {
  this->astore(1);
}

void
AbsEnvStatic::astore2() {
  this->astore(2);
}

void
AbsEnvStatic::astore3() {
  this->astore(3);
}

void
AbsEnvStatic::bipush(int byte) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->_vp, byte);
  //TODO: should I use TR::Int32 or something else?
  AbsValue *absValue = new (_region) AbsValue(data, TR::Int32);
  this->push(absValue);
}

void
AbsEnvStatic::bastore() {
  this->aastore();
}

void
AbsEnvStatic::baload() {
  this->aaload();
}

void
AbsEnvStatic::checkcast(int cpIndex, int bytecodeIndex) {
  AbsValue *absValue = this->pop();
  TR::VPConstraint *objectRef = absValue->_vp;
  if (!objectRef)
    {
    this->push(absValue);
    return;
    }

  TR_ResolvedMethod* method = this->_rms->getResolvedMethod();
  // TODO: can we do this some other way? I don't want to pass TR::comp();
  TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
  TR::VPConstraint *typeConstraint = NULL;
  // If objectRef is not assignable to a class of type type then throw an exception.

  this->push(absValue);
}

void
AbsEnvStatic::dup() {
  AbsValue *value = this->pop();
  this->push(value);
  this->push(value); 
}

void
AbsEnvStatic::dupx1() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  this->push(value1);
  this->push(value2);
  this->push(value1);
}

void
AbsEnvStatic::dupx2() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *value3 = this->pop();
  this->push(value1);
  this->push(value3);
  this->push(value2);
  this->push(value1);
}

void
AbsEnvStatic::dup2() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  this->push(value2);
  this->push(value1);
  this->push(value2);
  this->push(value1);
}

void
AbsEnvStatic::dup2x1() {
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
AbsEnvStatic::dup2x2() {
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
AbsEnvStatic::getstatic(int cpIndex, TR_J9ByteCodeIterator &bci) {
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
AbsEnvStatic::getfield(int cpIndex, TR_J9ByteCodeIterator &bci) {
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
AbsEnvStatic::iand() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
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
AbsEnvStatic::instanceof(int cpIndex, int byteCodeIndex, TR_J9ByteCodeIterator &bci) {
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
AbsEnvStatic::ior() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
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
AbsEnvStatic::ixor() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
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
AbsEnvStatic::irem() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
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
AbsEnvStatic::ishl() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
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
AbsEnvStatic::ishr() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
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
AbsEnvStatic::iushr() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
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
AbsEnvStatic::idiv() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
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
AbsEnvStatic::imul() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
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
AbsEnvStatic::ineg() {
  AbsValue *value1 = this->pop();
  bool allConstants = value1->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     this->push(result);
     return;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int result = -int1;
  this->push(new (_region) AbsValue(TR::VPIntConst::create(this->_vp, result), TR::Int32));
}

void
AbsEnvStatic::iconst0() {
  this->iconst(0);
}

void
AbsEnvStatic::iconst1() {
  this->iconst(1);
}

void
AbsEnvStatic::iconst2() {
  this->iconst(2);
}

void
AbsEnvStatic::iconst3() {
  this->iconst(3);
}

void
AbsEnvStatic::iconst4() {
  this->iconst(4);
}

void
AbsEnvStatic::iconst5() {
  this->iconst(5);
}

void
AbsEnvStatic::iconst(int n) {
  this->pushConstInt(n);
}

void
AbsEnvStatic::ifeq(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStatic::ifne(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStatic::iflt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStatic::ifle(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStatic::ifgt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStatic::ifge(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStatic::ifnull(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStatic::ifnonnull(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
}

void
AbsEnvStatic::ificmpge(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStatic::ificmpeq(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStatic::ificmpne(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStatic::ificmplt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStatic::ificmpgt(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStatic::ificmple(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStatic::ifacmpeq(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStatic::ifacmpne(int branchOffset, int bytecodeIndex, TR_J9ByteCodeIterator &bci) {
  this->pop();
  this->pop();
}

void
AbsEnvStatic::iload(int n) {
  this->aloadn(n);
}

void
AbsEnvStatic::iload0() {
  this->iload(0);
}

void
AbsEnvStatic::iload1() {
  this->iload(1);
}

void
AbsEnvStatic::iload2() {
  this->iload(2);
}

void
AbsEnvStatic::iload3() {
  this->iload(3);
}

void
AbsEnvStatic::istore(int n) {
  this->at(n, this->pop());
}

void
AbsEnvStatic::istore0() {
  this->at(0, this->pop());
}

void
AbsEnvStatic::istore1() {
  this->at(1, this->pop());
}

void
AbsEnvStatic::istore2() {
  this->at(2, this->pop());
}

void
AbsEnvStatic::istore3() {
  this->at(3, this->pop());
}

void
AbsEnvStatic::isub() {
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
AbsEnvStatic::iadd() {
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
AbsEnvStatic::i2d() {
  AbsValue *value = this->pop();
  AbsValue *result = this->getTopDataType(TR::Double);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStatic::i2f() {
  AbsValue *value = this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStatic::i2l() {
  AbsValue *value = this->pop();
  AbsValue *result = this->getTopDataType(TR::Int64);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStatic::i2s() {
  //empty?
}

void
AbsEnvStatic::i2c() {
  //empty?
}

void
AbsEnvStatic::i2b() {
  // empty?
}

void
AbsEnvStatic::dadd() {
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
AbsEnvStatic::dsub() {
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
AbsEnvStatic::fsub() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStatic::fadd() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStatic::ladd() {
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
AbsEnvStatic::lsub() {
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
AbsEnvStatic::l2i() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
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
AbsEnvStatic::land() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue *value3 = this->pop();
  AbsValue *value4 = this->pop();
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
AbsEnvStatic::ldiv() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
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
AbsEnvStatic::lmul() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
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
AbsEnvStatic::lneg() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
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
AbsEnvStatic::lor() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
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
AbsEnvStatic::lrem() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
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
  long result = int1 - (int1/ int2) * int2;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStatic::lshl() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
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
  long int2 = value2->_vp->asLongConst()->getLow() & 0x1f;
  long result = int1 << int2;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStatic::lshr() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
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
  long int2 = value2->_vp->asLongConst()->getLow() & 0x1f;
  long result = int1 >> int2;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStatic::lushr() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
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
  long int2 = value2->_vp->asLongConst()->getLow() & 0x1f;
  if (int2 == 0)
     {
     this->push(value2);
     this->push(value1);
     return;
     }
  long result = int1 >> int2;
  result &= 0x7FFFFFFFFFFFFFFF;
  this->push(new (_region) AbsValue(TR::VPLongConst::create(this->_vp, result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStatic::lxor() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue* value3 = this->pop();
  AbsValue* value4 = this->pop();
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
AbsEnvStatic::l2d() {
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Double);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStatic::l2f() {
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStatic::d2f() {
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  this->push(result);
}

void
AbsEnvStatic::f2d() {
  this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStatic::f2i() {
  this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Int32);
  this->push(result1);
}

void
AbsEnvStatic::f2l() {
  this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Int64);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStatic::d2i() {
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  this->push(result);
}

void
AbsEnvStatic::d2l() {
  this->pop();
  this->pop();
  AbsValue *result1 = this->getTopDataType(TR::Int64);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStatic::lload(int n) {
  aload(n);
  aload(n + 1);
}

void
AbsEnvStatic::lload0() {
  aload(0);
  aload(1);
}

void
AbsEnvStatic::lload1() {
  aload(1);
  aload(2);
}

void
AbsEnvStatic::lload2() {
  aload(2);
  aload(3);
}

void
AbsEnvStatic::lload3() {
  aload(3);
  aload(4);
}

void
AbsEnvStatic::dconst0() {
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result1);
  this->push(result2);
}

void
AbsEnvStatic::fconst0() {
  AbsValue *result1 = this->getTopDataType(TR::Float);
  this->push(result1);
}

void
AbsEnvStatic::dload(int n) {
  aload(n);
  aload(n + 1);
}

void
AbsEnvStatic::dload0() {
  aload(0);
  aload(1);
}

void
AbsEnvStatic::dload1() {
  aload(1);
  aload(2);
}

void
AbsEnvStatic::dload2() {
  aload(2);
  aload(3);
}

void
AbsEnvStatic::dload3() {
  aload(3);
  aload(4);
}

void
AbsEnvStatic::dstore(int n) {
  AbsValue *top = this->pop();
  AbsValue *bottom = this->pop();
  this->at(n, bottom);
  this->at(n + 1, top);
}

void
AbsEnvStatic::dstore0() {
  this->dstore(0);
}

void
AbsEnvStatic::dstore1() {
  this->dstore(1);
}

void
AbsEnvStatic::dstore2() {
  this->dstore(2);
}

void
AbsEnvStatic::dstore3() {
  this->dstore(3);
}

void
AbsEnvStatic::lconst0() {
  AbsValue *result = this->getTopDataType(TR::Int64);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStatic::lconst1() {
  AbsValue *result = this->getTopDataType(TR::Int64);
  this->push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  this->push(result2);
}

void
AbsEnvStatic::lcmp() {
  this->pop();
  this->pop();
  this->pop();
  this->pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  this->push(result);
}

void
AbsEnvStatic::pop2() {
  this->pop();
  this->pop();
}

void
AbsEnvStatic::fload(int n) {
  aload(n);
}

void
AbsEnvStatic::fload0() {
  this->fload(0);
}

void
AbsEnvStatic::fload1() {
  this->fload(1);
}

void
AbsEnvStatic::fload2() {
  this->fload(2);
}

void
AbsEnvStatic::fload3() {
  this->fload(3);
}

void
AbsEnvStatic::swap() {
  AbsValue *value1 = this->pop();
  AbsValue *value2 = this->pop();
  this->push(value1);
  this->push(value2);
}

void
AbsEnvStatic::fstore(int n) {
  this->at(n, this->pop());
}


void
AbsEnvStatic::fstore0() {
  this->fstore(0);
}

void
AbsEnvStatic::fstore1() {
  this->fstore(1);
}

void
AbsEnvStatic::fstore2() {
  this->fstore(2);
}

void
AbsEnvStatic::fstore3() {
  this->fstore(3);
}

void
AbsEnvStatic::dmul() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
}

void
AbsEnvStatic::dcmpl() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue *value3 = this->pop();
  AbsValue* value4 = this->pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  this->push(result);
}


void
AbsEnvStatic::fcmpl() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  this->push(result);
}

void
AbsEnvStatic::ddiv() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
}

void
AbsEnvStatic::fdiv() {
  AbsValue *value1 = this->pop();
}

void
AbsEnvStatic::drem() {
  AbsValue *value1 = this->pop();
  AbsValue* value2 = this->pop();
}

void
AbsEnvStatic::frem() {
  AbsValue *value = this->pop();
}

void
AbsEnvStatic::sipush(int16_t _short) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->_vp, _short);
  AbsValue *result = new (_region) AbsValue(data, TR::Int16);
  this->push(result);
}

void
AbsEnvStatic::iinc(int index, int incval) {
  AbsValue *value1 = this->at(index);
  TR::VPIntConstraint *value = value1->_vp->asIntConstraint();
  if (!value)
    {
    return;
    }

  TR::VPIntConst *inc = TR::VPIntConst::create(this->_vp, incval);
  TR::VPConstraint *result = value->add(inc, TR::Int32, this->_vp);
  AbsValue *result2 = new (_region) AbsValue(result, TR::Int32);
  this->at(index, result2);
}
