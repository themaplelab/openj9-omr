#include "compiler/optimizer/AbsEnvStatic.hpp"

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

AbstractState::AbstractState(TR::Region &region, IDT::Node *node) :
  _region(region),
  _array(region, node->maxLocals()),
  _stack(region, node->maxStack())
{
}

AbsValue*
AbstractState::at(unsigned int n)
{
  return this->_array.at(n);
}

void
AbstractState::at(unsigned int n, AbsValue* absValue)
{
  this->_array.at(n, absValue);
}

AbstractState::AbstractState(const AbstractState& other) :
  _region(other._region),
  _array(other._array, other._region),
  _stack(other._stack, other._region)
{
}

size_t
AbstractState::getStackSize() const
{
  return this->_stack.size();
}


TR::Region&
AbsEnvStatic::getRegion() const
{
  return this->_absFrame->getRegion();
}

TR::ResolvedMethodSymbol*
AbsEnvStatic::getResolvedMethodSymbol() const
{
  return this->_absFrame->getResolvedMethodSymbol();
}

TR::ResolvedMethodSymbol*
AbsFrame::getResolvedMethodSymbol() const
{
  return this->_rms;
}

TR::ValuePropagation *
AbsEnvStatic::getVP() const
{
  return this->_absFrame->_vp;
}

IDT::Node*
AbsEnvStatic::getNode() const
{
   return this->_absFrame->_node;
}

AbsEnvStatic::AbsEnvStatic(TR::Region &region, IDT::Node *node, AbsFrame* absFrame) :
  
  _absState(region, node),
  _absFrame(absFrame),
  _block(nullptr)
{
}

AbsEnvStatic::AbsEnvStatic(AbsEnvStatic &other) :
  _absState(other._absState),
  _absFrame(other._absFrame),
  _block(other._block)
{
}

void
AbstractState::visit(AbstractStateVisitor *visitor)
  {
  visitor->visitState(this);
  _array.visit(visitor);
  _stack.visit(visitor);
  }

void
AbstractState::visit(AbstractStateVisitor *visitor, AbstractState *other)
  {
  visitor->visitState(this, other);
  _array.visit(visitor, &other->_array);
  _stack.visit(visitor, &other->_stack);  
  }

void
AbstractState::merge(AbstractState &other, TR::ValuePropagation *vp)
{
   TR::Region &region = TR::comp()->trMemory()->currentStackRegion();
   AbstractState copyOfOther(other);
   this->_array.merge(copyOfOther._array, this->getRegion(), vp);
   this->_stack.merge(copyOfOther._stack, this->getRegion(), vp);
   this->trace(vp);
}

void
AbsEnvStatic::merge(AbsEnvStatic &other)
{
   getState().merge(other._absState, this->getVP());
}

AbsValue*
AbstractState::pop()
  {
  AbsValue *absValue = this->_stack.top();
  this->_stack.pop();
  return absValue;
  }

void
AbstractState::push(AbsValue *absValue)
  {
  this->_stack.push(absValue);
  }

void
AbsEnvStatic::pushNull()
  {
  this->getState().push(createAbsValue(NULL, TR::Address));
  }

void
AbsEnvStatic::pushConstInt(AbstractState& absState, int n)
  {
  TR::VPIntConst *intConst = TR::VPIntConst::create(this->getVP(), n);
  absState.push(createAbsValue(intConst, TR::Int32));
  }

AbsValue*
AbsEnvStatic::getTopDataType(TR::DataType dt)
  {
  // TODO: Don't allocate new memory and always return the same set of values
  return createAbsValue(NULL, dt);
  }


AbstractState&
AbsEnvStatic::aload0getfield(AbstractState &absState, int i) {
  this->aload0(absState);
  return absState;
}

