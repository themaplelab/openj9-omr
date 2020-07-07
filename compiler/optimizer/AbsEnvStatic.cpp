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
#include "compiler/optimizer/MethodSummary.hpp"



TR::Region&
AbsEnvStatic::getRegion() 
{
  TR::Region & region = this->_absFrame->getRegion();
  TR_ASSERT(&region != NULL, "region is null");
  return region;
}

TR::ResolvedMethodSymbol*
AbsEnvStatic::getResolvedMethodSymbol() 
{
  return this->_absFrame->getResolvedMethodSymbol();
}

TR::ResolvedMethodSymbol*
AbsFrame::getResolvedMethodSymbol() 
{
  return this->_rms;
}

TR::ValuePropagation *
AbsEnvStatic::getVP() 
{
  return this->_absFrame->_vp;
}

IDTNode*
AbsEnvStatic::getNode() 
{
   return this->_absFrame->_node;
}

AbsEnvStatic::AbsEnvStatic(TR::Region &region, IDTNode *node, AbsFrame* absFrame) :
  
  _absState(region, 1,1),
  _absFrame(absFrame),
  _block(NULL)
{
   TR_ASSERT(&(region) != NULL, "we have a null region\n");
}

void
AbsEnvStatic::zeroOut(AbsEnvStatic *other)
{
  this->trace("tracing this block\n");
  AbsState &absState = this->getState();
  TR::Region &region = other->getRegion();
  size_t stackSize = absState.getStackSize();
  AbsValue *array[stackSize];
  for (size_t i = 0; i < stackSize; i++)
     {
     array[stackSize - i - 1] = absState.pop();
     }
  
  for (size_t i = 0; i < stackSize; i++)
     {
     AbsValue *a = array[i];
     other->getState().push(this->getTopDataType(a ? array[i]->getDataType() : TR::Address));
     absState.push(array[i]);
     }

  size_t arraySize = absState.getArraySize();
  for (size_t i = 0; i < arraySize; i++)
     {
     AbsValue *a = absState.at(i);
     AbsValue *value1 = this->getTopDataType(a ? absState.at(i)->getDataType() : TR::Address);
     other->getState().set(i, value1);
     }
  other->trace("tracing other block\n");
}

AbsEnvStatic::AbsEnvStatic(AbsEnvStatic &other) :
  _absState(&other._absState),
  _absFrame(other._absFrame),
  _block(other._block)
{
}


void
AbsEnvStatic::merge(AbsEnvStatic &other)
{
   //getState().merge(other._absState, this->getVP());
}

void
AbsEnvStatic::pushNull()
  {
  this->getState().push(new (this->getRegion()) AbsValue(NULL, TR::Address));
  }

void
AbsEnvStatic::pushConstInt(AbsState& absState, int n)
  {
  TR::VPIntConst *intConst = TR::VPIntConst::create(this->getVP(), n);
  absState.push(new (this->getRegion()) AbsValue(intConst, TR::Int32));
  }

AbsValue*
AbsEnvStatic::getTopDataType(TR::DataType dt)
  {
  // TODO: Don't allocate new memory and always return the same set of values
  TR::Region &region = this->getRegion();
  return new (getRegion()) AbsValue(NULL, dt);
  }


AbsState&
AbsEnvStatic::aload0getfield(AbsState &absState, int i) {
  this->aload0(absState);
  return absState;
}

