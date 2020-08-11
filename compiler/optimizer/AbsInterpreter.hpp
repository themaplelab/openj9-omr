#ifndef ABS_INTERPRETER_INCL
#define ABS_INTERPRETER_INCL

#include "optimizer/IDTBuilder.hpp"
#include "optimizer/MethodSummary.hpp"
#include "optimizer/AbsState.hpp"
#include "il/Block.hpp"
#include "compiler/infra/ILWalk.hpp"

class AbsInterpreter : public TR_J9ByteCodeIterator, public TR::ReversePostorderSnapshotBlockIterator
   {
   public:
   AbsInterpreter(TR::ResolvedMethodSymbol* callerMethodSymbol, TR::CFG* cfg, TR::Region& region, TR::Compilation* comp);

   bool interpret();

   MethodSummary* getMethodSummary() { return _methodSummary; }
   AbsValue* getReturnValue() { return _returnValue; }

   void setCallerIndex(int32_t callerIndex) { _callerIndex = callerIndex; }
   
   private:
   TR::Compilation* comp() {  return _comp; }
   TR::Region& region() {  return _region;  }

   OMR::ValuePropagation *vp();

   void moveToNextBasicBlock();

   void setStartBlockState();

   // void transferAbsStates();
   // void mergePredecessorsAbsStates();
   
   // bool interpretByteCode();
      
   //For interpreting bytecode and updating AbsState
   void constant(int32_t i);
   void constant(int64 l);
   void constant(double d);
   void constant(float f);
   void constantNull();

   void load(TR::DataType type, int32_t index);

   void store(TR::DataType type, int32_t index);

   void aaload();
   void aastore();
   void aconstnull();
   void aload();
   void aload0();
   void aload0getfield();
   void aload1();
   void aload2();
   void aload3();
   void anewarray();
   void areturn();
   void arraylength();
   void astore();
   void astore0();
   void astore1();
   void astore2();
   void astore3();
   void athrow();
   void baload();
   void bastore();
   void bipush();
   void caload();
   void castore();
   void checkcast();
   void d2f();
   void d2i();
   void d2l();
   void dadd();
   void daload();
   void dastore();
   void dcmpl();
   void dcmpg();
   void dconst0();
   void dconst1();
   void ddiv();
   void dload();
   void dload0();
   void dload1();
   void dload2();
   void dload3();
   void dmul();
   void dneg();
   void drem();
   void dreturn();
   void dstore();
   void dstore0();
   void dstore1();
   void dstore2();
   void dstore3();
   void dsub();
   void dup();
   void dupx1();
   void dupx2();
   void dup2();
   void dup2x1();
   void dup2x2();
   void f2d();
   void f2i();
   void f2l();
   void fadd();
   void faload();
   void fastore();
   void fcmpl();
   void fcmpg();
   void fconst0();
   void fconst1();
   void fconst2();
   void fdiv();
   void fload();
   void fload0();
   void fload1();
   void fload2();
   void fload3();
   void fmul();
   void fneg();
   void frem();
   void freturn();
   void fstore();
   void fstore0();
   void fstore1();
   void fstore2();
   void fstore3();
   void fsub();
   void getfield();
   void getstatic();
   void goto_();
   void gotow();
   void i2b();
   void i2c();
   void i2d();
   void i2f();
   void i2l();
   void i2s();
   void iadd();
   void iaload();
   void iand();
   void iastore();
   void iconstm1();
   void iconst0();
   void iconst1();
   void iconst2();
   void iconst3();
   void iconst4();
   void iconst5();
   void idiv();
   void ifacmpeq();
   void ifacmpne();
   void ificmpeq();
   void ificmpge();
   void ificmpgt();
   void ificmplt();
   void ificmple();
   void ificmpne();
   void ifeq();
   void ifge();
   void ifgt();
   void ifle();
   void iflt();
   void ifne();
   void ifnonnull();
   void ifnull();
   void iinc();
   void iload(bool wide);
   void iload0();
   void iload1();
   void iload2();
   void iload3();
   void imul();
   void ineg();
   void instanceof();
   void invokedynamic();
   void invokeinterface();
   void invokespecial();
   void invokestatic();
   void invokevirtual();
   void ior();
   void irem();
   void ishl();
   void ishr();
   void istore();
   void istore0();
   void istore1();
   void istore2();
   void istore3();
   void isub();
   void iushr();
   void ixor();
   void l2d();
   void l2f();
   void l2i();
   void ladd();
   void laload();
   void land();
   void lastore();
   void lcmp();
   void lconst0();
   void lconst1();
   void ldc(bool wide);
   void ldcw();
   void ldiv();
   void lload();
   void lload0();
   void lload1();
   void lload2();
   void lload3();
   void lmul();
   void lneg();
   void lookupswitch();
   void lor();
   void lrem();
   void lshl();
   void lshr();
   void lstore();
   void lstorew();
   void lstore0();
   void lstore1();
   void lstore2();
   void lstore3();
   void lsub();
   void lushr();
   void lxor();
   void monitorenter();
   void monitorexit();
   void multianewarray();
   void new_();
   void newarray();
   void nop();
   void pop();
   void pop2();
   void putfield();
   void putstatic();
   void saload();
   void sastore();
   void sipush();
   void swap();
   void tableswitch();

   //Interpreter helpers
   void invoke(int, int, TR::MethodSymbol::Kinds, AbsState* absState, TR::Block* block);
   // void iconst(AbsState* absState, int n);
   // void ldcString(int, AbsState*);
   // void ldcAddress(int, AbsState*); 
   // void ldcInt32(int, AbsState*); 
   // void ldcInt64(int, AbsState*); 
   // void ldcFloat(AbsState*);
   // void ldcDouble(AbsState*);

   TR::SymbolReference* getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind);

   TR_CallSite* getCallSite(
      TR::MethodSymbol::Kinds kind,
      TR_ResolvedMethod *callerResolvedMethod,
      TR::TreeTop *callNodeTreeTop,
      TR::Node *parent,
      TR::Node *callNode,
      TR::Method * interfaceMethod,
      TR_OpaqueClassBlock *receiverClass,
      int32_t vftSlot,
      int32_t cpIndex,
      TR_ResolvedMethod *initialCalleeMethod,
      TR::ResolvedMethodSymbol * initialCalleeSymbol,
      bool isIndirectCall,
      bool isInterface,
      TR_ByteCodeInfo& bcInfo,
      int32_t depth=-1,
      bool allConsts=false,
      TR::SymbolReference *symRef=NULL);
      
   TR_CallSite* findCallSiteTargets(
      TR_ResolvedMethod *caller, 
      int bcIndex, 
      int cpIndex, 
      TR::MethodSymbol::Kinds kind, 
      int callerIndex, 
      TR_CallStack* callStack,
      TR::Block* block);


   MethodSummary* _methodSummary;
   AbsValue* _returnValue = NULL;
   IDTBuilder* _idtBuilder = NULL;
   IDTNode* _idtNode = NULL;
   int32_t _callerIndex = -1;
   TR_CallStack* _callStack =NULL;
   TR::ResolvedMethodSymbol* _callerMethodSymbol;
   TR_ResolvedMethod* _callerMethod;
   TR::CFG* _cfg;

   TR::Region& _region;
   TR::Compilation* _comp;
   OMR::ValuePropagation* _vp;
   };



#endif