void
AbsEnvStatic::interpret(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci)
  {
  TR::Compilation *comp = TR::comp();
  if (comp->trace(OMR::benefitInliner)) {
     bci.printByteCode();
     traceMsg(comp, "has size of = %d\n", bci.size(bc));
  }

  switch(bc)
     {
     //alphabetical order
     case J9BCaaload: this->aaload(this->_absState); break;
     case J9BCaastore: this->aastore(this->_absState); break;
     case J9BCaconstnull: this->aconstnull(this->_absState); break;
     case J9BCaload: this->aload(this->_absState, bci.nextByte()); break;
     case J9BCaload0: this->aload0(this->_absState); break;
     case /* aload0getfield */ 215: this->aload0getfield(this->_absState, 0); break;
     case J9BCaload1: this->aload1(this->_absState); break;
     case J9BCaload2: this->aload2(this->_absState); break;
     case J9BCaload3: this->aload3(this->_absState); break;
     case J9BCanewarray: this->anewarray(this->_absState, bci.next2Bytes()); break;
     //case J9BCareturn: 
     case J9BCarraylength: this->arraylength(this->_absState); break;
     case J9BCastore: this->astore(this->_absState, bci.nextByte()); break;
     case J9BCastore0: this->astore0(this->_absState); break;
     case J9BCastore1: this->astore1(this->_absState); break;
     case J9BCastore2: this->astore2(this->_absState); break;
     case J9BCastore3: this->astore3(this->_absState); break;
     case J9BCastorew: this->astore(this->_absState, bci.next2Bytes()); break; // WARN: internal bytecode
     case J9BCathrow: this->athrow(this->_absState); break;
     case J9BCbaload: this->baload(this->_absState); break;
     case J9BCbastore: this->bastore(this->_absState); break;
     case J9BCbipush: this->bipush(this->_absState, bci.nextByte()); break;
     case J9BCcaload : this->caload(this->_absState); break;
     case J9BCcastore: this->castore(this->_absState); break;
     case J9BCcheckcast: this->checkcast(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCd2f: this->d2f(this->_absState); break;
     case J9BCd2i: this->d2i(this->_absState); break;
     case J9BCd2l: this->d2l(this->_absState); break;
     case J9BCdadd: this->dadd(this->_absState); break;
     case J9BCdaload: this->daload(this->_absState); break;
     case J9BCdastore: this->dastore(this->_absState); break;
     case J9BCdcmpl: this->dcmpl(this->_absState); break;
     case J9BCdcmpg: this->dcmpg(this->_absState); break;
     case J9BCdconst0: this->dconst0(this->_absState); break;
     case J9BCdconst1: this->dconst1(this->_absState); break;
     case J9BCddiv: this->ddiv(this->_absState); break;
     case J9BCdload: this->dload(this->_absState, bci.nextByte()); break;
     case J9BCdload0: this->dload0(this->_absState); break;
     case J9BCdload1: this->dload1(this->_absState); break;
     case J9BCdload2: this->dload2(this->_absState); break;
     case J9BCdload3: this->dload3(this->_absState); break;
     case J9BCdmul: this->dmul(this->_absState); break;
     case J9BCdneg: this->dneg(this->_absState); break;
     case J9BCdrem: this->drem(this->_absState); break;
     //case J9BCdreturn
     case J9BCdstore: this->dstore(this->_absState, bci.nextByte()); break;
     case J9BCdstorew: this->dstore(this->_absState, bci.next2Bytes()); break; // WARN: internal bytecode
     case J9BCdstore0: this->dstore0(this->_absState); break;
     case J9BCdstore1: this->dstore1(this->_absState); break;
     case J9BCdstore2: this->dstore2(this->_absState); break;
     case J9BCdstore3: this->dstore3(this->_absState); break;
     case J9BCdsub: this->dsub(this->_absState); break;
     case J9BCdup: this->dup(this->_absState); break;
     case J9BCdupx1: this->dupx1(this->_absState); break;
     case J9BCdupx2: this->dupx2(this->_absState); break;
     case J9BCdup2: this->dup2(this->_absState); break;
     case J9BCdup2x1: this->dup2x1(this->_absState); break;
     case J9BCdup2x2: this->dup2x2(this->_absState); break;
     case J9BCf2d: this->f2d(this->_absState); break;
     case J9BCf2i: this->f2i(this->_absState); break;
     case J9BCf2l: this->f2l(this->_absState); break;
     case J9BCfadd: this->fadd(this->_absState); break;
     case J9BCfaload: this->faload(this->_absState); break;
     case J9BCfastore: this->fastore(this->_absState); break;
     case J9BCfcmpl: this->fcmpl(this->_absState); break;
     case J9BCfcmpg: this->fcmpg(this->_absState); break;
     case J9BCfconst0: this->fconst0(this->_absState); break;
     case J9BCfconst1: this->fconst1(this->_absState); break;
     case J9BCfdiv: this->fdiv(this->_absState); break;
     case J9BCfload: this->fload(this->_absState, bci.nextByte()); break;
     case J9BCfload0: this->fload0(this->_absState); break;
     case J9BCfload1: this->fload1(this->_absState); break;
     case J9BCfload2: this->fload2(this->_absState); break;
     case J9BCfload3: this->fload3(this->_absState); break;
     case J9BCfmul: this->fmul(this->_absState); break;
     case J9BCfneg: this->fneg(this->_absState); break;
     case J9BCfrem: this->frem(this->_absState); break;
     //case J9BCfreturn: 
     case J9BCfstore: this->fstore(this->_absState, bci.nextByte()); break;
     case J9BCfstorew: this->fstore(this->_absState, bci.next2Bytes()); break; // WARN: internal bytecode
     case J9BCfstore0: this->fstore0(this->_absState); break;
     case J9BCfstore1: this->fstore1(this->_absState); break;
     case J9BCfstore2: this->fstore2(this->_absState); break;
     case J9BCfstore3: this->fstore3(this->_absState); break;
     case J9BCfsub: this->fsub(this->_absState); break;
     case J9BCgenericReturn: this->getState().getStackSize() != 0 ? this->_absState.pop() : 0; break; 
     case J9BCgetfield: this->getfield(this->_absState, bci.next2Bytes()); break;
     case J9BCgetstatic: this->getstatic(this->_absState, bci.next2Bytes()); break;
     case J9BCgoto: this->_goto(this->_absState, bci.next2BytesSigned()); break;
     case J9BCgotow: this->_gotow(this->_absState, bci.next4BytesSigned()); break;
     case J9BCi2b: this->i2b(this->_absState); break;
     case J9BCi2c: this->i2c(this->_absState); break;
     case J9BCi2d: this->i2d(this->_absState); break;
     case J9BCi2f: this->i2f(this->_absState); break;
     case J9BCi2l: this->i2l(this->_absState); break;
     case J9BCi2s: this->i2s(this->_absState); break;
     case J9BCiadd: this->iadd(this->_absState); break;
     case J9BCiaload: this->iaload(this->_absState); break;
     case J9BCiand: this->iand(this->_absState); break;
     case J9BCiastore: this->iastore(this->_absState); break;
     case J9BCiconstm1: this->iconstm1(this->_absState); break;
     case J9BCiconst0: this->iconst0(this->_absState); break;
     case J9BCiconst1: this->iconst1(this->_absState); break;
     case J9BCiconst2: this->iconst2(this->_absState); break;
     case J9BCiconst3: this->iconst3(this->_absState); break;
     case J9BCiconst4: this->iconst4(this->_absState); break;
     case J9BCiconst5: this->iconst5(this->_absState); break;
     case J9BCidiv: this->idiv(this->_absState); break;
     case J9BCifacmpeq: this->ificmpeq(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break; //TODO own function
     case J9BCifacmpne: this->ificmpne(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break; //TODO own function
     case J9BCificmpeq: this->ificmpeq(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCificmpge: this->ificmpge(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCificmpgt: this->ificmpgt(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCificmple: this->ificmple(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCificmplt: this->ificmplt(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCificmpne: this->ificmpne(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCifeq: this->ifeq(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCifge: this->ifge(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCifgt: this->ifgt(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCifle: this->ifle(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCiflt: this->iflt(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCifne: this->ifne(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCifnonnull: this->ifnonnull(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCifnull: this->ifnull(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     case J9BCiinc: this->iinc(this->_absState, bci.nextByte(), bci.nextByteSigned(2)); break;
     case J9BCiload: this->iload(this->_absState, bci.nextByte()); break;
     case J9BCiload0: this->iload0(this->_absState); break;
     case J9BCiload1: this->iload1(this->_absState); break;
     case J9BCiload2: this->iload2(this->_absState); break;
     case J9BCiload3: this->iload3(this->_absState); break;
     case J9BCimul: this->imul(this->_absState); break;
     case J9BCineg: this->ineg(this->_absState); break;
     case J9BCinstanceof: this->instanceof(this->_absState, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
     // invokes...
     //case J9BCinvokedynamic: this->invokedynamic(bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokeinterface: this->invokeinterface(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokespecial: this->invokespecial(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokestatic: this->invokestatic(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokevirtual: this->invokevirtual(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCior: this->ior(this->_absState); break;
     case J9BCirem: this->irem(this->_absState); break;
     //case J9BCireturn:
     case J9BCishl: this->ishl(this->_absState); break;
     case J9BCishr: this->ishr(this->_absState); break;
     case J9BCistore: this->istore(this->_absState, bci.nextByte()); break;
     case J9BCistorew: this->istore(this->_absState, bci.next2Bytes()); break; //WARN: internal bytecode
     case J9BCistore0: this->istore0(this->_absState); break;
     case J9BCistore1: this->istore1(this->_absState); break;
     case J9BCistore2: this->istore2(this->_absState); break;
     case J9BCistore3: this->istore3(this->_absState); break;
     case J9BCisub: this->isub(this->_absState); break;
     case J9BCiushr: this->iushr(this->_absState); break;
     case J9BCixor: this->ixor(this->_absState); break;
     // jsr
     // jsr_w
     case J9BCl2d: this->l2d(this->_absState); break;
     case J9BCl2f: this->l2f(this->_absState); break;
     case J9BCl2i: this->l2i(this->_absState); break;
     case J9BCladd: this->ladd(this->_absState); break;
     case J9BClaload: this->laload(this->_absState); break;
     case J9BCland: this->land(this->_absState); break;
     case J9BClastore: this->lastore(this->_absState); break;
     case J9BClcmp: this->lcmp(this->_absState); break;
     case J9BClconst0: this->lconst0(this->_absState); break;
     case J9BClconst1: this->lconst1(this->_absState); break;
     case J9BCldc: this->ldc(this->_absState, bci.nextByte()); break;
     case J9BCldcw: this->ldcw(this->_absState, bci.next2Bytes()); break;
     case J9BCldc2lw: this->ldc(this->_absState, bci.next2Bytes()); break; //WARN: internal bytecode equivalent to ldc2_w
     case J9BCldc2dw: this->ldc(this->_absState, bci.next2Bytes()); break; //WARN: internal bytecode equivalent to ldc2_w
     case J9BCldiv: this->ldiv(this->_absState); break;
     case J9BClload: this->lload(this->_absState, bci.nextByte()); break;
     case J9BClload0: this->lload0(this->_absState); break;
     case J9BClload1: this->lload1(this->_absState); break;
     case J9BClload2: this->lload2(this->_absState); break;
     case J9BClload3: this->lload3(this->_absState); break;
     case J9BClmul: this->lmul(this->_absState); break;
     case J9BClneg: this->lneg(this->_absState); break;
     case J9BClookupswitch : this->lookupswitch(this->_absState); break;
     case J9BClor: this->lor(this->_absState); break;
     case J9BClrem: this->lrem(this->_absState); break;
     //case J9BClreturn: 
     case J9BClshl: this->lshl(this->_absState); break;
     case J9BClshr: this->lshr(this->_absState); break;
     case J9BClsub: this->lsub(this->_absState); break;
     case J9BClushr: this->lushr(this->_absState); break;
     case J9BClstore: this->lstore(this->_absState, bci.nextByte()); break;
     case J9BClstorew: this->lstorew(this->_absState, bci.next2Bytes()); break; // WARN: internal bytecode
     case J9BClstore0: this->lstore0(this->_absState); break;
     case J9BClstore1: this->lstore1(this->_absState); break;
     case J9BClstore2: this->lstore2(this->_absState); break;
     case J9BClstore3: this->lstore3(this->_absState); break;
     case J9BClxor: this->lxor(this->_absState); break;
     case J9BCmonitorenter: this->monitorenter(this->_absState); break;
     case J9BCmonitorexit: this->monitorexit(this->_absState); break;
     case J9BCmultianewarray: this->multianewarray(this->_absState, bci.next2Bytes(), bci.nextByteSigned(3)); break;
     case J9BCnew: this->_new(this->_absState, bci.next2Bytes()); break;
     case J9BCnewarray: this->newarray(this->_absState, bci.nextByte()); break;
     case J9BCnop: /* yeah nothing */ break;
     case J9BCpop: this->_absState.pop(); break;
     case J9BCpop2: this->pop2(this->_absState); break;
     case J9BCputfield: this->putfield(this->_absState); break;
     case J9BCputstatic: this->putstatic(this->_absState); break;
     // ret
     // return
     case J9BCsaload: this->saload(this->_absState); break;
     case J9BCsastore: this->sastore(this->_absState); break;
     case J9BCsipush: this->sipush(this->_absState, bci.next2Bytes()); break;
     case J9BCswap: this->swap(this->_absState); break;
     case J9BCReturnC: this->_absState.pop(); break;
     case J9BCReturnS: this->_absState.pop(); break;
     case J9BCReturnB: this->_absState.pop(); break;
     case J9BCReturnZ: /*yeah nothing */ break;
     case J9BCtableswitch: this->tableswitch(this->_absState); break;
     //TODO: clean me
     case J9BCwide:
       {
       //TODO: iincw
       bci.setIndex(bci.currentByteCodeIndex() + 1);
       TR_J9ByteCode bc2 = bci.current();
       switch (bc2)
          {
          case J9BCiload: this->iload(this->_absState, bci.next2Bytes()); break;
          case J9BClload: this->lload(this->_absState, bci.next2Bytes()); break;
          case J9BCfload: this->fload(this->_absState, bci.next2Bytes()); break;
          case J9BCdload: this->dload(this->_absState, bci.next2Bytes()); break;
          case J9BCaload: this->aload(this->_absState, bci.next2Bytes()); break;
          case J9BCistore: this->istore(this->_absState, bci.next2Bytes()); break; 
          case J9BClstore: this->dstore(this->_absState, bci.next2Bytes()); break;  // TODO: own function?
          case J9BCfstore: this->fstore(this->_absState, bci.next2Bytes()); break; 
          case J9BCdstore: this->dstore(this->_absState, bci.next2Bytes()); break; 
          case J9BCastore: this->astore(this->_absState, bci.next2Bytes()); break; 
          }
       }
     break;
     default:
     break;
     }
  if (comp->trace(OMR::benefitInliner))
     {
     traceMsg(comp, "\n");
     this->trace();
     }
  }

void
AbsEnvStatic::trace(const char* methodName)
  {
  if (methodName) traceMsg(TR::comp(), "method %s\n", methodName);
  this->getState().trace(this->getVP());
  }

void
AbstractState::trace(TR::ValuePropagation *vp)
{
   this->_array.trace(vp);
   this->_stack.trace(vp, this->_region);
}

AbstractState&
AbsEnvStatic::multianewarray(AbstractState & absState, int cpIndex, int dimensions)
{
  for (int i = 0; i < dimensions; i++)
    {
    absState.pop();
    }
  this->pushNull();
  return absState;
}

AbstractState&
AbsEnvStatic::caload(AbstractState& absState)
{
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  this->pushNull();
  return absState;
}


AbstractState&
AbsEnvStatic::faload(AbstractState & absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  this->pushNull();
  return absState;
  }

AbstractState&
AbsEnvStatic::iaload(AbstractState & absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  this->pushNull();
  return absState;
  }

AbstractState&
AbsEnvStatic::saload(AbstractState & absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  this->pushNull();
  return absState;
  }

AbstractState&
AbsEnvStatic::aaload(AbstractState & absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  this->pushNull();
  return absState;
  }

AbstractState&
AbsEnvStatic::laload(AbstractState &absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  AbsValue *value1 = this->getTopDataType(TR::Int64);
  AbsValue *value2 = this->getTopDataType(TR::NoType);
  absState.push(value1);
  absState.push(value2);
  return absState;
  }

AbstractState&
AbsEnvStatic::daload(AbstractState &absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  AbsValue *value1 = this->getTopDataType(TR::Double);
  AbsValue *value2 = this->getTopDataType(TR::NoType);
  absState.push(value1);
  absState.push(value2);
  return absState;
  }

AbstractState&
AbsEnvStatic::castore(AbstractState &absState)
{
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
}

AbstractState&
AbsEnvStatic::dastore(AbstractState &absState)
{
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
}

AbstractState&
AbsEnvStatic::fastore(AbstractState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbstractState&
AbsEnvStatic::iastore(AbstractState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbstractState&
AbsEnvStatic::lastore(AbstractState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbstractState&
AbsEnvStatic::sastore(AbstractState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbstractState&
AbsEnvStatic::aastore(AbstractState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbstractState&
AbsEnvStatic::aconstnull(AbstractState &absState) {
  TR::VPConstraint *null = TR::VPNullObject::create(this->getVP());
  AbsValue *absValue = createAbsValue(null, TR::Address);
  absState.push(absValue);
  return absState;
}

AbstractState&
AbsEnvStatic::aload(AbstractState &absState, int n) {
  this->aloadn(absState, n);
  return absState;
}

AbstractState&
AbsEnvStatic::aload0(AbstractState &absState) {
  this->aloadn(absState, 0);
  return absState;
}

AbstractState&
AbsEnvStatic::aload1(AbstractState &absState) {
  this->aloadn(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::aload2(AbstractState &absState) {
  this->aloadn(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::aload3(AbstractState &absState) {
  this->aloadn(absState, 3);
  return absState;
}

void
AbsEnvStatic::aloadn(AbstractState& absState, int n) {
  AbsValue *constraint = absState.at(n);
  absState.push(constraint);
}

AbstractState&
AbsEnvStatic::astore(AbstractState &absState, int varIndex) {
  AbsValue* constraint = absState.pop();
  absState.at(varIndex, constraint);
  return absState;
}

AbstractState&
AbsEnvStatic::astore0(AbstractState &absState) {
  this->astore(absState, 0);
  return absState;
}

AbstractState&
AbsEnvStatic::astore1(AbstractState &absState) {
  this->astore(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::astore2(AbstractState &absState) {
  this->astore(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::astore3(AbstractState &absState) {
  this->astore(absState, 3);
  return absState;
}

AbstractState&
AbsEnvStatic::bipush(AbstractState &absState, int byte) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->getVP(), byte);
  //TODO: should I use TR::Int32 or something else?
  AbsValue *absValue = createAbsValue(data, TR::Int32);
  absState.push(absValue);
  return absState;
}

AbstractState&
AbsEnvStatic::bastore(AbstractState &absState) {
  this->aastore(absState);
  return absState;
}

AbstractState&
AbsEnvStatic::baload(AbstractState &absState) {
  this->aaload(absState);
  return absState;
}

AbstractState&
AbsEnvStatic::checkcast(AbstractState &absState, int cpIndex, int bytecodeIndex) {
  AbsValue *absValue = absState.pop();
  TR::VPConstraint *objectRef = absValue->_vp;
  if (!objectRef)
    {
    absState.push(absValue);
    return absState;
    }

  TR_ResolvedMethod* method = this->getResolvedMethodSymbol()->getResolvedMethod();
  // TODO: can we do this some other way? I don't want to pass TR::comp();
  TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
  TR::VPConstraint *typeConstraint = NULL;
  // If objectRef is not assignable to a class of type type then throw an exception.

  absState.push(absValue);
  return absState;
}

AbstractState&
AbsEnvStatic::dup(AbstractState &absState) {
  AbsValue *value = absState.pop();
  absState.push(value);
  absState.push(value); 
  return absState;
}

AbstractState&
AbsEnvStatic::dupx1(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  absState.push(value1);
  absState.push(value2);
  absState.push(value1);
  return absState;
}

AbstractState&
AbsEnvStatic::dupx2(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  absState.push(value1);
  absState.push(value3);
  absState.push(value2);
  absState.push(value1);
  return absState;
}

AbstractState&
AbsEnvStatic::dup2(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  absState.push(value2);
  absState.push(value1);
  absState.push(value2);
  absState.push(value1);
  return absState;
}

AbstractState&
AbsEnvStatic::dup2x1(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  absState.push(value2);
  absState.push(value1);
  absState.push(value3);
  absState.push(value2);
  absState.push(value1);
  return absState;
}

AbstractState&
AbsEnvStatic::dup2x2(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue *value4 = absState.pop();
  absState.push(value2);
  absState.push(value1);
  absState.push(value4);
  absState.push(value3);
  absState.push(value2);
  absState.push(value1);
  return absState;
}

AbstractState&
AbsEnvStatic::_goto(AbstractState &absState, int branch)
{
  return absState;
}


AbstractState&
AbsEnvStatic::_gotow(AbstractState &absState, int branch)
{
  return absState;
}

AbstractState&
AbsEnvStatic::getstatic(AbstractState &absState, int cpIndex) {
   void* staticAddress;
   TR::DataType type = TR::NoType;
   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   bool isResolved = this->getResolvedMethodSymbol()->getResolvedMethod()->staticAttributes(TR::comp(),
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
   absState.push(value1);
   if (!value1->isType2()) { return absState; }
  AbsValue *value2= this->getTopDataType(TR::NoType);
  absState.push(value2);
  return absState;
}


AbstractState&
AbsEnvStatic::getfield(AbstractState &absState, int cpIndex) {
   AbsValue *objectref = absState.pop();
   uint32_t fieldOffset;
   TR::DataType type = TR::NoType;
   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   bool isResolved = this->getResolvedMethodSymbol()->getResolvedMethod()->fieldAttributes(TR::comp(),
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
   absState.push(value1);
   if (!value1->isType2()) { return absState; }
   AbsValue *value2= this->getTopDataType(TR::NoType);
   absState.push(value2);
  return absState;
   
}

AbstractState&
AbsEnvStatic::iand(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int result = value1->_vp->asIntConst()->getLow() & value2->_vp->asIntConst()->getLow();
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::instanceof(AbstractState &absState, int cpIndex, int byteCodeIndex) {
  AbsValue *objectRef = absState.pop();
  if (!objectRef->_vp)
     {
     absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), 0), TR::Int32));
     return absState;
     }

  TR_ResolvedMethod* method = this->getResolvedMethodSymbol()->getResolvedMethod();
  TR_OpaqueClassBlock *block = method->getClassFromConstantPool(TR::comp(), cpIndex);
  if (!block)
     {
     absState.push(createAbsValue(TR::VPIntRange::create(this->getVP(), 0, 1), TR::Int32));
     return absState;
     }

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->getVP(), block);
  TR::VPObjectLocation *objectLocation = TR::VPObjectLocation::create(this->getVP(), TR::VPObjectLocation::J9ClassObject);
  TR::VPConstraint *typeConstraint= TR::VPClass::create(this->getVP(), fixedClass, NULL, NULL, NULL, objectLocation);
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
     if(typeConstraint->intersect(objectRef->_vp, this->getVP()))
        absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), 1), TR::Int32));
     else
        absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), 0), TR::Int32));
   return absState;
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

  absState.push(createAbsValue(TR::VPIntRange::create(this->getVP(), 0, 1), TR::Int32));
   return absState;
}

AbstractState&
AbsEnvStatic::ior(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int result = value1->_vp->asIntConst()->getLow() | value2->_vp->asIntConst()->getLow();
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::ixor(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int result = value1->_vp->asIntConst()->getLow() ^ value2->_vp->asIntConst()->getLow();
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::irem(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow();
  int result = int1 - (int1/ int2) * int2;
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::ishl(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  int result = int1 << int2;
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::ishr(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  //arithmetic shift.
  int result = int1 >> int2;
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::iushr(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  int result = int1 >> int2;
  //logical shift, gets rid of the sign.
  result &= 0x7FFFFFFF;
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::idiv(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow();
  if (int2 == 0)
    {
     // this should throw an exception.
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
    }
  int result = int1 / int2;
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::imul(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->_vp->asIntConst() && value2->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->_vp->asIntConst()->getLow();
  int int2 = value2->_vp->asIntConst()->getLow();
  int result = int1 * int2;
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::ineg(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  bool allConstants = value1->_vp && value1->_vp->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  //TODO: more precision for ranges, subtract VPIntConst 0 from value1
  int int1 = value1->_vp->asIntConst()->getLow();
  int result = -int1;
  absState.push(createAbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbstractState&
AbsEnvStatic::iconstm1(AbstractState &absState) {
  this->iconst(absState, -1);
  return absState;
}

AbstractState&
AbsEnvStatic::iconst0(AbstractState &absState) {
  this->iconst(absState, 0);
  return absState;
}

AbstractState&
AbsEnvStatic::iconst1(AbstractState &absState) {
  this->iconst(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::iconst2(AbstractState &absState) {
  this->iconst(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::iconst3(AbstractState &absState) {
  this->iconst(absState, 3);
  return absState;
}

AbstractState&
AbsEnvStatic::iconst4(AbstractState &absState) {
  this->iconst(absState, 4);
  return absState;
}

AbstractState&
AbsEnvStatic::iconst5(AbstractState &absState) {
  this->iconst(absState, 5);
  return absState;
}

void
AbsEnvStatic::iconst(AbstractState &absState, int n) {
  this->pushConstInt(absState, n);
}

AbstractState&
AbsEnvStatic::ifeq(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ifne(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::iflt(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ifle(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ifgt(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ifge(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ifnull(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ifnonnull(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ificmpge(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ificmpeq(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ificmpne(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ificmplt(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ificmpgt(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ificmple(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ifacmpeq(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::ifacmpne(AbstractState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::iload(AbstractState& absState, int n) {
  this->aloadn(absState, n);
  return absState;
}

AbstractState&
AbsEnvStatic::iload0(AbstractState& absState) {
  this->iload(absState, 0);
  return absState;
}

AbstractState&
AbsEnvStatic::iload1(AbstractState& absState) {
  this->iload(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::iload2(AbstractState& absState) {
  this->iload(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::iload3(AbstractState& absState) {
  this->iload(absState, 3);
  return absState;
}

AbstractState&
AbsEnvStatic::istore(AbstractState& absState, int n) {
  absState.at(n, absState.pop());
  return absState;
}

AbstractState&
AbsEnvStatic::istore0(AbstractState& absState) {
  absState.at(0, absState.pop());
  return absState;
}

AbstractState&
AbsEnvStatic::istore1(AbstractState& absState) {
  absState.at(1, absState.pop());
  return absState;
}

AbstractState&
AbsEnvStatic::istore2(AbstractState& absState) {
  absState.at(2, absState.pop());
  return absState;
}

AbstractState&
AbsEnvStatic::istore3(AbstractState& absState) {
  absState.at(3, absState.pop());
  return absState;
}

AbstractState&
AbsEnvStatic::isub(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
    {
    AbsValue *result = this->getTopDataType(TR::Int32);
    absState.push(result);
    return absState;
    }

  TR::VPConstraint *result_vp = value1->_vp->subtract(value2->_vp, value2->_dt, this->getVP());
  AbsValue *result = createAbsValue(result_vp, value2->_dt);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::iadd(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  bool nonnull = value1->_vp && value2->_vp;
  if (!nonnull)
    {
    AbsValue *result = this->getTopDataType(TR::Int32);
    absState.push(result);
    return absState;
    }

  TR::VPConstraint *result_vp = value1->_vp->add(value2->_vp, value2->_dt, this->getVP());
  AbsValue *result = createAbsValue(result_vp, value2->_dt);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::i2d(AbstractState& absState) {
  AbsValue *value = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Double);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::i2f(AbstractState& absState) {
  AbsValue *value = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::i2l(AbstractState& absState) {
  AbsValue *value = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int64);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::i2s(AbstractState& absState) {
  //empty?
  return absState;
}

AbstractState&
AbsEnvStatic::i2c(AbstractState& absState) {
  //empty?
  return absState;
}

AbstractState&
AbsEnvStatic::i2b(AbstractState& absState) {
  // empty?
  return absState;
}

AbstractState&
AbsEnvStatic::dadd(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue *value4 = absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::dsub(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue *value4 = absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::fsub(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::fadd(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::ladd(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue *value4 = absState.pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
    return absState;
    }

  TR::VPConstraint *result_vp = value2->_vp->add(value4->_vp, value4->_dt, this->getVP());
  AbsValue *result = createAbsValue(result_vp, TR::Int64);
  absState.push(result);
  this->getTopDataType(TR::NoType);
  return absState;
}

AbstractState&
AbsEnvStatic::lsub(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue *value4 = absState.pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
    return absState;
    }

  TR::VPConstraint *result_vp = value2->_vp->subtract(value4->_vp, value4->_dt, this->getVP());
  AbsValue *result = createAbsValue(result_vp, TR::Int64);
  absState.push(result);
  this->getTopDataType(TR::NoType);
  return absState;
}

AbstractState&
AbsEnvStatic::l2i(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  bool nonnull = value2->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int32);
    absState.push(result1);
    return absState;
    }

  bool canCompute = value2->_vp && value2->_vp->asLongConstraint();
  if (!canCompute)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  TR::VPConstraint *intConst = TR::VPIntRange::create(this->getVP(), value2->_vp->asLongConstraint()->getLow(), value2->_vp->asLongConstraint()->getHigh());
  AbsValue *result = createAbsValue(intConst, TR::Int32);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::land(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue *value4 = absState.pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
    return absState;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  int result = value2->_vp->asLongConst()->getLow() & value4->_vp->asLongConst()->getLow();
  absState.push(createAbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::ldiv(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value2->_vp->asLongConst()->getLow();
  long int2 = value4->_vp->asLongConst()->getLow();
  if (int2 == 0)
    {
     // this should throw an exception.
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
    }
  long result = int1 / int2;
  absState.push(createAbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lmul(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value1->_vp->asLongConst()->getLow();
  long int2 = value2->_vp->asLongConst()->getLow();
  long result = int1 * int2;
  absState.push(createAbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lookupswitch(AbstractState& absState)
{
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::tableswitch(AbstractState& absState)
{
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::lneg(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  bool nonnull = value2->_vp;
  if (!nonnull)
    {
    absState.push(value2);
    absState.push(value1);
     return absState;
    }
  bool allConstants = value2->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value1->_vp->asLongConst()->getLow();
  long result = -int1;
  absState.push(createAbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lor(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->_vp->asLongConst() && value2->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long result = value1->_vp->asLongConst()->getLow() | value2->_vp->asLongConst()->getLow();
  absState.push(createAbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lrem(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value2->_vp->asLongConst()->getLow();
  long int2 = value4->_vp->asLongConst()->getLow();
  long result = int1 - (int1/ int2) * int2;
  absState.push(createAbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lshl(AbstractState& absState) {
  AbsValue *value2 = absState.pop(); // int
  AbsValue* value0 = absState.pop(); // nothing
  AbsValue* value1 = absState.pop(); // long
  bool nonnull = value2->_vp && value1->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->_vp->asIntConst() && value1->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value1->_vp->asLongConst()->getLow();
  long int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  long result = int1 << int2;
  absState.push(createAbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lshr(AbstractState& absState) {
  AbsValue *value2 = absState.pop(); // int
  AbsValue* value0 = absState.pop(); // nothing
  AbsValue* value1 = absState.pop(); // long
  bool nonnull = value2->_vp && value1->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->_vp->asIntConst() && value1->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value1->_vp->asLongConst()->getLow();
  long int2 = value2->_vp->asIntConst()->getLow() & 0x1f;
  long result = int1 >> int2;
  absState.push(createAbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lushr(AbstractState& absState) {
  this->lshr(absState);
  return absState;
}

AbstractState&
AbsEnvStatic::lxor(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->_vp && value4->_vp;
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
    return absState;
    }
  bool allConstants = value2->_vp->asLongConst() && value4->_vp->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long result = value1->_vp->asLongConst()->getLow() ^ value2->_vp->asLongConst()->getLow();
  absState.push(createAbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  return absState;
}

AbstractState&
AbsEnvStatic::l2d(AbstractState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Double);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::l2f(AbstractState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::d2f(AbstractState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::f2d(AbstractState& absState) {
  absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::f2i(AbstractState& absState) {
  absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Int32);
  absState.push(result1);
  return absState;
}

AbstractState&
AbsEnvStatic::f2l(AbstractState& absState) {
  absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Int64);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::d2i(AbstractState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::d2l(AbstractState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Int64);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lload(AbstractState& absState, int n) {
  aload(absState, n);
  aload(absState, n + 1);
  return absState;
}

AbstractState&
AbsEnvStatic::lload0(AbstractState& absState) {
  aload(absState, 0);
  aload(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::lload1(AbstractState &absState) {
  aload(absState, 1);
  aload(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::lload2(AbstractState &absState) {
  aload(absState, 2);
  aload(absState, 3);
  return absState;
}

AbstractState&
AbsEnvStatic::lload3(AbstractState &absState) {
  aload(absState, 3);
  aload(absState, 4);
  return absState;
}

AbstractState&
AbsEnvStatic::dconst0(AbstractState &absState) {
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::dconst1(AbstractState &absState) {
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::fconst0(AbstractState &absState) {
  AbsValue *result1 = this->getTopDataType(TR::Float);
  absState.push(result1);
  return absState;
}

AbstractState&
AbsEnvStatic::fconst1(AbstractState &absState) {
  AbsValue *result1 = this->getTopDataType(TR::Float);
  absState.push(result1);
  return absState;
}

AbstractState&
AbsEnvStatic::dload(AbstractState &absState, int n) {
  aload(absState, n);
  aload(absState, n + 1);
  return absState;
}

AbstractState&
AbsEnvStatic::dload0(AbstractState &absState) {
  aload(absState, 0);
  aload(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::dload1(AbstractState &absState) {
  aload(absState, 1);
  aload(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::dload2(AbstractState &absState) {
  aload(absState, 2);
  aload(absState, 3);
  return absState;
}

AbstractState&
AbsEnvStatic::dload3(AbstractState &absState) {
  aload(absState, 3);
  aload(absState, 4);
  return absState;
}

AbstractState&
AbsEnvStatic::lstorew(AbstractState &absState, int n)
{
  lstore(absState, n);
  return absState;
}

AbstractState&
AbsEnvStatic::lstore0(AbstractState &absState)
{
  lstore(absState, 0);
  return absState;
}

AbstractState&
AbsEnvStatic::lstore1(AbstractState &absState)
{
  lstore(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::lstore2(AbstractState &absState)
{
  lstore(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::lstore3(AbstractState &absState)
{
  lstore(absState, 3);
  return absState;
}


AbstractState&
AbsEnvStatic::lstore(AbstractState &absState, int n) {
  AbsValue *top = absState.pop();
  AbsValue *bottom = absState.pop();
  absState.at(n, bottom);
  absState.at(n + 1, top);
  return absState;
}

AbstractState&
AbsEnvStatic::dstore(AbstractState &absState, int n) {
  AbsValue *top = absState.pop();
  AbsValue *bottom = absState.pop();
  absState.at(n, bottom);
  absState.at(n + 1, top);
  return absState;
}

AbstractState&
AbsEnvStatic::dstore0(AbstractState& absState) {
  this->dstore(absState, 0);
  return absState;
}

AbstractState&
AbsEnvStatic::dstore1(AbstractState& absState) {
  this->dstore(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::dstore2(AbstractState& absState) {
  this->dstore(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::dstore3(AbstractState& absState) {
  this->dstore(absState, 3);
  return absState;
}

AbstractState&
AbsEnvStatic::lconst0(AbstractState& absState) {
  AbsValue *result = this->getTopDataType(TR::Int64);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lconst1(AbstractState& absState) {
  AbsValue *result = this->getTopDataType(TR::Int64);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbstractState&
AbsEnvStatic::lcmp(AbstractState& absState) {
  absState.pop();
  absState.pop();
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::pop2(AbstractState& absState) {
  absState.pop();
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::fload(AbstractState& absState, int n) {
  aload(absState, n);
  return absState;
}

AbstractState&
AbsEnvStatic::fload0(AbstractState& absState) {
  this->fload(absState, 0);
  return absState;
}

AbstractState&
AbsEnvStatic::fload1(AbstractState& absState) {
  this->fload(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::fload2(AbstractState& absState) {
  this->fload(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::fload3(AbstractState& absState) {
  this->fload(absState, 3);
  return absState;
}

AbstractState&
AbsEnvStatic::swap(AbstractState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  absState.push(value1);
  absState.push(value2);
  return absState;
}

AbstractState&
AbsEnvStatic::fstore(AbstractState& absState, int n) {
  absState.at(n, absState.pop());
  return absState;
}


AbstractState&
AbsEnvStatic::fstore0(AbstractState& absState) {
  this->fstore(absState, 0);
  return absState;
}

AbstractState&
AbsEnvStatic::fstore1(AbstractState& absState) {
  this->fstore(absState, 1);
  return absState;
}

AbstractState&
AbsEnvStatic::fstore2(AbstractState& absState) {
  this->fstore(absState, 2);
  return absState;
}

AbstractState&
AbsEnvStatic::fstore3(AbstractState &absState) {
  this->fstore(absState, 3);
  return absState;
}

AbstractState&
AbsEnvStatic::fmul(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::dmul(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::dneg(AbstractState &absState)
{
  return absState;
}

AbstractState&
AbsEnvStatic::dcmpl(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::dcmpg(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::fcmpg(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::fcmpl(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::ddiv(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::fdiv(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::drem(AbstractState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::fneg(AbstractState &absState)
{
  return absState;
}

AbstractState&
AbsEnvStatic::freturn(AbstractState &absState)
{
  return absState;
}

AbstractState&
AbsEnvStatic::frem(AbstractState &absState) {
  AbsValue *value = absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::sipush(AbstractState &absState, int16_t _short) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->getVP(), _short);
  AbsValue *result = createAbsValue(data, TR::Int16);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::iinc(AbstractState& absState, int index, int incval) {
  AbsValue *value1 = absState.at(index);
  TR::VPIntConstraint *value = value1->_vp ? value1->_vp->asIntConstraint() : nullptr;
  if (!value)
    {
    return absState;
    }

  TR::VPIntConst *inc = TR::VPIntConst::create(this->getVP(), incval);
  TR::VPConstraint *result = value->add(inc, TR::Int32, this->getVP());
  AbsValue *result2 = createAbsValue(result, TR::Int32);
  absState.at(index, result2);
  return absState;
}

AbstractState&
AbsEnvStatic::putfield(AbstractState& absState) {
  // WONTFIX we do not model the heap
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  if(value2->isType2()) {
    AbsValue *value3 = absState.pop();
  }
  return absState;
}

AbstractState&
AbsEnvStatic::putstatic(AbstractState& absState) {
  // WONTFIX we do not model the heap
  AbsValue *value1 = absState.pop();
  if (!value1->_dt == TR::NoType) { // category type 2
    AbsValue *value2 = absState.pop();
  }
  return absState;
}

void
AbsEnvStatic::ldcInt32(int cpIndex) {
  int32_t value =  this->getResolvedMethodSymbol()->getResolvedMethod()->intConstant(cpIndex); 
  TR::VPIntConst *constraint = TR::VPIntConst::create(this->getVP(), value);
  AbsValue *result = createAbsValue(constraint, TR::Int32);
  this->getState().push(result);
}

void
AbsEnvStatic::ldcInt64(int cpIndex) {
   auto value =  this->getResolvedMethodSymbol()->getResolvedMethod()->longConstant(cpIndex); 
   TR::VPLongConst *constraint = TR::VPLongConst::create(this->getVP(), value);
   AbsValue *result = createAbsValue(constraint, TR::Int64);
   this->getState().push(result);
   AbsValue *result2 = this->getTopDataType(TR::NoType);
   this->getState().push(result2);
}

void
AbsEnvStatic::ldcFloat() {
   AbsValue *result = this->getTopDataType(TR::Float);
   this->getState().push(result);
}

void
AbsEnvStatic::ldcDouble() {
   AbsValue *result = this->getTopDataType(TR::Double);
   AbsValue *result2 = this->getTopDataType(TR::NoType);
   this->getState().push(result);
   this->getState().push(result2);
}

void
AbsEnvStatic::ldcAddress(int cpIndex) {
  bool isString = this->getResolvedMethodSymbol()->getResolvedMethod()->isStringConstant(cpIndex);
  if (isString) { ldcString(cpIndex); return; }
  //TODO: non string case
  AbsValue *result = this->getTopDataType(TR::Address);
  this->getState().push(result);
}

void
AbsEnvStatic::ldcString(int cpIndex) {
   // TODO: we might need the resolved method symbol here
   // TODO: aAbstractState& _rms    
   TR::SymbolReference *symRef = TR::comp()->getSymRefTab()->findOrCreateStringSymbol(this->getResolvedMethodSymbol(), cpIndex);
   if (symRef->isUnresolved())
        {
        AbsValue *result = this->getTopDataType(TR::Address);
        this->getState().push(result);
        return;
        }
   TR::VPConstraint *constraint = TR::VPConstString::create(this->getVP(), symRef);
   AbsValue *result = createAbsValue(constraint, TR::Address);
   this->getState().push(result);
}

AbstractState&
AbsEnvStatic::ldcw(AbstractState &absState, int cpIndex)
{
  ldc(absState, cpIndex);
  return absState;
}

AbstractState&
AbsEnvStatic::ldc(AbstractState& absState, int cpIndex) {
   TR::DataType datatype = this->getResolvedMethodSymbol()->getResolvedMethod()->getLDCType(cpIndex);
   switch(datatype) {
     case TR::Int32: this->ldcInt32(cpIndex); break;
     case TR::Int64: this->ldcInt64(cpIndex); break;
     case TR::Float: this->ldcFloat(); break;
     case TR::Double: this->ldcDouble(); break;
     case TR::Address: this->ldcAddress(cpIndex); break;
     default: {
       //TODO: arrays and what nots.
       AbsValue *result = this->getTopDataType(TR::Address);
      absState.push(result);
      
     } break;
   }
  return absState;
}

AbstractState&
AbsEnvStatic::monitorenter(AbstractState& absState) {
  // TODO: possible optimization
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::monitorexit(AbstractState& absState) {
  // TODO: possible optimization
  absState.pop();
  return absState;
}

AbstractState&
AbsEnvStatic::areturn(AbstractState& absState)
{
  return absState;
}

AbstractState&
AbsEnvStatic::dreturn(AbstractState& absState)
{
  return absState;
}

AbstractState&
AbsEnvStatic::athrow(AbstractState& absState)
{
  return absState;
}

AbstractState&
AbsEnvStatic::anewarray(AbstractState& absState, int cpIndex) {
  //TODO: actually make an array
  AbsValue *count = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Address);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::arraylength(AbstractState& absState) {
  //TODO: actually make use of the value
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::_new(AbstractState& absState, int cpIndex) {
  //TODO: actually look at the semantics
  AbsValue *result = this->getTopDataType(TR::Address);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::newarray(AbstractState& absState, int atype) {
  //TODO: actually impement the sematncis
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Address);
  absState.push(result);
  return absState;
}

AbstractState&
AbsEnvStatic::invokevirtual(AbstractState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Virtual);
  return absState;
}

AbstractState&
AbsEnvStatic::invokestatic(AbstractState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Static);
  return absState;
}

AbstractState&
AbsEnvStatic::invokespecial(AbstractState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Special);
  return absState;
}

AbstractState&
AbsEnvStatic::invokedynamic(AbstractState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::ComputedVirtual);
  return absState;
}

AbstractState&
AbsEnvStatic::invokeinterface(AbstractState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Interface);
  return absState;
}

void
AbsEnvStatic::invoke(int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind) {
  auto callerResolvedMethodSymbol = this->getResolvedMethodSymbol();
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
  TR::Method *method = comp->fej9()->createMethod(comp->trMemory(), callerResolvedMethod->containingClass(), cpIndex);
  // how many pops?
  uint32_t params = method->numberOfExplicitParameters();
  int isStatic = kind == TR::MethodSymbol::Kinds::Static;
  int numberOfImplicitParameters = isStatic ? 0 : 1;
  if (numberOfImplicitParameters == 1) this->getState().pop();
  for (int i = 0; i < params; i++) {
    TR::DataType datatype = method->parmType(i);
    this->getState().pop();
    switch (datatype) {
      case TR::Double:
      case TR::Int64:
        this->getState().pop();
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
        this->getState().push(result);
        }
        break;
      case TR::Double:
      case TR::Int64:
        {
        AbsValue *result = this->getTopDataType(datatype);
        this->getState().push(result);
        AbsValue *result2 = this->getTopDataType(TR::NoType);
        this->getState().push(result2);
        }
        break;
      default:
        {
        //TODO: 
        AbsValue *result = this->getTopDataType(TR::Address);
        this->getState().push(result);
        }
        break;
  }
  
}

AbsFrame::AbsFrame(TR::Region &region, IDT::Node *node)
  : _region(region)
  , _node(node)
  , _vp(node->getValuePropagation())
  , _rms(node->getResolvedMethodSymbol())
  , _bci(node->getResolvedMethodSymbol(), static_cast<TR_ResolvedJ9Method*>(node->getResolvedMethodSymbol()->getResolvedMethod()), static_cast<TR_J9VMBase*>(TR::comp()->fe()), TR::comp())
{
}

AbsValue *AbsEnvStatic::createAbsValue(TR::VPConstraint *vp, TR::DataType dt)
  {
  return getFrame()->createAbsValue(vp, dt);
  }

AbsEnvStatic* AbsEnvStatic::enterMethod(TR::Region& region, IDT::Node* node, AbsFrame* absFrame, TR::ResolvedMethodSymbol* rms)
{
  AbsEnvStatic *absEnv = new (region) AbsEnvStatic(region, node, absFrame);
  TR_ResolvedMethod *resolvedMethod = rms->getResolvedMethod();
  const auto numberOfParameters = resolvedMethod->numberOfParameters();
  const auto numberOfExplicitParameters = resolvedMethod->numberOfExplicitParameters();
  const auto numberOfImplicitParameters = numberOfParameters - numberOfExplicitParameters;
  const auto hasImplicitParameter = numberOfImplicitParameters > 0;

  if (hasImplicitParameter)
     {
     TR_OpaqueClassBlock *implicitParameterClass = resolvedMethod->containingClass();
     AbsValue* value = AbsEnvStatic::getClassConstraint(implicitParameterClass, absFrame->getValuePropagation(), region);
     absEnv->getState().at(0, value);
     }

  TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*TR::comp());
  TR::ParameterSymbol *p = nullptr;
  for (int i = numberOfImplicitParameters; !parameterIterator->atEnd(); parameterIterator->advanceCursor(), i++)
     {
     auto parameter = parameterIterator;
     TR::DataType dataType = parameter->getDataType();
     switch (dataType) {
        case TR::Int8:
          absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::Int32));
          continue;
        break;

        case TR::Int16:
          absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::Int32));
          continue;
        break;

        case TR::Int32:
          absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::Int32));
          continue;
        break;

        case TR::Int64:
          absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::Int64));
          i = i+1;
          absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::NoType));
          continue;
        break;

        case TR::Float:
          absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::Float));
          continue;
        break;

        case TR::Double:
          absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::Double));
          i = i+1;
          absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::NoType));
        continue;
        break;

        default:
        //TODO: what about vectors and aggregates?
        break;
     }
     const bool isClass = parameter->isClass();
     if (!isClass)
       {
       absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::Address));
       continue;
       }

     TR_OpaqueClassBlock *parameterClass = parameter->getOpaqueClass();
     if (!parameterClass)
       {
       absEnv->getState().at(i, absFrame->createAbsValue(NULL, TR::Address));
       continue;
       }

      AbsValue* value = AbsEnvStatic::getClassConstraint(parameterClass, absFrame->getValuePropagation(), region);
      absEnv->getState().at(i, value);
     }
  return absEnv;
}


void AbsFrame::interpret()
{
  TR::CFG* cfg = this->getResolvedMethodSymbol()->getFlowGraph();
  TR::CFGNode *cfgNode = cfg->getStartForReverseSnapshot();
  TR::Block *startBlock = cfgNode->asBlock();

  // TODO: maybe instead of region try trCurrentStackRegion
  TR::list<OMR::Block*, TR::Region&> queue(this->getRegion());
  bool first = true;
  //TODO: get rid of TR::comp()
  for (TR::ReversePostorderSnapshotBlockIterator blockIt (startBlock, TR::comp()); blockIt.currentBlock(); ++blockIt)
     {
        OMR::Block *block = blockIt.currentBlock();
        if (first) {
           block->_absEnv = AbsEnvStatic::enterMethod(this->getRegion(), this->_node, this, this->getResolvedMethodSymbol());
           block->_absEnv->_block = block;
           first = false;
        }
        block->setVisitCount(0);
        queue.push_back(block);
     }

  while (!queue.empty()) {
    OMR::Block *block = queue.front();
    queue.pop_front();
    interpret(block);
  }
}


void AbsFrame::interpret(OMR::Block* block)
{
  int start = block->getBlockBCIndex();
  int end = start + block->getBlockSize();
  if (start < 0 || end < 1) return;
  this->factFlow(block);
  _bci.setIndex(start);
  TR::Compilation *comp = TR::comp();
  if (comp->trace(OMR::benefitInliner))
     {
     traceMsg(comp, "basic block %d start = %d end = %d\n", block->getNumber(), start, end);
     }
  for (TR_J9ByteCode bc = _bci.current(); bc != J9BCunknown && _bci.currentByteCodeIndex() < end; bc = _bci.next())
     {
     if (block->_absEnv)
     block->_absEnv->interpret(bc, _bci);
     }
}

void AbsFrame::interpret(OMR::Block* block, AbsEnvStatic *env)
{
  int start = block->getBlockBCIndex();
  int end = start + block->getBlockSize();
  if (start < 0 || end < 1) return;
  this->factFlow(block);
  _bci.setIndex(start);
  TR::Compilation *comp = TR::comp();
  if (comp->trace(OMR::benefitInliner))
     {
     traceMsg(comp, "basic block %d start = %d end = %d\n", block->getNumber(), start, end);
     }
  for (TR_J9ByteCode bc = _bci.current(); bc != J9BCunknown && _bci.currentByteCodeIndex() < end; bc = _bci.next())
     {
     env->interpret(bc, _bci);
     }
}

AbsValue *
AbsFrame::createAbsValue(TR::VPConstraint *vp, TR::DataType dt)
  {
  return new (getRegion()) AbsValue(vp, dt);
  }

AbsValue* AbsEnvStatic::getClassConstraint(TR_OpaqueClassBlock *opaqueClass, TR::ValuePropagation *vp, TR::Region &region)
  {
  if (!opaqueClass) return NULL;

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(vp, opaqueClass);
  TR::VPConstraint *classConstraint = TR::VPClass::create(vp, fixedClass, NULL, NULL, NULL, NULL);
  return new (region) AbsValue (classConstraint, TR::Address);
  }

void AbsFrame::factFlow(OMR::Block *block) {
  int start = block->getBlockBCIndex();
  if (start == 0) {
     return; // _absEnv should already be computed and stored there.
  }

  // has no predecessors (apparently can happen... maybe dead code?)
  if (block->getPredecessors().size() == 0) {
    return;
  }

  // this can happen I think if we have a loop that has some CFG inside. So its better to just return without assigning anything
  // as we should only visit if we actually have abs env to propagate
  if (block->hasOnlyOnePredecessor() && !block->getPredecessors().front()->getFrom()->asBlock()->_absEnv) {
    //TODO: put it at the end of the queue?
    return;
  }

  // should never be null if we only have one predecessor and there are no loops.
  if (block->hasOnlyOnePredecessor() && block->getPredecessors().front()->getFrom()->asBlock()->_absEnv) {
    AbsEnvStatic *absEnv = new (this->getRegion()) AbsEnvStatic(*block->getPredecessors().front()->getFrom()->asBlock()->_absEnv);
    block->_absEnv = absEnv;
    block->_absEnv->_block = block;
    return;
  }

  // multiple predecessors...
  if (block->hasAbstractInterpretedAllPredecessors()) {
     block->_absEnv = this->mergeAllPredecessors(block);
     block->_absEnv->_block = block;
     return;
  }
}

AbsEnvStatic* AbsFrame::mergeAllPredecessors(OMR::Block *block) {
  TR::CFGEdgeList &predecessors = block->getPredecessors();
  AbsEnvStatic *absEnv = nullptr;
  bool first = true;
  traceMsg(TR::comp(), "computing how merging strategy for basic block %d\n", block->getNumber());
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
        traceMsg(TR::comp(), "copy basic block %d\n", aBlock->getNumber());
        absEnv = new (this->getRegion()) AbsEnvStatic(*aBlock->_absEnv);
        absEnv->trace();
        continue;
     }
     // merge with the rest;
     traceMsg(TR::comp(), "merge with basic block %d\n", aBlock->getNumber());
     absEnv->merge(*aBlock->_absEnv);
     absEnv->trace();
  }
  return absEnv;
}

AbsOpStackStatic&
AbsEnvStatic::getStack()
{
  return _absState._stack;
}

AbsVarArrayStatic&
AbsEnvStatic::getArray()
{
  return _absState._array;
}

TR::Region&
AbstractState::getRegion()
{
  return _region;
}

