#ifndef ABS_INTERPRETER_INCL
#define ABS_INTERPRETER_INCL

#include "optimizer/IDTBuilder.hpp"
#include "optimizer/MethodSummary.hpp"
#include "optimizer/AbsState.hpp"
#include "il/Block.hpp"
#include "compiler/infra/ILWalk.hpp"

class AbsInterpreter 
   {
   public:
   AbsInterpreter(
      IDTNode* node,
      int callerIndex,
      IDTBuilder* idtBuilder,  
      TR_CallStack* callStack,
      TR::Region& region, 
      TR::Compilation* comp);

   bool interpret();

   MethodSummary* getMethodSummary() { return _methodSummary; };
   AbsValue* getReturnValue() { return _returnValue; }; 

   void setCallerIndex(int callerIndex) { _callerIndex = callerIndex; };
   void setCallerMethodSymbol(TR::ResolvedMethodSymbol* symbol) { _callerMethodSymbol = symbol; };
   void setCallStack(TR_CallStack* callStack) { _callStack = callStack; };  

   void cleanup(); 
   
   private:
   TR::Compilation* comp() {  return _comp; };
   TR::Region& region() {  return _region;  };
   OMR::ValuePropagation *vp();

   AbsState* initializeAbsState();

   void transferAbsStates(TR::Block* block);
   AbsState* mergeAllPredecessors(TR::Block* block);
   
   bool interpretByteCode(AbsState* absState, TR_J9ByteCode bc, TR_J9ByteCodeIterator&, TR::Block* block);

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

   
      
   //For interpreting bytecode and updating AbsState
   AbsState* aaload(AbsState*);
   AbsState* aastore(AbsState*);
   AbsState* aconstnull(AbsState*);
   AbsState* aload(AbsState*, int);
   AbsState* aload0(AbsState*);
   AbsState* aload0getfield(AbsState*);
   AbsState* aload1(AbsState*);
   AbsState* aload2(AbsState*);
   AbsState* aload3(AbsState*);
   AbsState* anewarray(AbsState*, int);
   AbsState* areturn(AbsState*);
   AbsState* arraylength(AbsState*);
   AbsState* astore(AbsState*, int);
   AbsState* astore0(AbsState*);
   AbsState* astore1(AbsState*);
   AbsState* astore2(AbsState*);
   AbsState* astore3(AbsState*);
   AbsState* athrow(AbsState*);
   AbsState* baload(AbsState*);
   AbsState* bastore(AbsState*);
   AbsState* bipush(AbsState*, int);
   AbsState* caload(AbsState*);
   AbsState* castore(AbsState*);
   AbsState* checkcast(AbsState*, int, int);
   AbsState* d2f(AbsState*);
   AbsState* d2i(AbsState*);
   AbsState* d2l(AbsState*);
   AbsState* dadd(AbsState*);
   AbsState* daload(AbsState*);
   AbsState* dastore(AbsState*);
   AbsState* dcmpl(AbsState*);
   AbsState* dcmpg(AbsState*);
   AbsState* dconst0(AbsState*);
   AbsState* dconst1(AbsState*);
   AbsState* ddiv(AbsState*);
   AbsState* dload(AbsState*, int);
   AbsState* dload0(AbsState*);
   AbsState* dload1(AbsState*);
   AbsState* dload2(AbsState*);
   AbsState* dload3(AbsState*);
   AbsState* dmul(AbsState*);
   AbsState* dneg(AbsState*);
   AbsState* drem(AbsState*);
   AbsState* dreturn(AbsState*);
   AbsState* dstore(AbsState*, int);
   AbsState* dstore0(AbsState*);
   AbsState* dstore1(AbsState*);
   AbsState* dstore2(AbsState*);
   AbsState* dstore3(AbsState*);
   AbsState* dsub(AbsState*);
   AbsState* dup(AbsState*);
   AbsState* dupx1(AbsState*);
   AbsState* dupx2(AbsState*);
   AbsState* dup2(AbsState*);
   AbsState* dup2x1(AbsState*);
   AbsState* dup2x2(AbsState*);
   AbsState* f2d(AbsState*);
   AbsState* f2i(AbsState*);
   AbsState* f2l(AbsState*);
   AbsState* fadd(AbsState*);
   AbsState* faload(AbsState*);
   AbsState* fastore(AbsState*);
   AbsState* fcmpl(AbsState*);
   AbsState* fcmpg(AbsState*);
   AbsState* fconst0(AbsState*);
   AbsState* fconst1(AbsState*);
   AbsState* fconst2(AbsState*);
   AbsState* fdiv(AbsState*);
   AbsState* fload(AbsState*, int);
   AbsState* fload0(AbsState*);
   AbsState* fload1(AbsState*);
   AbsState* fload2(AbsState*);
   AbsState* fload3(AbsState*);
   AbsState* fmul(AbsState*);
   AbsState* fneg(AbsState*);
   AbsState* frem(AbsState*);
   AbsState* freturn(AbsState*);
   AbsState* fstore(AbsState*, int);
   AbsState* fstore0(AbsState*);
   AbsState* fstore1(AbsState*);
   AbsState* fstore2(AbsState*);
   AbsState* fstore3(AbsState*);
   AbsState* fsub(AbsState*);
   AbsState* getfield(AbsState*, int);
   AbsState* getstatic(AbsState*, int);
   AbsState* _goto(AbsState*, int);
   AbsState* _gotow(AbsState*, int);
   AbsState* i2b(AbsState*);
   AbsState* i2c(AbsState*);
   AbsState* i2d(AbsState*);
   AbsState* i2f(AbsState*);
   AbsState* i2l(AbsState*);
   AbsState* i2s(AbsState*);
   AbsState* iadd(AbsState*);
   AbsState* iaload(AbsState*);
   AbsState* iand(AbsState*);
   AbsState* iastore(AbsState*);
   AbsState* iconstm1(AbsState*);
   AbsState* iconst0(AbsState*);
   AbsState* iconst1(AbsState*);
   AbsState* iconst2(AbsState*);
   AbsState* iconst3(AbsState*);
   AbsState* iconst4(AbsState*);
   AbsState* iconst5(AbsState*);
   AbsState* idiv(AbsState*);
   AbsState* ifacmpeq(AbsState*, int, int);
   AbsState* ifacmpne(AbsState*, int, int);
   AbsState* ificmpeq(AbsState*, int, int);
   AbsState* ificmpge(AbsState*, int, int);
   AbsState* ificmpgt(AbsState*, int, int);
   AbsState* ificmplt(AbsState*, int, int);
   AbsState* ificmple(AbsState*, int, int);
   AbsState* ificmpne(AbsState*, int, int);
   AbsState* ifeq(AbsState*, int, int);
   AbsState* ifge(AbsState*, int, int);
   AbsState* ifgt(AbsState*, int, int);
   AbsState* ifle(AbsState*, int, int);
   AbsState* iflt(AbsState*, int, int);
   AbsState* ifne(AbsState*, int, int);
   AbsState* ifnonnull(AbsState*, int, int);
   AbsState* ifnull(AbsState*, int, int);
   AbsState* iinc(AbsState*, int, int);
   AbsState* iload(AbsState*, int);
   AbsState* iload0(AbsState*);
   AbsState* iload1(AbsState*);
   AbsState* iload2(AbsState*);
   AbsState* iload3(AbsState*);
   AbsState* imul(AbsState*);
   AbsState* ineg(AbsState*);
   AbsState* instanceof(AbsState*, int, int);
   AbsState* invokedynamic(AbsState*, int, int, TR::Block*);
   AbsState* invokeinterface(AbsState*, int, int, TR::Block*);
   AbsState* invokespecial(AbsState*, int, int, TR::Block*);
   AbsState* invokestatic(AbsState*, int, int, TR::Block*);
   AbsState* invokevirtual(AbsState*, int, int, TR::Block*);
   AbsState* ior(AbsState*);
   AbsState* irem(AbsState*);
// AbsState* ireturn(AbsState*);
   AbsState* ishl(AbsState*);
   AbsState* ishr(AbsState*);
   AbsState* istore(AbsState*, int);
   AbsState* istore0(AbsState*);
   AbsState* istore1(AbsState*);
   AbsState* istore2(AbsState*);
   AbsState* istore3(AbsState*);
   AbsState* isub(AbsState*);
   AbsState* iushr(AbsState*);
   AbsState* ixor(AbsState*);
// AbsState* jsr(AbsState*);
// AbsState* jsrw(AbsState*);
   AbsState* l2d(AbsState*);
   AbsState* l2f(AbsState*);
   AbsState* l2i(AbsState*);
   AbsState* ladd(AbsState*);
   AbsState* laload(AbsState*);
   AbsState* land(AbsState*);
   AbsState* lastore(AbsState*);
   AbsState* lcmp(AbsState*);
   AbsState* lconst0(AbsState*);
   AbsState* lconst1(AbsState*);
   AbsState* ldc(AbsState*, int);
   AbsState* ldcw(AbsState*, int);
// AbsState* ldc2w(AbsState*int);
   AbsState* ldiv(AbsState*);
   AbsState* lload(AbsState*, int);
   AbsState* lload0(AbsState*);
   AbsState* lload1(AbsState*);
   AbsState* lload2(AbsState*);
   AbsState* lload3(AbsState*);
   AbsState* lmul(AbsState*);
   AbsState* lneg(AbsState*);
   AbsState* lookupswitch(AbsState*);
   AbsState* lor(AbsState*);
   AbsState* lrem(AbsState*);
// AbsState* lreturn(AbsState*);
   AbsState* lshl(AbsState*);
   AbsState* lshr(AbsState*);
   AbsState* lstore(AbsState*, int);
   AbsState* lstorew(AbsState*, int);
   AbsState* lstore0(AbsState*);
   AbsState* lstore1(AbsState*);
   AbsState* lstore2(AbsState*);
   AbsState* lstore3(AbsState*);
   AbsState* lsub(AbsState*);
   AbsState* lushr(AbsState*);
   AbsState* lxor(AbsState*);
   AbsState* monitorenter(AbsState*);
   AbsState* monitorexit(AbsState*);
   AbsState* multianewarray(AbsState*, int, int);
   AbsState* _new(AbsState*, int);
   AbsState* newarray(AbsState*, int);
   AbsState* nop(AbsState*);
   AbsState* pop(AbsState*);
   AbsState* pop2(AbsState*);
   AbsState* putfield(AbsState*, int);
   AbsState* putstatic(AbsState*, int);
// AbsState* ret(AbsState*);
// AbsState* return(AbsState*);
   AbsState* saload(AbsState*);
   AbsState* sastore(AbsState*);
   AbsState* sipush(AbsState*, int16_t);
   AbsState* swap(AbsState*);
   AbsState* tableswitch(AbsState*);
// AbsState* wide(AbsState*);

   //Interpreter helpers
   void invoke(int, int, TR::MethodSymbol::Kinds, AbsState* absState, TR::Block* block);
   void iconst(AbsState* absState, int n);
   void ldcString(int, AbsState*);
   void ldcAddress(int, AbsState*); 
   void ldcInt32(int, AbsState*); 
   void ldcInt64(int, AbsState*); 
   void ldcFloat(AbsState*);
   void ldcDouble(AbsState*);


   MethodSummary* _methodSummary;
   AbsValue* _returnValue;
   IDTBuilder* _idtBuilder;
   IDTNode* _idtNode;
   int32_t _callerIndex;
   TR_CallStack* _callStack;
   TR::ResolvedMethodSymbol* _callerMethodSymbol;
   TR_ResolvedMethod* _callerMethod;

   TR::Region& _region;
   TR::Compilation* _comp;
   OMR::ValuePropagation* _vp;
   };



#endif