void
AbsEnvStatic::interpret(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci)
  {

  TR::Compilation *comp = TR::comp();
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
  {
    traceMsg(comp, "AbsEnvStatic: interpret\n");
    bci.printByteCode();
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
     case J9BCfconst2: this->fconst2(this->_absState); break;
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
     case J9BCinvokedynamic: TR_ASSERT_FATAL(false, "no inoke dynamic"); break;
     case J9BCinvokeinterface: this->invokeinterface(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokespecial: this->invokespecial(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokestatic: this->invokestatic(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes()); break;
     case J9BCinvokevirtual: this->invokevirtual(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes());break;
     case J9BCinvokespecialsplit: this->invokespecial(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes() | J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG); break;
     case J9BCinvokestaticsplit: this->invokestatic(this->_absState, bci.currentByteCodeIndex(), bci.next2Bytes() | J9_STATIC_SPLIT_TABLE_INDEX_FLAG); break;
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
  if (!TR::comp()->trace(OMR::benefitInliner)) return;
  if (methodName) traceMsg(TR::comp(), "method %s\n", methodName);
  this->getState().trace(this->getVP());
  }

AbsState&
AbsEnvStatic::multianewarray(AbsState & absState, int cpIndex, int dimensions)
{
  for (int i = 0; i < dimensions; i++)
    {
    absState.pop();
    }
  this->pushNull();
  return absState;
}

AbsState&
AbsEnvStatic::caload(AbsState& absState)
{
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  AbsValue *value1 = this->getTopDataType(TR::Int32);
  absState.push(value1);
  return absState;
}


AbsState&
AbsEnvStatic::faload(AbsState & absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  AbsValue *value1 = this->getTopDataType(TR::Float);
  absState.push(value1);
  return absState;
  }

AbsState&
AbsEnvStatic::iaload(AbsState & absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  AbsValue *value1 = this->getTopDataType(TR::Int32);
  absState.push(value1);
  return absState;
  }

AbsState&
AbsEnvStatic::saload(AbsState & absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  AbsValue *value1 = this->getTopDataType(TR::Int32);
  absState.push(value1);
  return absState;
  }

AbsState&
AbsEnvStatic::aaload(AbsState & absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  if (!arrayRef || !arrayRef->getConstraint()) {
    AbsValue *value1 = this->getTopDataType(TR::Address);
    absState.push(value1);
    return absState;
  }

  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
        traceMsg(TR::comp(), "\n%s:%d:%s aaload... look here! \n", __FILE__, __LINE__, __func__);
  }

  arrayRef->print(this->getVP());
  absState.push(arrayRef);
  return absState;
  }

AbsState&
AbsEnvStatic::laload(AbsState &absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  AbsValue *value1 = this->getTopDataType(TR::Int64);
  AbsValue *value2 = this->getTopDataType(TR::NoType);
  absState.push(value1);
  absState.push(value2);
  return absState;
  }

AbsState&
AbsEnvStatic::daload(AbsState &absState)
  {
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  AbsValue *value1 = this->getTopDataType(TR::Double);
  AbsValue *value2 = this->getTopDataType(TR::NoType);
  absState.push(value1);
  absState.push(value2);
  return absState;
  }

AbsState&
AbsEnvStatic::castore(AbsState &absState)
{
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
}

AbsState&
AbsEnvStatic::dastore(AbsState &absState)
{
  //TODO:
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
}

AbsState&
AbsEnvStatic::fastore(AbsState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbsState&
AbsEnvStatic::iastore(AbsState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbsState&
AbsEnvStatic::lastore(AbsState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbsState&
AbsEnvStatic::sastore(AbsState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbsState&
AbsEnvStatic::aastore(AbsState &absState)
  {
  //TODO:
  AbsValue *value = absState.pop();
  AbsValue *index = absState.pop();
  AbsValue *arrayRef = absState.pop();
  // if we have arrayRef, we know arrayRef is not null
  // if we have index and arrayRef length, we may know if safe to remove bounds checking
  return absState;
  }

AbsState&
AbsEnvStatic::aconstnull(AbsState &absState) {
  TR::VPConstraint *null = TR::VPNullObject::create(this->getVP());
  AbsValue *absValue = new (getRegion()) AbsValue(null, TR::Address);
  absState.push(absValue);
  return absState;
}

AbsState&
AbsEnvStatic::aload(AbsState &absState, int n) {
  this->aloadn(absState, n);
  return absState;
}

AbsState&
AbsEnvStatic::aload0(AbsState &absState) {
  this->aloadn(absState, 0);
  return absState;
}

AbsState&
AbsEnvStatic::aload1(AbsState &absState) {
  this->aloadn(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::aload2(AbsState &absState) {
  this->aloadn(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::aload3(AbsState &absState) {
  this->aloadn(absState, 3);
  return absState;
}

void
AbsEnvStatic::aloadn(AbsState& absState, int n) {
  AbsValue *constraint = absState.at(n);
  absState.push(constraint);
}

AbsState&
AbsEnvStatic::astore(AbsState &absState, int varIndex) {
  AbsValue* constraint = absState.pop();
  absState.set(varIndex, constraint);
  return absState;
}

AbsState&
AbsEnvStatic::astore0(AbsState &absState) {
  this->astore(absState, 0);
  return absState;
}

AbsState&
AbsEnvStatic::astore1(AbsState &absState) {
  this->astore(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::astore2(AbsState &absState) {
  this->astore(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::astore3(AbsState &absState) {
  this->astore(absState, 3);
  return absState;
}

AbsState&
AbsEnvStatic::bipush(AbsState &absState, int byte) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->getVP(), byte);
  //TODO: should I use TR::Int32 or something else?
  AbsValue *absValue = new (getRegion()) AbsValue(data, TR::Int32);
  absState.push(absValue);
  return absState;
}

AbsState&
AbsEnvStatic::bastore(AbsState &absState) {
  this->aastore(absState);
  return absState;
}

AbsState&
AbsEnvStatic::baload(AbsState &absState) {
  this->aaload(absState);
  return absState;
}

AbsState&
AbsEnvStatic::checkcast(AbsState &absState, int cpIndex, int bytecodeIndex) {
  AbsValue *absValue = absState.pop();
  if (!absValue)
    {
    AbsValue *value1 = this->getTopDataType(TR::Address);
    absState.push(absValue);
    return absState;
    }
  TR::VPConstraint *objectRef = absValue->getConstraint();
  if (!objectRef)
    {
    absState.push(absValue);
    return absState;
    }

  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  // TODO: can we do this some other way? I don't want to pass TR::comp();
  TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
  if (!type)
  {
  absState.push(absValue);
  return absState;
  }
  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->getVP(), type);
  if (!fixedClass)
  {
  absState.push(absValue);
  return absState;
  }
  TR::VPConstraint *typeConstraint= TR::VPClass::create(this->getVP(), fixedClass, NULL, NULL, NULL, NULL);
  if (!typeConstraint)
  {
  absState.push(absValue);
  return absState;
  }
  // If objectRef is not assignable to a class of type type then throw an exception.

  absState.push(new (getRegion()) AbsValue(typeConstraint, TR::Address));
  return absState;
}

AbsState&
AbsEnvStatic::dup(AbsState &absState) {
  AbsValue *value = absState.pop();
  absState.push(value);
  absState.push(value); 
  return absState;
}

AbsState&
AbsEnvStatic::dupx1(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  absState.push(value1);
  absState.push(value2);
  absState.push(value1);
  return absState;
}

AbsState&
AbsEnvStatic::dupx2(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  absState.push(value1);
  absState.push(value3);
  absState.push(value2);
  absState.push(value1);
  return absState;
}

AbsState&
AbsEnvStatic::dup2(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  absState.push(value2);
  absState.push(value1);
  absState.push(value2);
  absState.push(value1);
  return absState;
}

AbsState&
AbsEnvStatic::dup2x1(AbsState &absState) {
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

AbsState&
AbsEnvStatic::dup2x2(AbsState &absState) {
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

AbsState&
AbsEnvStatic::_goto(AbsState &absState, int branch)
{
  return absState;
}


AbsState&
AbsEnvStatic::_gotow(AbsState &absState, int branch)
{
  return absState;
}

AbsState&
AbsEnvStatic::getstatic(AbsState &absState, int cpIndex) {
   void* staticAddress;
   TR::DataType type = TR::NoType;
   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   bool isResolved = this->getFrame()->_bci.method()->staticAttributes(TR::comp(),
         cpIndex,
         &staticAddress,
         &type,
         &isVolatile,
         &isFinal,
         &isPrivate, 
         false, // isStore
         &isUnresolvedInVP,
         false); //needsAOTValidation

   if (!isResolved || isUnresolvedInVP || type != TR::Address) {
     if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
        traceMsg(TR::comp(), "\n%s:%d:%s Not an address \n", __FILE__, __LINE__, __func__);
     }
     AbsValue *value1 = this->getTopDataType(type);
     absState.push(value1);
     if (!value1->isType2()) { return absState; }
     AbsValue *value2= this->getTopDataType(TR::NoType);
     absState.push(value2);
     return absState;
   }


   // This is definitely an address...
   TR::SymbolReference * symRef = TR::comp()->getSymRefTab()->findStaticSymbol(this->getFrame()->_bci.method(), cpIndex, type);
   //if (!symRef || symRef->isUnresolved()) {
     if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
        traceMsg(TR::comp(), "\n%s:%d:%s symref not found\n", __FILE__, __LINE__, __func__);
     }
     AbsValue *value1 = this->getTopDataType(type);
     absState.push(value1);
     return absState;
   //}

   if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
        traceMsg(TR::comp(), "\n%s:%d:%s symref is resolved...\n", __FILE__, __LINE__, __func__);
   }

   TR_OpaqueClassBlock * feClass = this->getFrame()->_bci.method()->classOfStatic(cpIndex);
   TR::VPClassType *constraint = TR::VPResolvedClass::create(this->getVP(), feClass);
   TR::VPConstraint *classConstraint = TR::VPClass::create(this->getVP(), constraint, NULL, NULL, NULL, NULL);
   AbsValue *result = new (getRegion()) AbsValue(classConstraint, TR::Address);
   absState.push(result);
   return absState;

}


AbsState&
AbsEnvStatic::getfield(AbsState &absState, int cpIndex) {
   AbsValue *objectref = absState.pop();
   uint32_t fieldOffset;
   TR::DataType type = TR::NoType;
   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   bool isResolved = this->getFrame()->_bci.method()->fieldAttributes(TR::comp(),
         cpIndex,
         &fieldOffset,
         &type,
         &isVolatile,
         &isFinal,
         &isPrivate, 
         false, // isStore
         &isUnresolvedInVP,
         false); //needsAOTValidation
   /*if (type == TR::Address) {
     TR_ResolvedMethod *method = this->getFrame()->_bci.method();
     TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
     AbsValue* value = AbsEnvStatic::getClassConstraint(type, this->getFrame()->getValuePropagation(), this->getRegion());
     absState.push(value);
     return absState;
   }*/
   AbsValue *value1 = this->getTopDataType(type);
   absState.push(value1);
   if (!value1->isType2()) { return absState; }
   AbsValue *value2= this->getTopDataType(TR::NoType);
   absState.push(value2);
  return absState;
   
}

AbsState&
AbsEnvStatic::iand(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int result = value1->getConstraint()->asIntConst()->getLow() & value2->getConstraint()->asIntConst()->getLow();
  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::instanceof(AbsState &absState, int cpIndex, int byteCodeIndex) {
  AbsValue *objectRef = absState.pop();
  if (!objectRef->getConstraint())
     {
     absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), 0), TR::Int32));
     return absState;
     }

  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  TR_OpaqueClassBlock *block = method->getClassFromConstantPool(TR::comp(), cpIndex);
  if (!block)
     {
     absState.push(new (getRegion()) AbsValue(TR::VPIntRange::create(this->getVP(), 0, 1), TR::Int32));
     return absState;
     }

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->getVP(), block);
  TR::VPNonNullObject *nonnull = TR::VPNonNullObject::create(this->getFrame()->getValuePropagation());
  TR::VPConstraint *typeConstraint= TR::VPClass::create(this->getVP(), fixedClass, nonnull, NULL, NULL, NULL);
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
  if (objectRef->getConstraint()->asClass())
     {
     /*
      * If T is a class type, then S must be the same class as T, or S
      * must be a subclass of T;
      */
     if(typeConstraint->intersect(objectRef->getConstraint(), this->getVP()))
        absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), 1), TR::Int32));
     else
        absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), 0), TR::Int32));
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

  absState.push(new (getRegion()) AbsValue(TR::VPIntRange::create(this->getVP(), 0, 1), TR::Int32));
   return absState;
}

