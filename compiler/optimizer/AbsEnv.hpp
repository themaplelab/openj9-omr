#pragma once

#include "compiler/optimizer/AbsVarArray.hpp"
#include "compiler/optimizer/AbsOpStack.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "MethodSummary.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "compile/Compilation.hpp"
#include <map>



#include "optimizer/ValuePropagation.hpp"

namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace OMR { class Block; }


class AbsEnv {
public:
  typedef TR::typed_allocator<int, TR::Region &> intAllocator;
  typedef std::map<int, bool, std::less<int>, intAllocator > IndexMap;
  AbsEnv(TR::Compilation *comp, TR::Region& region);
  AbsEnv(TR::Compilation *comp, TR::Region& region, IDT::Node* hunk);
  AbsEnv(AbsEnv &absEnv);
  AbsEnv(AbsEnv &absEnv, IDT::Node* hunk);
  AbsEnv(TR::Compilation *, TR::Region&, IDT::Node* hunk, AbsEnv &absEnv);
  void compareInformationAtCallSite(IDT::Node *hunk, AbsEnv &absEnv);
  void compareInformationAtCallSite(TR_ResolvedMethod *resolvedMethod, AbsEnv &context);
  TR::VPConstraint* topAndPop();
  void merge(AbsEnv*);
  void pop();
  void pop2();
  void swap();
  void aastore();
  void aaload();
  void aconst_null();
  void at(int, TR::VPConstraint*);
  void aload(int);
  void aload0(void);
  void aload0getfield(int);
  void aload1(void);
  void aload2(void);
  void aload3(void);
  void aloadn(int);
  void astore0();
  void astore1();
  void astore2();
  void astore3();
  void astore(int);
  void bipush(int);
  void checkcast(int, int);
  void getstatic(int);
  void putfield(int);
  void putstatic(int);
  void getfield(int);
  void iand();
  void interpret(bool skip);
  bool interpret(int address, OMR::Block * block , IndexMap* indexMap );
  bool interpret1(int address, OMR::Block * block , IndexMap* indexMap );
  void interpret(OMR::Block *, IndexMap *);
  void interpret1(OMR::Block *, IndexMap *);
  void interpret2(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci, int end);
  int interpret(int address, int until);
  static bool wereAllPredecessorsVisited(OMR::Block *block, IndexMap *);
  void instanceof(int, int);
  void iconst0(void);
  void iconst(int);
  void iconst1(void);
  void iconst2(void);
  void iconst3(void);
  void iconst4(void);
  void iconst5(void);
  void istore0(void);
  void istore1(void);
  void istore2(void);
  void istore3(void);
  void istore(int n);
  void copyStack(AbsEnv&);
  void invokeinterface(int, int);
  void invokestatic(int, int);
  void invokespecial(int, int);
  void invokevirtual(int bcIndex, int cpIndex);
  void invoke(int, int, TR::MethodSymbol::Kinds);
  void handleInvokeWithoutHunk(int bcIndex, int cpIndex, TR::MethodSymbol::Kinds);
  bool isLengthInternal(const IDT::Node* hunk) const;
  bool isLengthInternal(int cpIndex);
  bool isGetClass(const IDT::Node* hunk) const;
  bool isGetClass(int cpIndex);
  void handleLengthInternal(int, IDT::Node* hunk = NULL);
  void handleGetClass(int, IDT::Node* hunk = NULL);
  void dup();
  void dup_x1();
  void dup_x2();
  void dup2();
  void dup2_x1();
  void dup2_x2();
  void iinc(int, int);
  void multianewarray(int, int);
  void iload(int);
  void iload1();
  void iload2();
  void iload3();
  void ineg();
  void ifeq(int, int, TR_J9ByteCodeIterator&);
  void ifne(int, int, TR_J9ByteCodeIterator&);
  void iflt(int, int, TR_J9ByteCodeIterator&);
  void ifle(int, int, TR_J9ByteCodeIterator&);
  void ifgt(int, int, TR_J9ByteCodeIterator&);
  void ifge(int, int, TR_J9ByteCodeIterator&);
  void ifnull(int, int, TR_J9ByteCodeIterator&);
  void ifnonnull(int, int, TR_J9ByteCodeIterator&);
  void ificmpge(int, int, TR_J9ByteCodeIterator&);
  void ificmpgt(int, int, TR_J9ByteCodeIterator&);
  void ificmple(int, int, TR_J9ByteCodeIterator&);
  void ificmplt(int, int, TR_J9ByteCodeIterator&);
  void ificmpeq(int, int, TR_J9ByteCodeIterator&);
  void ificmpne(int, int, TR_J9ByteCodeIterator&);
  void isub();
  void dadd();
  void daload();
  void dastore();
  void dcmpg();
  void dcmpl();
  void dconst0();
  void ddiv();
  void dmul();
  void dneg();
  void drem();
  void dstore(int);
  void dstore0();
  void dstore1();
  void dstore2();
  void dstore3();
  void dsub();
  void f2d();
  void f2l();
  void f2i();
  void i2f();
  void i2l();
  void i2s();
  void i2b();
  void i2c();
  void aload();
  void bastore();
  void baload();
  void idiv();
  void imul();
  void ior();
  void ixor();
  void irem();
  void ishl();
  void ishr();
  void iushr();
  void dload(int);
  void dload0();
  void dconst1();
  void iadd();
  void ladd();
  void l2d();
  void l2f();
  void l2i();
  void land();
  void lor();
  void lrem();
  void lshl();
  void lshr();
  void lushr();
  void lxor();
  void sipush(int);
  void d2i();
  void d2l();
  void d2f();
  void ldiv();
  void lmul();
  void lneg();
  void lcmp();
  void lload(int);
  void lload0();
  void lload1();
  void lload2();
  void lload3();
  void lconst(long);
  void lconst0();
  void store(int);
  void lconst1();
  void lstore(int);
  void lstore0();
  void lstore1();
  void lstore2();
  void lstore3();
  void lsub();
  void sub(TR::DataType);
  void add(TR::DataType);
  void _goto(int);
  void _new(int cpindex);
  void newarray(int atype);
  void anewarray(int cpIndex);
  void arraylength();
  void ireturn(void);
  void ldc(int);
  void ldcString(int, TR::ResolvedMethodSymbol*);
  void push(TR::VPConstraint*);
  void pushNull(void);
  bool switchLoop(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci);
  bool switchLoop1(TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci);
  bool endsWithReturn(void);
  void updateReturn(TR::VPConstraint*);
  size_t size() const;
  TR::VPConstraint* at(int);
  TR::VPConstraint* top();
  TR::ResolvedMethodSymbol* getMethodSymbol();
  TR_ResolvedMethod* getMethod();
  TR::Compilation *comp() { return this->_comp; }
  TR::ValuePropagation *_vp;
  TR::VPConstraint* _returns;
  IDT::Node *_hunk;
  MethodSummary* _newMethodSummary;
  MethodSummary* getSummaryFromBuffer(IDT::Node *aHunk);
  MethodSummary* getSummaryFromBuffer(const char *name);
  int numberOfParametersInCurrentMethod();
  int _methodSummaryIndex;
  int *_methodSummaryIndexPtr;
  struct cmp_str
  {
    bool operator()(char const *a, char const *b)
    {
      return strcmp(a, b) < 0;
    }
  };
  typedef TR::typed_allocator<const char*, TR::Region &> strAllocator;
#ifdef TR_CAS_INLINER_USE_MAP
  std::map<const char*, MethodSummary*, cmp_str, strAllocator> *_precomputedMethod;
#endif
  void print();
private:
  int _directDependency;
  OMR::Block *_block;
  void addMethodParameterConstraints(IDT::Node*);
  void addMethodParameterConstraints(TR_ResolvedMethod *resolvedMethod);
  void addImplicitArgumentConstraint(IDT::Node *hunk);
  void addImplicitArgumentConstraint(TR_ResolvedMethod *resolvedMethod);
  TR::VPConstraint *getClassConstraint(TR_OpaqueClassBlock*);
  bool _firstReturn;
  bool _endsWithReturn;
  TR::Compilation *_comp;
  TR::Region &_region;
  AbsVarArray _variableArray;
  AbsOpStack _operandStack;
  AbsVarArray **_methodSummary;
};
