#pragma once

#include "compiler/ilgen/J9ByteCodeIterator.hpp"
#include "compiler/optimizer/AbsVarArrayStatic.hpp"
#include "compiler/optimizer/AbsOpStackStatic.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"
#include "compiler/optimizer/IDT.hpp"

class AbsEnvStatic {
public:
  AbsEnvStatic(TR::Region &region, IDT::Node *node);
  AbsEnvStatic(AbsEnvStatic&);
  void trace(const char* methodName = NULL);
  void interpret();


private:
  void merge(AbsEnvStatic&);
  AbsEnvStatic *mergeAllPredecessors(OMR::Block *);
  void interpret(TR_J9ByteCode, TR_J9ByteCodeIterator &);
  void enterMethod(TR::ResolvedMethodSymbol *);
  void interpret(OMR::Block *, TR_J9ByteCodeIterator &);
  IDT::Node *_node;
  TR::ValuePropagation *_vp;
  TR::ResolvedMethodSymbol *_rms;
  TR::Region &_region;
  AbsVarArrayStatic _array;
  AbsOpStackStatic _stack;


  // stack manipulation
  void push(AbsValue *);
  AbsValue* pop();

  // array manipulation
  void at(unsigned int, AbsValue*);
  AbsValue *at(unsigned int);

  // abstract interpreter
  void aload0getfield(int, TR_J9ByteCodeIterator &);
  void aaload();
  void daload();
  void laload();
  void anewarray(int);
  void newarray(int);
  void arraylength();
  void aastore();
  void aconstnull();
  void multianewarray(int, int);
  void aload(int);
  void aload0();
  void aload1();
  void aload2();
  void aload3();
  void astore(int);
  void astore0();
  void astore1();
  void astore2();
  void astore3();
  void bipush(int);
  void bastore();
  void baload();
  void checkcast(int, int);
  void d2f();
  void d2i();
  void d2l();
  void dload(int);
  void dload0();
  void dload1();
  void dload2();
  void dload3();
  void dadd();
  void dsub();
  void dup();
  void dupx1();
  void dupx2();
  void dup2();
  void dup2x1();
  void dup2x2();
  void dconst0();
  void dconst1();
  void dstore(int);
  void dstore0();
  void dstore1();
  void dstore2();
  void dstore3();
  void ddiv();
  void dmul();
  void drem();
  void dcmpl();
  void dcmpg();
  void frem();
  void f2d();
  void f2i();
  void f2l();
  void fcmpl();
  void fdiv();
  void fconst0();
  void fstore(int);
  void fstore0();
  void fstore1();
  void fstore2();
  void fstore3();
  void fmul();
  void fload(int);
  void fload0();
  void fload1();
  void fload2();
  void fload3();
  void fadd();
  void fsub();
  void getstatic(int, TR_J9ByteCodeIterator &bci);
  void getfield(int, TR_J9ByteCodeIterator &bci);
  void iand();
  void instanceof(int, int, TR_J9ByteCodeIterator &bci);
  void ior();
  void ixor();
  void irem();
  void ishl();
  void ishr();
  void iushr();
  void idiv();
  void imul();
  void ineg();
  void iconstm1();
  void iconst0();
  void iconst1();
  void iconst2();
  void iconst3();
  void iconst4();
  void iconst5();
  void ifeq(int, int, TR_J9ByteCodeIterator &bci);
  void ifne(int, int, TR_J9ByteCodeIterator &bci);
  void iflt(int, int, TR_J9ByteCodeIterator &bci);
  void ifle(int, int, TR_J9ByteCodeIterator &bci);
  void ifgt(int, int, TR_J9ByteCodeIterator &bci);
  void ifge(int, int, TR_J9ByteCodeIterator &bci);
  void ifnull(int, int, TR_J9ByteCodeIterator &bci);
  void ifnonnull(int, int, TR_J9ByteCodeIterator &bci);
  void ificmpge(int, int, TR_J9ByteCodeIterator &bci);
  void ificmpeq(int, int, TR_J9ByteCodeIterator &bci);
  void ificmpne(int, int, TR_J9ByteCodeIterator &bci);
  void ificmplt(int, int, TR_J9ByteCodeIterator &bci);
  void ificmpgt(int, int, TR_J9ByteCodeIterator &bci);
  void ificmple(int, int, TR_J9ByteCodeIterator &bci);
  void ifacmpeq(int, int, TR_J9ByteCodeIterator &bci);
  void ifacmpne(int, int, TR_J9ByteCodeIterator &bci);
  void iload(int);
  void iload0();
  void iload1();
  void iload2();
  void iload3();
  void istore(int);
  void istore0();
  void istore1();
  void istore2();
  void istore3();
  void isub();
  void iadd();
  void i2d();
  void i2f();
  void i2l();
  void i2s();
  void i2c();
  void i2b();
  void ladd();
  void lsub();
  void l2i();
  void ldc(int, TR_J9ByteCodeIterator &);
  void land();
  void ldiv();
  void lmul();
  void lneg();
  void lor();
  void lrem();
  void lshl();
  void lshr();
  void lushr();
  void lxor();
  void l2d();
  void l2f();
  void lload(int);
  void lload0();
  void lload1();
  void lload2();
  void lload3();
  void lconst0();
  void lconst1();
  void lcmp();
  void pop2();
  void swap();
  void sipush(int16_t);
  void iinc(int, int);
  void monitorenter();
  void monitorexit();
  void putfield();
  void putstatic();
  void _new(int);
  void invokevirtual(int, int, TR_J9ByteCodeIterator &);
  void invokespecial(int, int, TR_J9ByteCodeIterator &);
  void invokestatic(int, int, TR_J9ByteCodeIterator &);
  void invokedynamic(int, int, TR_J9ByteCodeIterator &);
  void invokeinterface(int, int, TR_J9ByteCodeIterator &);

  // abstract interpreter helper
  void invoke(int, int, TR_J9ByteCodeIterator &, TR::MethodSymbol::Kinds);
  void aloadn(int);
  void pushConstInt(int);
  void pushNull();
  void iconst(int);
  void ldcString(int);
  void ldcAddress(int, TR_J9ByteCodeIterator &);
  void ldcInt32(int, TR_J9ByteCodeIterator &);
  void ldcInt64(int, TR_J9ByteCodeIterator &);
  void ldcFloat();
  void ldcDouble();
  void factFlow(OMR::Block *);
  AbsValue* getClassConstraint(TR_OpaqueClassBlock *);
  AbsValue* getTopDataType(TR::DataType);
};