AbsState&
AbsEnvStatic::ior(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int result = value1->getConstraint()->asIntConst()->getLow() | value2->getConstraint()->asIntConst()->getLow();
  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::ixor(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int result = value1->getConstraint()->asIntConst()->getLow() ^ value2->getConstraint()->asIntConst()->getLow();
  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::irem(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->getConstraint()->asIntConst()->getLow();
  int int2 = value2->getConstraint()->asIntConst()->getLow();
  int result = int1 - (int1/ int2) * int2;
  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::ishl(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->getConstraint()->asIntConst()->getLow();
  int int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;
  int result = int1 << int2;
  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::ishr(AbsState &absState) {

  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->getConstraint()->asIntConst()->getLow();
  int int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;

  //arithmetic shift.
  int result = int1 >> int2;

  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::iushr(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->getConstraint()->asIntConst()->getLow();
  int int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;
  int result = int1 >> int2;
  //logical shift, gets rid of the sign.
  result &= 0x7FFFFFFF;
  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::idiv(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->getConstraint()->asIntConst()->getLow();
  int int2 = value2->getConstraint()->asIntConst()->getLow();
  if (int2 == 0)
    {
     // this should throw an exception.
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
    }
  int result = int1 / int2;
  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::imul(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }
  bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  int int1 = value1->getConstraint()->asIntConst()->getLow();
  int int2 = value2->getConstraint()->asIntConst()->getLow();
  int result = int1 * int2;
  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::ineg(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  bool allConstants = value1->getConstraint() && value1->getConstraint()->asIntConst();
  if (!allConstants)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  //TODO: more precision for ranges, subtract VPIntConst 0 from value1
  int int1 = value1->getConstraint()->asIntConst()->getLow();
  int result = -int1;
  absState.push(new (getRegion()) AbsValue(TR::VPIntConst::create(this->getVP(), result), TR::Int32));
  return absState;
}

AbsState&
AbsEnvStatic::iconstm1(AbsState &absState) {
  this->iconst(absState, -1);
  return absState;
}

AbsState&
AbsEnvStatic::iconst0(AbsState &absState) {
  this->iconst(absState, 0);
  return absState;
}

AbsState&
AbsEnvStatic::iconst1(AbsState &absState) {
  this->iconst(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::iconst2(AbsState &absState) {
  this->iconst(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::iconst3(AbsState &absState) {
  this->iconst(absState, 3);
  return absState;
}

AbsState&
AbsEnvStatic::iconst4(AbsState &absState) {
  this->iconst(absState, 4);
  return absState;
}

AbsState&
AbsEnvStatic::iconst5(AbsState &absState) {
  this->iconst(absState, 5);
  return absState;
}

void
AbsEnvStatic::iconst(AbsState &absState, int n) {
  this->pushConstInt(absState, n);
}

AbsState&
AbsEnvStatic::ifeq(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ifne(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::iflt(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ifle(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ifgt(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ifge(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ifnull(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ifnonnull(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ificmpge(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ificmpeq(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ificmpne(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ificmplt(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ificmpgt(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ificmple(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ifacmpeq(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::ifacmpne(AbsState& absState, int branchOffset, int bytecodeIndex) {
  absState.pop();
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::iload(AbsState& absState, int n) {
  this->aloadn(absState, n);
  return absState;
}

AbsState&
AbsEnvStatic::iload0(AbsState& absState) {
  this->iload(absState, 0);
  return absState;
}

AbsState&
AbsEnvStatic::iload1(AbsState& absState) {
  this->iload(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::iload2(AbsState& absState) {
  this->iload(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::iload3(AbsState& absState) {
  this->iload(absState, 3);
  return absState;
}

AbsState&
AbsEnvStatic::istore(AbsState& absState, int n) {
  absState.set(n, absState.pop());
  return absState;
}

AbsState&
AbsEnvStatic::istore0(AbsState& absState) {
  absState.set(0, absState.pop());
  return absState;
}

AbsState&
AbsEnvStatic::istore1(AbsState& absState) {
  absState.set(1, absState.pop());
  return absState;
}

AbsState&
AbsEnvStatic::istore2(AbsState& absState) {
  absState.set(2, absState.pop());
  return absState;
}

AbsState&
AbsEnvStatic::istore3(AbsState& absState) {
  absState.set(3, absState.pop());
  return absState;
}

AbsState&
AbsEnvStatic::isub(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
    {
    AbsValue *result = this->getTopDataType(TR::Int32);
    absState.push(result);
    return absState;
    }

  TR::VPConstraint *result_vp = value1->getConstraint()->subtract(value2->getConstraint(), value2->getDataType(), this->getVP());
  AbsValue *result = new (getRegion()) AbsValue(result_vp, value2->getDataType());
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::iadd(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  bool nonnull = value1->getConstraint() && value2->getConstraint();
  if (!nonnull)
    {
    AbsValue *result = this->getTopDataType(TR::Int32);
    absState.push(result);
    return absState;
    }

  TR::VPConstraint *result_vp = value1->getConstraint()->add(value2->getConstraint(), value2->getDataType(), this->getVP());
  AbsValue *result = new (getRegion()) AbsValue(result_vp, value2->getDataType());
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::i2d(AbsState& absState) {
  AbsValue *value = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Double);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::i2f(AbsState& absState) {
  AbsValue *value = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::i2l(AbsState& absState) {
  AbsValue *value = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int64);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::i2s(AbsState& absState) {
  //empty?
  return absState;
}

AbsState&
AbsEnvStatic::i2c(AbsState& absState) {
  //empty?
  return absState;
}

AbsState&
AbsEnvStatic::i2b(AbsState& absState) {
  // empty?
  return absState;
}

AbsState&
AbsEnvStatic::dadd(AbsState& absState) {
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

AbsState&
AbsEnvStatic::dsub(AbsState& absState) {
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

AbsState&
AbsEnvStatic::fsub(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::fadd(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::ladd(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue *value4 = absState.pop();
  bool nonnull = value2->getConstraint() && value4->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
    return absState;
    }

  TR::VPConstraint *result_vp = value2->getConstraint()->add(value4->getConstraint(), value4->getDataType(), this->getVP());
  AbsValue *result = new (getRegion()) AbsValue(result_vp, TR::Int64);
  absState.push(result);
  this->getTopDataType(TR::NoType);
  return absState;
}

AbsState&
AbsEnvStatic::lsub(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue *value4 = absState.pop();
  bool nonnull = value2->getConstraint() && value4->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
    return absState;
    }

  TR::VPConstraint *result_vp = value2->getConstraint()->subtract(value4->getConstraint(), value4->getDataType(), this->getVP());
  AbsValue *result = new (getRegion()) AbsValue(result_vp, TR::Int64);
  absState.push(result);
  this->getTopDataType(TR::NoType);
  return absState;
}

AbsState&
AbsEnvStatic::l2i(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  bool nonnull = value2->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int32);
    absState.push(result1);
    return absState;
    }

  bool canCompute = value2->getConstraint() && value2->getConstraint()->asLongConstraint();
  if (!canCompute)
     {
     AbsValue *result = this->getTopDataType(TR::Int32);
     absState.push(result);
     return absState;
     }

  TR::VPConstraint *intConst = TR::VPIntRange::create(this->getVP(), value2->getConstraint()->asLongConstraint()->getLow(), value2->getConstraint()->asLongConstraint()->getHigh());
  AbsValue *result = new (getRegion()) AbsValue(intConst, TR::Int32);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::land(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue *value4 = absState.pop();
  bool nonnull = value2->getConstraint() && value4->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
    return absState;
    }
  bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  int result = value2->getConstraint()->asLongConst()->getLow() & value4->getConstraint()->asLongConst()->getLow();
  absState.push(new (getRegion()) AbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::ldiv(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->getConstraint() && value4->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value2->getConstraint()->asLongConst()->getLow();
  long int2 = value4->getConstraint()->asLongConst()->getLow();
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
  absState.push(new (getRegion()) AbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lmul(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->getConstraint() && value4->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value1->getConstraint()->asLongConst()->getLow();
  long int2 = value2->getConstraint()->asLongConst()->getLow();
  long result = int1 * int2;
  absState.push(new (getRegion()) AbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lookupswitch(AbsState& absState)
{
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::tableswitch(AbsState& absState)
{
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::lneg(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  bool nonnull = value2->getConstraint();
  if (!nonnull)
    {
    absState.push(value2);
    absState.push(value1);
     return absState;
    }
  bool allConstants = value2->getConstraint()->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value1->getConstraint()->asLongConst()->getLow();
  long result = -int1;
  absState.push(new (getRegion()) AbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lor(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->getConstraint() && value4->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->getConstraint()->asLongConst() && value2->getConstraint()->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long result = value1->getConstraint()->asLongConst()->getLow() | value2->getConstraint()->asLongConst()->getLow();
  absState.push(new (getRegion()) AbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lrem(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->getConstraint() && value4->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value2->getConstraint()->asLongConst()->getLow();
  long int2 = value4->getConstraint()->asLongConst()->getLow();
  long result = int1 - (int1/ int2) * int2;
  absState.push(new (getRegion()) AbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lshl(AbsState& absState) {
  AbsValue *value2 = absState.pop(); // int
  AbsValue* value0 = absState.pop(); // nothing
  AbsValue* value1 = absState.pop(); // long
  bool nonnull = value2->getConstraint() && value1->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->getConstraint()->asIntConst() && value1->getConstraint()->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value1->getConstraint()->asLongConst()->getLow();
  long int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;
  long result = int1 << int2;
  absState.push(new (getRegion()) AbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lshr(AbsState& absState) {
  AbsValue *value2 = absState.pop(); // int
  AbsValue* value0 = absState.pop(); // nothing
  AbsValue* value1 = absState.pop(); // long
  bool nonnull = value2->getConstraint() && value1->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
     return absState;
    }
  bool allConstants = value2->getConstraint()->asIntConst() && value1->getConstraint()->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long int1 = value1->getConstraint()->asLongConst()->getLow();
  long int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;
  long result = int1 >> int2;
  absState.push(new (getRegion()) AbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lushr(AbsState& absState) {
  this->lshr(absState);
  return absState;
}

AbsState&
AbsEnvStatic::lxor(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue* value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  bool nonnull = value2->getConstraint() && value4->getConstraint();
  if (!nonnull)
    {
    AbsValue *result1 = this->getTopDataType(TR::Int64);
    AbsValue *result2 = this->getTopDataType(TR::NoType);
    absState.push(result1);
    absState.push(result2);
    return absState;
    }
  bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
  if (!allConstants)
     {
     AbsValue *result1 = this->getTopDataType(TR::Int64);
     AbsValue *result2 = this->getTopDataType(TR::NoType);
     absState.push(result1);
     absState.push(result2);
     return absState;
     }

  long result = value1->getConstraint()->asLongConst()->getLow() ^ value2->getConstraint()->asLongConst()->getLow();
  absState.push(new (getRegion()) AbsValue(TR::VPLongConst::create(this->getVP(), result), TR::Int64));
  return absState;
}

AbsState&
AbsEnvStatic::l2d(AbsState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Double);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::l2f(AbsState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::d2f(AbsState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Float);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::f2d(AbsState& absState) {
  absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::f2i(AbsState& absState) {
  absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Int32);
  absState.push(result1);
  return absState;
}

AbsState&
AbsEnvStatic::f2l(AbsState& absState) {
  absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Int64);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::d2i(AbsState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::d2l(AbsState& absState) {
  absState.pop();
  absState.pop();
  AbsValue *result1 = this->getTopDataType(TR::Int64);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lload(AbsState& absState, int n) {
  aload(absState, n);
  aload(absState, n + 1);
  return absState;
}

AbsState&
AbsEnvStatic::lload0(AbsState& absState) {
  aload(absState, 0);
  aload(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::lload1(AbsState &absState) {
  aload(absState, 1);
  aload(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::lload2(AbsState &absState) {
  aload(absState, 2);
  aload(absState, 3);
  return absState;
}

AbsState&
AbsEnvStatic::lload3(AbsState &absState) {
  aload(absState, 3);
  aload(absState, 4);
  return absState;
}

AbsState&
AbsEnvStatic::dconst0(AbsState &absState) {
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::dconst1(AbsState &absState) {
  AbsValue *result1 = this->getTopDataType(TR::Double);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result1);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::fconst0(AbsState &absState) {
  AbsValue *result1 = this->getTopDataType(TR::Float);
  absState.push(result1);
  return absState;
}

AbsState&
AbsEnvStatic::fconst1(AbsState &absState) {
  AbsValue *result1 = this->getTopDataType(TR::Float);
  absState.push(result1);
  return absState;
}

AbsState&
AbsEnvStatic::fconst2(AbsState &absState) {
  AbsValue *result1 = this->getTopDataType(TR::Float);
  absState.push(result1);
  return absState;
}

AbsState&
AbsEnvStatic::dload(AbsState &absState, int n) {
  aload(absState, n);
  aload(absState, n + 1);
  return absState;
}

AbsState&
AbsEnvStatic::dload0(AbsState &absState) {
  aload(absState, 0);
  aload(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::dload1(AbsState &absState) {
  aload(absState, 1);
  aload(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::dload2(AbsState &absState) {
  aload(absState, 2);
  aload(absState, 3);
  return absState;
}

AbsState&
AbsEnvStatic::dload3(AbsState &absState) {
  aload(absState, 3);
  aload(absState, 4);
  return absState;
}

AbsState&
AbsEnvStatic::lstorew(AbsState &absState, int n)
{
  lstore(absState, n);
  return absState;
}

AbsState&
AbsEnvStatic::lstore0(AbsState &absState)
{
  lstore(absState, 0);
  return absState;
}

AbsState&
AbsEnvStatic::lstore1(AbsState &absState)
{
  lstore(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::lstore2(AbsState &absState)
{
  lstore(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::lstore3(AbsState &absState)
{
  lstore(absState, 3);
  return absState;
}


AbsState&
AbsEnvStatic::lstore(AbsState &absState, int n) {
  AbsValue *top = absState.pop();
  AbsValue *bottom = absState.pop();
  absState.set(n, bottom);
  absState.set(n + 1, top);
  return absState;
}

AbsState&
AbsEnvStatic::dstore(AbsState &absState, int n) {
  AbsValue *top = absState.pop();
  AbsValue *bottom = absState.pop();
  absState.set(n, bottom);
  absState.set(n + 1, top);
  return absState;
}

AbsState&
AbsEnvStatic::dstore0(AbsState& absState) {
  this->dstore(absState, 0);
  return absState;
}

AbsState&
AbsEnvStatic::dstore1(AbsState& absState) {
  this->dstore(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::dstore2(AbsState& absState) {
  this->dstore(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::dstore3(AbsState& absState) {
  this->dstore(absState, 3);
  return absState;
}

AbsState&
AbsEnvStatic::lconst0(AbsState& absState) {
  AbsValue *result = this->getTopDataType(TR::Int64);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lconst1(AbsState& absState) {
  AbsValue *result = this->getTopDataType(TR::Int64);
  absState.push(result);
  AbsValue *result2 = this->getTopDataType(TR::NoType);
  absState.push(result2);
  return absState;
}

AbsState&
AbsEnvStatic::lcmp(AbsState& absState) {
  absState.pop();
  absState.pop();
  absState.pop();
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::pop2(AbsState& absState) {
  absState.pop();
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::fload(AbsState& absState, int n) {
  aload(absState, n);
  return absState;
}

AbsState&
AbsEnvStatic::fload0(AbsState& absState) {
  this->fload(absState, 0);
  return absState;
}

AbsState&
AbsEnvStatic::fload1(AbsState& absState) {
  this->fload(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::fload2(AbsState& absState) {
  this->fload(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::fload3(AbsState& absState) {
  this->fload(absState, 3);
  return absState;
}

AbsState&
AbsEnvStatic::swap(AbsState& absState) {
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  absState.push(value1);
  absState.push(value2);
  return absState;
}

AbsState&
AbsEnvStatic::fstore(AbsState& absState, int n) {
  absState.set(n, absState.pop());
  return absState;
}


AbsState&
AbsEnvStatic::fstore0(AbsState& absState) {
  this->fstore(absState, 0);
  return absState;
}

AbsState&
AbsEnvStatic::fstore1(AbsState& absState) {
  this->fstore(absState, 1);
  return absState;
}

AbsState&
AbsEnvStatic::fstore2(AbsState& absState) {
  this->fstore(absState, 2);
  return absState;
}

AbsState&
AbsEnvStatic::fstore3(AbsState &absState) {
  this->fstore(absState, 3);
  return absState;
}

AbsState&
AbsEnvStatic::fmul(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::dmul(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::dneg(AbsState &absState)
{
  return absState;
}

AbsState&
AbsEnvStatic::dcmpl(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::dcmpg(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *value3 = absState.pop();
  AbsValue* value4 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::fcmpg(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::fcmpl(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::ddiv(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::fdiv(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::drem(AbsState &absState) {
  AbsValue *value1 = absState.pop();
  AbsValue* value2 = absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::fneg(AbsState &absState)
{
  return absState;
}

AbsState&
AbsEnvStatic::freturn(AbsState &absState)
{
  return absState;
}

AbsState&
AbsEnvStatic::frem(AbsState &absState) {
  AbsValue *value = absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::sipush(AbsState &absState, int16_t _short) {
  TR::VPShortConst *data = TR::VPShortConst::create(this->getVP(), _short);
  AbsValue *result = new (getRegion()) AbsValue(data, TR::Int16);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::iinc(AbsState& absState, int index, int incval) {
  AbsValue *value1 = absState.at(index);
  TR::VPIntConstraint *value = value1->getConstraint() ? value1->getConstraint()->asIntConstraint() : nullptr;
  if (!value)
    {
    return absState;
    }

  TR::VPIntConst *inc = TR::VPIntConst::create(this->getVP(), incval);
  TR::VPConstraint *result = value->add(inc, TR::Int32, this->getVP());
  AbsValue *result2 = new (getRegion()) AbsValue(result, TR::Int32);
  absState.set(index, result2);
  return absState;
}

AbsState&
AbsEnvStatic::putfield(AbsState& absState) {
  // WONTFIX we do not model the heap
  AbsValue *value1 = absState.pop();
  AbsValue *value2 = absState.pop();
  if(value2->isType2()) {
    AbsValue *value3 = absState.pop();
  }
  return absState;
}

AbsState&
AbsEnvStatic::putstatic(AbsState& absState) {
  // WONTFIX we do not model the heap
  AbsValue *value1 = absState.pop();
  if (value1->getDataType() == TR::NoType) { // category type 2
    AbsValue *value2 = absState.pop();
  }
  return absState;
}

void
AbsEnvStatic::ldcInt32(int cpIndex) {
  auto value = this->getFrame()->_bci.method()->intConstant(cpIndex);
  TR::VPIntConst *constraint = TR::VPIntConst::create(this->getVP(), value);
  AbsValue *result = new (getRegion()) AbsValue(constraint, TR::Int32);
  this->getState().push(result);
}

void
AbsEnvStatic::ldcInt64(int cpIndex) {
  auto value = this->getFrame()->_bci.method()->intConstant(cpIndex);
   TR::VPLongConst *constraint = TR::VPLongConst::create(this->getVP(), value);
   AbsValue *result = new (getRegion()) AbsValue(constraint, TR::Int64);
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
  bool isString = this->getFrame()->_bci.method()->isStringConstant(cpIndex);
  if (isString) { ldcString(cpIndex); return; }
  //TODO: non string case
  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
  AbsValue* value = AbsEnvStatic::getClassConstraint(type, this->getFrame()->getValuePropagation(), this->getRegion(), NULL, NULL);
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
        traceMsg(TR::comp(), "\n%s:%d:%s ldcAddress... look here! \n", __FILE__, __LINE__, __func__);
        if(value) value->print(this->getFrame()->getValuePropagation());
  }
  this->getState().push(value);
}

void
AbsEnvStatic::ldcString(int cpIndex) {
   // TODO: we might need the resolved method symbol here
   // TODO: aAbsState& _rms    
   TR_ResolvedMethod *callerResolvedMethod = this->getFrame()->_bci.method();
   auto callerResolvedMethodSymbol = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), callerResolvedMethod, TR::comp());
   TR::SymbolReference *symRef = TR::comp()->getSymRefTab()->findOrCreateStringSymbol(callerResolvedMethodSymbol, cpIndex);
   if (symRef->isUnresolved())
        {
        TR_ResolvedMethod *method = this->getFrame()->_bci.method();
        TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
        AbsValue* value = AbsEnvStatic::getClassConstraint(type, this->getFrame()->getValuePropagation(), this->getRegion(), NULL, NULL);
        this->getState().push(value);
        return;
        }
   TR::VPConstraint *constraint = TR::VPConstString::create(this->getVP(), symRef);
   AbsValue *result = new (getRegion()) AbsValue(constraint, TR::Address);
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
        traceMsg(TR::comp(), "\n%s:%d:%s ldcStringSuccess!... look here! \n", __FILE__, __LINE__, __func__);
        if(result) result->print(this->getFrame()->getValuePropagation());
  }
   this->getState().push(result);
}

AbsState&
AbsEnvStatic::ldcw(AbsState &absState, int cpIndex)
{
  ldc(absState, cpIndex);
  return absState;
}

AbsState&
AbsEnvStatic::ldc(AbsState& absState, int cpIndex) {
   TR::DataType datatype = this->getFrame()->_bci.method()->getLDCType(cpIndex);
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

AbsState&
AbsEnvStatic::monitorenter(AbsState& absState) {
  // TODO: possible optimization
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::monitorexit(AbsState& absState) {
  // TODO: possible optimization
  absState.pop();
  return absState;
}

AbsState&
AbsEnvStatic::areturn(AbsState& absState)
{
  return absState;
}

AbsState&
AbsEnvStatic::dreturn(AbsState& absState)
{
  return absState;
}

AbsState&
AbsEnvStatic::athrow(AbsState& absState)
{
  return absState;
}

AbsState&
AbsEnvStatic::anewarray(AbsState& absState, int cpIndex) {
  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
  TR::VPNonNullObject *nonnull = TR::VPNonNullObject::create(this->getFrame()->getValuePropagation());
  AbsValue *count = absState.pop();

  if (count->getConstraint() && count->getConstraint()->asIntConstraint()) {
    TR::VPArrayInfo *info = TR::VPArrayInfo::create(this->getFrame()->getValuePropagation(),  ((TR::VPIntConstraint*)count->getConstraint())->getLow(), ((TR::VPIntConstraint*)count->getConstraint())->getHigh(), 4);
    AbsValue* value = AbsEnvStatic::getClassConstraint(type, this->getFrame()->getValuePropagation(), this->getRegion(), nonnull, info);
    if(value) value->print(this->getFrame()->getValuePropagation());
    absState.push(value);
    return absState;
  }

    TR::VPArrayInfo *info = TR::VPArrayInfo::create(this->getFrame()->getValuePropagation(),  0, INT_MAX, 4);
    AbsValue* value = AbsEnvStatic::getClassConstraint(type, this->getFrame()->getValuePropagation(), this->getRegion(), nonnull, info);
    if(value) value->print(this->getFrame()->getValuePropagation());
    absState.push(value);
  return absState;
}

AbsState&
AbsEnvStatic::arraylength(AbsState& absState) {
  //TODO: actually make use of the value
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Int32);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::_new(AbsState& absState, int cpIndex) {
  //TODO: actually look at the semantics
  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
  TR::VPNonNullObject *nonnull = TR::VPNonNullObject::create(this->getFrame()->getValuePropagation());
  AbsValue* value = AbsEnvStatic::getClassConstraint(type, this->getFrame()->getValuePropagation(), this->getRegion(), nonnull, NULL);
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
        traceMsg(TR::comp(), "\n%s:%d:%s _new... look here! \n", __FILE__, __LINE__, __func__);
        if(value) value->print(this->getFrame()->getValuePropagation());
  }
  absState.push(value);
  return absState;
}

AbsState&
AbsEnvStatic::newarray(AbsState& absState, int atype) {
  //TODO: actually impement the sematncis
  absState.pop();
  AbsValue *result = this->getTopDataType(TR::Address);
  absState.push(result);
  return absState;
}

AbsState&
AbsEnvStatic::invokevirtual(AbsState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Virtual, this->loadFromIDT());
  return absState;
}

AbsState&
AbsEnvStatic::invokestatic(AbsState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Static, this->loadFromIDT());
  return absState;
}

AbsState&
AbsEnvStatic::invokespecial(AbsState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Special, this->loadFromIDT());
  return absState;
}

AbsState&
AbsEnvStatic::invokedynamic(AbsState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::ComputedVirtual, this->loadFromIDT());
  return absState;
}

AbsState&
AbsEnvStatic::invokeinterface(AbsState& absState, int bcIndex, int cpIndex) {
  invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Interface, this->loadFromIDT());
  return absState;
}

void
AbsEnvStatic::invoke(int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind, bool loadFromIDT) {
  TR_ResolvedMethod *callerResolvedMethod = this->getFrame()->_bci.method();
  auto callerResolvedMethodSymbol = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), callerResolvedMethod, TR::comp());
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
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "\n%s:%d:%s callsite invariants for %s\n", __FILE__, __LINE__, __func__, method->signature(TR::comp()->trMemory()));
  }
  if (loadFromIDT ) {
    IDTNode * callee = this->getNode()->findChildWithBytecodeIndex(bcIndex);
    if (callee) traceMsg(TR::comp(), "\n%s:%d:%s callsite invariants for (FROM IDT) %s\n", __FILE__, __LINE__, __func__, callee->getName());
    if (!callee) goto withoutCallee;
    // TODO: can I place these on the stack?
    
    AbsFrame absFrame(this->getRegion(), callee);
    absFrame.interpret(this, kind);
    //TODO: What I need to do is to create a new AbsEnvStatic with the contents of operand stack as the variable array.
    // how many pops?
  
    return;
  }
withoutCallee:
  // how many pops?
  uint32_t params = method->numberOfExplicitParameters();
  AbsValue *absValue = NULL;
  for (int i = 0; i < params; i++) {
    absValue = this->getState().pop();
    AbsValue *absValue2 = NULL;
    TR::DataType datatype = method->parmType(i);
    switch (datatype) {
      case TR::Double:
      case TR::Int64:
        absValue2 = this->getState().pop();
        break;
      default:
        break;
    }
    if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
    {
      traceMsg(TR::comp(), "\n%s:%d:%s \n", __FILE__, __LINE__, __func__);
      if (absValue) absValue->print(this->getVP());
      if (absValue2) absValue2->print(this->getVP());
    }
  }
  int isStatic = kind == TR::MethodSymbol::Kinds::Static;
  int numberOfImplicitParameters = isStatic ? 0 : 1;
  if (numberOfImplicitParameters == 1) absValue = this->getState().pop();

  if (TR::comp()->getOption(TR_TraceAbstractInterpretation) && !isStatic)
  {
      traceMsg(TR::comp(), "\n%s:%d:%s \n", __FILE__, __LINE__, __func__);
      if (absValue) absValue->print(this->getVP());
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



AbsFrame::AbsFrame(TR::Region &region, IDTNode *node)
  : _region(region)
  , _target(node->getCallTarget())
  , _node(node)
  , _vp(NULL)
  , _rms(node->getResolvedMethodSymbol())
  , _bci(node->getResolvedMethodSymbol(), static_cast<TR_ResolvedJ9Method*>(node->getCallTarget()->_calleeMethod), static_cast<TR_J9VMBase*>(TR::comp()->fe()), TR::comp())
{
}


AbsEnvStatic* AbsEnvStatic::enterMethod(TR::Region& region, IDTNode* node, AbsFrame* absFrame, TR::ResolvedMethodSymbol* rms)
{
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "%s:%d:%s\n", __FILE__, __LINE__, __func__);
  }
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
     absEnv->getState().set(0, value);
     }

  TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*TR::comp());
  TR::ParameterSymbol *p = nullptr;
  for (int i = numberOfImplicitParameters; !parameterIterator->atEnd(); parameterIterator->advanceCursor(), i++)
     {
     auto parameter = parameterIterator;
     TR::DataType dataType = parameter->getDataType();
     AbsValue *temp;
     switch (dataType) {
        case TR::Int8:
          temp = new (region) AbsValue(NULL, TR::Int32);
          temp->setParamPosition(i);
          absEnv->getState().set(i, temp);
          continue;
        break;

        case TR::Int16:
          temp = new (region) AbsValue(NULL, TR::Int32);
          temp->setParamPosition(i);
          absEnv->getState().set(i, temp);
          continue;
        break;

        case TR::Int32:
          temp = new (region) AbsValue(NULL, dataType);
          temp->setParamPosition(i);
          absEnv->getState().set(i, temp);
          continue;
        break;

        case TR::Int64:
          temp = new (region) AbsValue(NULL, dataType);
          temp->setParamPosition(i);
          absEnv->getState().set(i, temp);
          i = i+1;
          absEnv->getState().set(i, new (region) AbsValue(NULL, TR::NoType));
          continue;
        break;

        case TR::Float:
          temp = new (region) AbsValue(NULL, dataType);
          temp->setParamPosition(i);
          absEnv->getState().set(i, temp);
          continue;
        break;

        case TR::Double:
          temp = new (region) AbsValue(NULL, dataType);
          temp->setParamPosition(i);
          absEnv->getState().set(i, temp);
          i = i+1;
          absEnv->getState().set(i, new (region) AbsValue(NULL, TR::NoType));
        continue;
        break;

        default:
        //TODO: what about vectors and aggregates?
        break;
     }
     const bool isClass = parameter->isClass();
     if (!isClass)
       {
          temp = new (region) AbsValue(NULL, TR::Address);
          temp->setParamPosition(i);
          absEnv->getState().set(i, temp);
       //absEnv->getState().at(i, new (region) AbsValue(NULL, TR::Address));
       continue;
       }

     TR_OpaqueClassBlock *parameterClass = parameter->getOpaqueClass();
     if (!parameterClass)
       {
          temp = new (region) AbsValue(NULL, TR::Address);
          temp->setParamPosition(i);
          absEnv->getState().set(i, temp);
       //absEnv->getState().at(i, new (region) AbsValue(NULL, TR::Address));
       continue;
       }

      AbsValue* value = AbsEnvStatic::getClassConstraint(parameterClass, absFrame->getValuePropagation(), region);
      value->setParamPosition(i);
      absEnv->getState().set(i, value);
     }
  return absEnv;
}

void
AbsFrame::interpret(AbsEnvStatic *absEnvStatic, TR::MethodSymbol::Kinds kind)
{
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "%s:%d:%s interpreting getting state from other state\n", __FILE__, __LINE__, __func__, this->_node->getName());
  }

  auto method = this->_node->getResolvedMethodSymbol()->getResolvedMethod();
  uint32_t params = method->numberOfExplicitParameters();
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "%s:%d:%s number of explicit parameters %d \n", __FILE__, __LINE__, __func__, params);
  }

  int isStatic = kind == TR::MethodSymbol::Kinds::Static;
  int numberOfImplicitParameters = isStatic ? 0 : 1;

  AbsValue *absValue = NULL;
  int totalParameters = numberOfImplicitParameters + params;
  AbsValue *paramsArray[totalParameters];
  for (int i = 0; i < params; i++) {
    absValue = absEnvStatic->getState().pop();
    AbsValue *absValue2 = NULL;
    TR::DataType datatype = method->parmType(i);
    switch (datatype) {
      case TR::Double:
      case TR::Int64:
        absValue2 = absEnvStatic->getState().pop();
        break;
      default:
        break;
    }
    if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
    {
      traceMsg(TR::comp(), "\n%s:%d:%s \n", __FILE__, __LINE__, __func__);
      if (absValue) absValue->print(absEnvStatic->getVP());
      if (absValue2) absValue2->print(absEnvStatic->getVP());
    }

    //TODO do we need absValue or absValue2 for Double and Int64?
    paramsArray[totalParameters - i - 1] = absValue2 ? absValue2 : absValue;
  }

  if (numberOfImplicitParameters == 1) absValue = absEnvStatic->getState().pop();
  paramsArray[0] = absValue;


  if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
  {
      traceMsg(TR::comp(), "\n%s:%d:%s \n", __FILE__, __LINE__, __func__);
      if (absValue) absValue->print(absEnvStatic->getVP());

  }

  AbsEnvStatic innerAbsEnvStatic(this->getRegion(), this->_node, this);

  if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
  {
     traceMsg(TR::comp(), "\n%s:%d:%s what variables are being placed in the local variable array?\n", __FILE__, __LINE__, __func__);
  }
     int value = 0;
     for (int i = 0, j = 0; i < totalParameters; i++, j++)
     {
     if (paramsArray[i]) paramsArray[i]->print(absEnvStatic->getVP());
     //this->aloadn
     if (paramsArray[i]) paramsArray[i]->setParamPosition(i);

      AbsValue *param = paramsArray[i];
      value += this->_node->getMethodSummary() ?  this->_node->getMethodSummary()->predicate(param, i) : 0;
      if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
          traceMsg(TR::comp(), "%s:%d:%s AAAA benefit %d\n", __FILE__, __LINE__, __func__, value);
      }

     

     innerAbsEnvStatic.getState().set(j, paramsArray[i]);
     if (paramsArray[i]->isType2()) innerAbsEnvStatic.getState().set(++j, new (this->getRegion()) AbsValue(NULL, TR::NoType));
     }

  this->_node->setBenefit(value);
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "%s:%d:%s interpreting %s\n", __FILE__, __LINE__, __func__, this->_node->getName());
  }
  TR::CFG* cfg = this->getCFG();
  TR::CFGNode *cfgNode = cfg->getStartForReverseSnapshot();
  TR::Block *startBlock = cfgNode->asBlock();

  // TODO: maybe instead of region try trCurrentStackRegion
  TR::list<OMR::Block*, TR::Region&> queue(this->getRegion());
  //TODO: get rid of TR::comp()
  for (TR::ReversePostorderSnapshotBlockIterator blockIt (startBlock, TR::comp()); blockIt.currentBlock(); ++blockIt)
     {
        OMR::Block *block = blockIt.currentBlock();
        block->_absEnv = NULL;
        block->setVisitCount(0);
        queue.push_back(block);
     }

  while (!queue.empty()) {
    OMR::Block *block = queue.front();
    queue.pop_front();
    interpret(block);
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
        AbsValue *result = absEnvStatic->getTopDataType(datatype);
        absEnvStatic->getState().push(result);
        }
        break;
      case TR::Double:
      case TR::Int64:
        {
        AbsValue *result = absEnvStatic->getTopDataType(datatype);
        absEnvStatic->getState().push(result);
        AbsValue *result2 = absEnvStatic->getTopDataType(TR::NoType);
        absEnvStatic->getState().push(result2);
        }
        break;
      default:
        {
        //TODO: 
        AbsValue *result = absEnvStatic->getTopDataType(TR::Address);
        absEnvStatic->getState().push(result);
        }
        break;
  }
  return;
}


void AbsFrame::interpret()
{
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "%s:%d:%s interpreting %s\n", __FILE__, __LINE__, __func__, this->_node->getName());
  }
  TR::CFG* cfg = this->getCFG();
  TR::CFGNode *cfgNode = cfg->getStartForReverseSnapshot();
  TR::Block *startBlock = cfgNode->asBlock();

  // TODO: maybe instead of region try trCurrentStackRegion
  TR::list<OMR::Block*, TR::Region&> queue(this->getRegion());
  bool first = true;
  //TODO: get rid of TR::comp()
  for (TR::ReversePostorderSnapshotBlockIterator blockIt (startBlock, TR::comp()); blockIt.currentBlock(); ++blockIt)
     {
        OMR::Block *block = blockIt.currentBlock();
        block->_absEnv = NULL;
        if (first) {
           TR_ResolvedMethod *callerResolvedMethod = this->_bci.method();
           auto callerResolvedMethodSymbol = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), callerResolvedMethod, TR::comp());
           block->_absEnv = AbsEnvStatic::enterMethod(this->getRegion(), this->_node, this, callerResolvedMethodSymbol);
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

  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "%s:%d:%s\n", __FILE__, __LINE__, __func__);
  }
  int start = block->getBlockBCIndex();
  int end = start + block->getBlockSize();
  if (start < 0 || end < 1) return;
  // basic block 3 will always be an empty exit block...
  if (block->getNumber() == 3) return;
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
        {
        block->_absEnv->interpret(bc, _bci);
        }
     else if (comp->trace(OMR::benefitInliner))
        {
        traceMsg(comp, "basic block has no absEnv\n");
        }
     }
}


AbsValue*
AbsEnvStatic::getClassConstraintResolved(TR_OpaqueClassBlock *opaqueClass, TR::ValuePropagation *vp, TR::Region &region, TR::VPClassPresence *presence, TR::VPArrayInfo *info)
{
  if (!opaqueClass) return new (region) AbsValue(NULL, TR::Address);

  TR::VPClassType *resolvedClass = TR::VPResolvedClass::create(vp, opaqueClass);
  TR::VPConstraint *classConstraint = TR::VPClass::create(vp, resolvedClass, presence, NULL, info, NULL);
  return new (region) AbsValue (classConstraint, TR::Address);
}

AbsValue* AbsEnvStatic::getClassConstraint(TR_OpaqueClassBlock *opaqueClass, TR::ValuePropagation *vp, TR::Region &region, TR::VPClassPresence *presence, TR::VPArrayInfo *info)
  {
  if (!opaqueClass) return new (region) AbsValue(NULL, TR::Address);

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(vp, opaqueClass);
  TR::VPConstraint *classConstraint = TR::VPClass::create(vp, fixedClass, presence, NULL, info, NULL);
  return new (region) AbsValue (classConstraint, TR::Address);
  }

void AbsFrame::factFlow(OMR::Block *block) {
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "%s:%d:%s\n", __FILE__, __LINE__, __func__);
  }

  if (block->getNumber() == 3) return;

  int start = block->getBlockBCIndex();
  if (start == 0) {
     return; // _absEnv should already be computed and stored there.
  }

  // has no predecessors (apparently can happen... maybe dead code?)
  if (block->getPredecessors().size() == 0) {
    return;
  }

  if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "%s:%d:%s block has null absEnv ? %s\n", __FILE__, __LINE__, __func__, block->_absEnv == NULL ? "TRUE" : "FALSE");
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

  // multiple predecessors...all interpreted
  if (block->hasAbstractInterpretedAllPredecessors()) {
     block->_absEnv = this->mergeAllPredecessors(block);
     block->_absEnv->_block = block;
     return;
  }


  // we have not interpreted all predecessors...
  // look for a predecessor that has been interpreted
  traceMsg(TR::comp(), "we do not have predecessors\n");
  TR::CFGEdgeList &predecessors = block->getPredecessors();
  for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i)
     {
     auto *edge = *i;
     TR::Block *aBlock = edge->getFrom()->asBlock();
     TR::Block *check = edge->getTo()->asBlock();
     if (check != block)
        {
        traceMsg(TR::comp(), "failsCheck\n");
        continue;
        }
     if (!aBlock->_absEnv)
        {
        traceMsg(TR::comp(), "does not have absEnv\n");
        continue;
        }

     traceMsg(TR::comp(), "we have a predecessor\n");
     AbsEnvStatic *oldAbsEnv = aBlock->_absEnv;
     TR_ASSERT(&(oldAbsEnv->getFrame()->getRegion()) != NULL, "our region is null!\n");
     // how many 
     oldAbsEnv->trace();
     AbsEnvStatic *absEnv = new (this->getRegion()) AbsEnvStatic(*oldAbsEnv);
     TR_ASSERT(&(oldAbsEnv->getFrame()->getRegion()) != NULL, "our region is null!\n");
     aBlock->_absEnv->zeroOut(absEnv);
     block->_absEnv = absEnv;
     block->_absEnv->_block = block;
     return;
     }
   traceMsg(TR::comp(), "predecessors found nothing\n");
}

AbsEnvStatic* AbsFrame::mergeAllPredecessors(OMR::Block *block) {
  TR::CFGEdgeList &predecessors = block->getPredecessors();
  AbsEnvStatic *absEnv = nullptr;
  bool first = true;
  if (TR::comp()->trace(OMR::benefitInliner))
     {
     traceMsg(TR::comp(), "computing how merging strategy for basic block %d\n", block->getNumber());
     }
  for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i)
  {
     auto *edge = *i;
     TR::Block *aBlock = edge->getFrom()->asBlock();
     TR::Block *check = edge->getTo()->asBlock();
     if (check != block) {
        continue;
     }
     //TODO: can aBlock->_absEnv be null?
     TR_ASSERT(aBlock->_absEnv, "absEnv is null, i don't think this is possible");
     if (first) {
        first = false;
        if (TR::comp()->trace(OMR::benefitInliner))
           {
           traceMsg(TR::comp(), "copy basic block %d\n", aBlock->getNumber());
           }
        absEnv = new (this->getRegion()) AbsEnvStatic(*aBlock->_absEnv);
        absEnv->trace();
        continue;
     }
     // merge with the rest;
     if (TR::comp()->trace(OMR::benefitInliner))
        {
        traceMsg(TR::comp(), "merge with basic block %d\n", aBlock->getNumber());
        }
     absEnv->merge(*aBlock->_absEnv);
     absEnv->trace();
  }
  return absEnv;
}

TR::CFG*
AbsFrame::getCFG()
{
   return _target->_cfg;
}
