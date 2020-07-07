#ifndef ABS_INTERPRETER_INCL
#define ABS_INTERPRETER_INCL

#include "optimizer/IDTBuilder.hpp"
#include "optimizer/MethodSummary.hpp"
#include "optimizer/AbsState.hpp"
#include "il/Block.hpp"
#include "compiler/infra/ILWalk.hpp"

// class AbsSemanticsFunctional
//    {
//    public:
//    virtual AbsState* aaload(AbsState*) = 0;
//    virtual AbsState* aastore(AbsState*) = 0;
//    virtual AbsState* aconstnull(AbsState*) = 0;
//    virtual AbsState* aload(AbsState*, int) = 0;
//    virtual AbsState* aload0(AbsState*) = 0;
//    virtual AbsState* aload0getfield(AbsState*, int) = 0;
//    virtual AbsState* aload1(AbsState*) = 0;
//    virtual AbsState* aload2(AbsState*) = 0;
//    virtual AbsState* aload3(AbsState*) = 0;
//    virtual AbsState* anewarray(AbsState*, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* areturn(AbsState*) = 0;
//    virtual AbsState* arraylength(AbsState*) = 0;
//    virtual AbsState* astore(AbsState*, int) = 0;
//    virtual AbsState* astore0(AbsState*) = 0;
//    virtual AbsState* astore1(AbsState*) = 0;
//    virtual AbsState* astore2(AbsState*) = 0;
//    virtual AbsState* astore3(AbsState*) = 0;
//    virtual AbsState* athrow(AbsState*) = 0;
//    virtual AbsState* baload(AbsState*) = 0;
//    virtual AbsState* bastore(AbsState*) = 0;
//    virtual AbsState* bipush(AbsState*, int) = 0;
//    virtual AbsState* caload(AbsState*) = 0;
//    virtual AbsState* castore(AbsState*) = 0;
//    virtual AbsState* checkcast(AbsState*, int, int,TR_ResolvedMethod*) = 0;
//    virtual AbsState* d2f(AbsState*) = 0;
//    virtual AbsState* d2i(AbsState*) = 0;
//    virtual AbsState* d2l(AbsState*) = 0;
//    virtual AbsState* dadd(AbsState*) = 0;
//    virtual AbsState* daload(AbsState*) = 0;
//    virtual AbsState* dastore(AbsState*) = 0;
//    virtual AbsState* dcmpl(AbsState*) = 0;
//    virtual AbsState* dcmpg(AbsState*) = 0;
//    virtual AbsState* dconst0(AbsState*) = 0;
//    virtual AbsState* dconst1(AbsState*) = 0;
//    virtual AbsState* ddiv(AbsState*) = 0;
//    virtual AbsState* dload(AbsState*, int) = 0;
//    virtual AbsState* dload0(AbsState*) = 0;
//    virtual AbsState* dload1(AbsState*) = 0;
//    virtual AbsState* dload2(AbsState*) = 0;
//    virtual AbsState* dload3(AbsState*) = 0;
//    virtual AbsState* dmul(AbsState*) = 0;
//    virtual AbsState* dneg(AbsState*) = 0;
//    virtual AbsState* drem(AbsState*) = 0;
//    virtual AbsState* dreturn(AbsState*) = 0;
//    virtual AbsState* dstore(AbsState*, int) = 0;
//    virtual AbsState* dstore0(AbsState*) = 0;
//    virtual AbsState* dstore1(AbsState*) = 0;
//    virtual AbsState* dstore2(AbsState*) = 0;
//    virtual AbsState* dstore3(AbsState*) = 0;
//    virtual AbsState* dsub(AbsState*) = 0;
//    virtual AbsState* dup(AbsState*) = 0;
//    virtual AbsState* dupx1(AbsState*) = 0;
//    virtual AbsState* dupx2(AbsState*) = 0;
//    virtual AbsState* dup2(AbsState*) = 0;
//    virtual AbsState* dup2x1(AbsState*) = 0;
//    virtual AbsState* dup2x2(AbsState*) = 0;
//    virtual AbsState* f2d(AbsState*) = 0;
//    virtual AbsState* f2i(AbsState*) = 0;
//    virtual AbsState* f2l(AbsState*) = 0;
//    virtual AbsState* fadd(AbsState*) = 0;
//    virtual AbsState* faload(AbsState*) = 0;
//    virtual AbsState* fastore(AbsState*) = 0;
//    virtual AbsState* fcmpl(AbsState*) = 0;
//    virtual AbsState* fcmpg(AbsState*) = 0;
//    virtual AbsState* fconst0(AbsState*) = 0;
//    virtual AbsState* fconst1(AbsState*) = 0;
//    virtual AbsState* fconst2(AbsState*) = 0;
//    virtual AbsState* fdiv(AbsState*) = 0;
//    virtual AbsState* fload(AbsState*, int) = 0;
//    virtual AbsState* fload0(AbsState*) = 0;
//    virtual AbsState* fload1(AbsState*) = 0;
//    virtual AbsState* fload2(AbsState*) = 0;
//    virtual AbsState* fload3(AbsState*) = 0;
//    virtual AbsState* fmul(AbsState*) = 0;
//    virtual AbsState* fneg(AbsState*) = 0;
//    virtual AbsState* frem(AbsState*) = 0;
//    virtual AbsState* freturn(AbsState*) = 0;
//    virtual AbsState* fstore(AbsState*, int) = 0;
//    virtual AbsState* fstore0(AbsState*) = 0;
//    virtual AbsState* fstore1(AbsState*) = 0;
//    virtual AbsState* fstore2(AbsState*) = 0;
//    virtual AbsState* fstore3(AbsState*) = 0;
//    virtual AbsState* fsub(AbsState*) = 0;
//    virtual AbsState* getfield(AbsState*, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* getstatic(AbsState*, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* _goto(AbsState*, int) = 0;
//    virtual AbsState* _gotow(AbsState*, int) = 0;
//    virtual AbsState* i2b(AbsState*) = 0;
//    virtual AbsState* i2c(AbsState*) = 0;
//    virtual AbsState* i2d(AbsState*) = 0;
//    virtual AbsState* i2f(AbsState*) = 0;
//    virtual AbsState* i2l(AbsState*) = 0;
//    virtual AbsState* i2s(AbsState*) = 0;
//    virtual AbsState* iadd(AbsState*) = 0;
//    virtual AbsState* iaload(AbsState*) = 0;
//    virtual AbsState* iand(AbsState*) = 0;
//    virtual AbsState* iastore(AbsState*) = 0;
//    virtual AbsState* iconstm1(AbsState*) = 0;
//    virtual AbsState* iconst0(AbsState*) = 0;
//    virtual AbsState* iconst1(AbsState*) = 0;
//    virtual AbsState* iconst2(AbsState*) = 0;
//    virtual AbsState* iconst3(AbsState*) = 0;
//    virtual AbsState* iconst4(AbsState*) = 0;
//    virtual AbsState* iconst5(AbsState*) = 0;
//    virtual AbsState* idiv(AbsState*) = 0;
//    virtual AbsState* ifacmpeq(AbsState*, int, int) = 0;
//    virtual AbsState* ifacmpne(AbsState*, int, int) = 0;
//    virtual AbsState* ificmpeq(AbsState*, int, int) = 0;
//    virtual AbsState* ificmpge(AbsState*, int, int) = 0;
//    virtual AbsState* ificmpgt(AbsState*, int, int) = 0;
//    virtual AbsState* ificmplt(AbsState*, int, int) = 0;
//    virtual AbsState* ificmple(AbsState*, int, int) = 0;
//    virtual AbsState* ificmpne(AbsState*, int, int) = 0;
//    virtual AbsState* ifeq(AbsState*, int, int) = 0;
//    virtual AbsState* ifge(AbsState*, int, int) = 0;
//    virtual AbsState* ifgt(AbsState*, int, int) = 0;
//    virtual AbsState* ifle(AbsState*, int, int) = 0;
//    virtual AbsState* iflt(AbsState*, int, int) = 0;
//    virtual AbsState* ifne(AbsState*, int, int) = 0;
//    virtual AbsState* ifnonnull(AbsState*, int, int) = 0;
//    virtual AbsState* ifnull(AbsState*, int, int) = 0;
//    virtual AbsState* iinc(AbsState*, int, int) = 0;
//    virtual AbsState* iload(AbsState*, int) = 0;
//    virtual AbsState* iload0(AbsState*) = 0;
//    virtual AbsState* iload1(AbsState*) = 0;
//    virtual AbsState* iload2(AbsState*) = 0;
//    virtual AbsState* iload3(AbsState*) = 0;
//    virtual AbsState* imul(AbsState*) = 0;
//    virtual AbsState* ineg(AbsState*) = 0;
//    virtual AbsState* instanceof(AbsState*, int, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* invokedynamic(AbsState*, int, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* invokeinterface(AbsState*, int, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* invokespecial(AbsState*, int, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* invokestatic(AbsState*, int, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* invokevirtual(AbsState*, int, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* ior(AbsState*) = 0;
//    virtual AbsState* irem(AbsState*) = 0;
//    //virtual AbsState* ireturn(AbsState*) = 0;
//    virtual AbsState* ishl(AbsState*) = 0;
//    virtual AbsState* ishr(AbsState*) = 0;
//    virtual AbsState* istore(AbsState*, int) = 0;
//    virtual AbsState* istore0(AbsState*) = 0;
//    virtual AbsState* istore1(AbsState*) = 0;
//    virtual AbsState* istore2(AbsState*) = 0;
//    virtual AbsState* istore3(AbsState*) = 0;
//    virtual AbsState* isub(AbsState*) = 0;
//    virtual AbsState* iushr(AbsState*) = 0;
//    virtual AbsState* ixor(AbsState*) = 0;
//    //virtual AbsState* jsr(AbsState*) = 0;
//    //virtual AbsState* jsrw(AbsState*) = 0;
//    virtual AbsState* l2d(AbsState*) = 0;
//    virtual AbsState* l2f(AbsState*) = 0;
//    virtual AbsState* l2i(AbsState*) = 0;
//    virtual AbsState* ladd(AbsState*) = 0;
//    virtual AbsState* laload(AbsState*) = 0;
//    virtual AbsState* land(AbsState*) = 0;
//    virtual AbsState* lastore(AbsState*) = 0;
//    virtual AbsState* lcmp(AbsState*) = 0;
//    virtual AbsState* lconst0(AbsState*) = 0;
//    virtual AbsState* lconst1(AbsState*) = 0;
//    virtual AbsState* ldc(AbsState*, int,  TR_ResolvedMethod*) = 0;
//    virtual AbsState* ldcw(AbsState*, int,  TR_ResolvedMethod*) = 0;
//    //virtual AbsState* ldc2w(AbsState*int) = 0;
//    virtual AbsState* ldiv(AbsState*) = 0;
//    virtual AbsState* lload(AbsState*, int) = 0;
//    virtual AbsState* lload0(AbsState*) = 0;
//    virtual AbsState* lload1(AbsState*) = 0;
//    virtual AbsState* lload2(AbsState*) = 0;
//    virtual AbsState* lload3(AbsState*) = 0;
//    virtual AbsState* lmul(AbsState*) = 0;
//    virtual AbsState* lneg(AbsState*) = 0;
//    virtual AbsState* lookupswitch(AbsState*) = 0;
//    virtual AbsState* lor(AbsState*) = 0;
//    virtual AbsState* lrem(AbsState*) = 0;
//    //virtual AbsState* lreturn(AbsState*) = 0;
//    virtual AbsState* lshl(AbsState*) = 0;
//    virtual AbsState* lshr(AbsState*) = 0;
//    virtual AbsState* lstore(AbsState*, int) = 0;
//    virtual AbsState* lstorew(AbsState*, int) = 0;
//    virtual AbsState* lstore0(AbsState*) = 0;
//    virtual AbsState* lstore1(AbsState*) = 0;
//    virtual AbsState* lstore2(AbsState*) = 0;
//    virtual AbsState* lstore3(AbsState*) = 0;
//    virtual AbsState* lsub(AbsState*) = 0;
//    virtual AbsState* lushr(AbsState*) = 0;
//    virtual AbsState* lxor(AbsState*) = 0;
//    virtual AbsState* monitorenter(AbsState*) = 0;
//    virtual AbsState* monitorexit(AbsState*) = 0;
//    virtual AbsState* multianewarray(AbsState*, int, int) = 0;
//    virtual AbsState* _new(AbsState*, int, TR_ResolvedMethod*) = 0;
//    virtual AbsState* newarray(AbsState*, int) = 0;
//    //virtual AbsState* nop(AbsState*) = 0;
//    //virtual AbsState* pop(AbsState*) = 0;
//    virtual AbsState* pop2(AbsState*) = 0;
//    virtual AbsState* putfield(AbsState*) = 0;
//    virtual AbsState* putstatic(AbsState*) = 0;
//    //virtual AbsState* ret(AbsState*) = 0;
//    //virtual AbsState* return(AbsState*) = 0;
//    virtual AbsState* saload(AbsState*) = 0;
//    virtual AbsState* sastore(AbsState*) = 0;
//    virtual AbsState* sipush(AbsState*, int16_t) = 0;
//    virtual AbsState* swap(AbsState*) = 0;
//    virtual AbsState* tableswitch(AbsState*) = 0;
//    //virtual AbsState* wide(AbsState*) = 0;
//    };

//Abstract Interpreter interprets each method (aka. IDTNode)
//Use different constructors for two different purposes
class AbsInterpreter 
   {
   public:
   //Use this constructor, calling interpret() will build the IDT and obtain the method summary (use getMethodSummary())
   AbsInterpreter(
      IDTNode* node,
      IDTBuilder* idtBuilder, 
      TR::ValuePropagation* valuePropagation, 
      TR_CallStack* callStack,
      IDTNodeDeque* idtNodeChildren,
      TR::Region& region, 
      TR::Compilation* comp);

   // Use this contructor, calling interpret() will use the method summary to update IDTNode's static benefit.
   AbsInterpreter(IDTNode* node, TR::ValuePropagation* valuePropagation, TR::Region& region, TR::Compilation* comp);

   void interpret();

   //Only use this after building IDT and obtaining method summary, otherwise it will cause TR_ASSERT_FATAL to be triggered. 
   MethodSummaryExtension* getMethodSummary();

   private:
   MethodSummaryExtension* _methodSummary;
   IDTBuilder* _idtBuilder;
   IDTNode* _idtNode;
   TR_CallStack* _callStack;
   IDTNodeDeque* _idtNodeChildren;
   TR::Region& _region;
   TR::Compilation* _comp;
   TR_J9ByteCodeIterator _bcIterator;
   TR::ValuePropagation* _valuePropagation;
   TR::Compilation* comp();
   TR::Region& region();

   bool isForBuildingIDT(); 
   bool isForUpdatingIDT();


   //Maybe removed, not used
   UDATA maxOpStackDepth(TR::ResolvedMethodSymbol* symbol);
   IDATA maxVarArrayDepth(TR::ResolvedMethodSymbol* symbol);

   AbsValue* getClassAbsValue(TR_OpaqueClassBlock* opaqueClass, TR::VPClassPresence *presence = NULL, TR::VPArrayInfo *info = NULL);
   AbsValue* getTOPAbsValue(TR::DataType dataType);

   //Interpreter helpers
   void invoke(int, int, TR::MethodSymbol::Kinds, TR_ResolvedMethod* method, AbsState* absState, TR::Block* block);
   void iconst(AbsState* absState, int n);
   void ldcString(int, TR_ResolvedMethod*, AbsState*);
   void ldcAddress(int, TR_ResolvedMethod*, AbsState*); 
   void ldcInt32(int, TR_ResolvedMethod*, AbsState*); 
   void ldcInt64(int, TR_ResolvedMethod*, AbsState*); 
   void ldcFloat(AbsState*);
   void ldcDouble(AbsState*);
   
   //Prior to the asbtract interpretation
   AbsState* enterMethod(TR::ResolvedMethodSymbol* symbol);
   void updateIDTNodeWithMethodSummary(IDTNode* child, AbsState* absState);
   void transferAbsStates(TR::Block* block);
   AbsState* mergeAllPredecessors(TR::Block* block);
   void traverseBasicBlocks(TR::CFG* cfg);
   void traverseByteCode(TR::Block *block);
   void interpretByteCode(AbsState* absState, TR_J9ByteCode bc, TR_J9ByteCodeIterator&, TR::Block* block);
   
   //For interpreting bytecode
   virtual AbsState* aaload(AbsState*);
   virtual AbsState* aastore(AbsState*);
   virtual AbsState* aconstnull(AbsState*);
   virtual AbsState* aload(AbsState*, int);
   virtual AbsState* aload0(AbsState*);
   virtual AbsState* aload0getfield(AbsState*, int);
   virtual AbsState* aload1(AbsState*);
   virtual AbsState* aload2(AbsState*);
   virtual AbsState* aload3(AbsState*);
   virtual AbsState* anewarray(AbsState*, int, TR_ResolvedMethod*);
   virtual AbsState* areturn(AbsState*);
   virtual AbsState* arraylength(AbsState*);
   virtual AbsState* astore(AbsState*, int);
   virtual AbsState* astore0(AbsState*);
   virtual AbsState* astore1(AbsState*);
   virtual AbsState* astore2(AbsState*);
   virtual AbsState* astore3(AbsState*);
   virtual AbsState* athrow(AbsState*);
   virtual AbsState* baload(AbsState*);
   virtual AbsState* bastore(AbsState*);
   virtual AbsState* bipush(AbsState*, int);
   virtual AbsState* caload(AbsState*);
   virtual AbsState* castore(AbsState*);
   virtual AbsState* checkcast(AbsState*, int, int, TR_ResolvedMethod*);
   virtual AbsState* d2f(AbsState*);
   virtual AbsState* d2i(AbsState*);
   virtual AbsState* d2l(AbsState*);
   virtual AbsState* dadd(AbsState*);
   virtual AbsState* daload(AbsState*);
   virtual AbsState* dastore(AbsState*);
   virtual AbsState* dcmpl(AbsState*);
   virtual AbsState* dcmpg(AbsState*);
   virtual AbsState* dconst0(AbsState*);
   virtual AbsState* dconst1(AbsState*);
   virtual AbsState* ddiv(AbsState*);
   virtual AbsState* dload(AbsState*, int);
   virtual AbsState* dload0(AbsState*);
   virtual AbsState* dload1(AbsState*);
   virtual AbsState* dload2(AbsState*);
   virtual AbsState* dload3(AbsState*);
   virtual AbsState* dmul(AbsState*);
   virtual AbsState* dneg(AbsState*);
   virtual AbsState* drem(AbsState*);
   virtual AbsState* dreturn(AbsState*);
   virtual AbsState* dstore(AbsState*, int);
   virtual AbsState* dstore0(AbsState*);
   virtual AbsState* dstore1(AbsState*);
   virtual AbsState* dstore2(AbsState*);
   virtual AbsState* dstore3(AbsState*);
   virtual AbsState* dsub(AbsState*);
   virtual AbsState* dup(AbsState*);
   virtual AbsState* dupx1(AbsState*);
   virtual AbsState* dupx2(AbsState*);
   virtual AbsState* dup2(AbsState*);
   virtual AbsState* dup2x1(AbsState*);
   virtual AbsState* dup2x2(AbsState*);
   virtual AbsState* f2d(AbsState*);
   virtual AbsState* f2i(AbsState*);
   virtual AbsState* f2l(AbsState*);
   virtual AbsState* fadd(AbsState*);
   virtual AbsState* faload(AbsState*);
   virtual AbsState* fastore(AbsState*);
   virtual AbsState* fcmpl(AbsState*);
   virtual AbsState* fcmpg(AbsState*);
   virtual AbsState* fconst0(AbsState*);
   virtual AbsState* fconst1(AbsState*);
   virtual AbsState* fconst2(AbsState*);
   virtual AbsState* fdiv(AbsState*);
   virtual AbsState* fload(AbsState*, int);
   virtual AbsState* fload0(AbsState*);
   virtual AbsState* fload1(AbsState*);
   virtual AbsState* fload2(AbsState*);
   virtual AbsState* fload3(AbsState*);
   virtual AbsState* fmul(AbsState*);
   virtual AbsState* fneg(AbsState*);
   virtual AbsState* frem(AbsState*);
   virtual AbsState* freturn(AbsState*);
   virtual AbsState* fstore(AbsState*, int);
   virtual AbsState* fstore0(AbsState*);
   virtual AbsState* fstore1(AbsState*);
   virtual AbsState* fstore2(AbsState*);
   virtual AbsState* fstore3(AbsState*);
   virtual AbsState* fsub(AbsState*);
   virtual AbsState* getfield(AbsState*, int, TR_ResolvedMethod*);
   virtual AbsState* getstatic(AbsState*, int, TR_ResolvedMethod*);
   virtual AbsState* _goto(AbsState*, int);
   virtual AbsState* _gotow(AbsState*, int);
   virtual AbsState* i2b(AbsState*);
   virtual AbsState* i2c(AbsState*);
   virtual AbsState* i2d(AbsState*);
   virtual AbsState* i2f(AbsState*);
   virtual AbsState* i2l(AbsState*);
   virtual AbsState* i2s(AbsState*);
   virtual AbsState* iadd(AbsState*);
   virtual AbsState* iaload(AbsState*);
   virtual AbsState* iand(AbsState*);
   virtual AbsState* iastore(AbsState*);
   virtual AbsState* iconstm1(AbsState*);
   virtual AbsState* iconst0(AbsState*);
   virtual AbsState* iconst1(AbsState*);
   virtual AbsState* iconst2(AbsState*);
   virtual AbsState* iconst3(AbsState*);
   virtual AbsState* iconst4(AbsState*);
   virtual AbsState* iconst5(AbsState*);
   virtual AbsState* idiv(AbsState*);
   virtual AbsState* ifacmpeq(AbsState*, int, int);
   virtual AbsState* ifacmpne(AbsState*, int, int);
   virtual AbsState* ificmpeq(AbsState*, int, int);
   virtual AbsState* ificmpge(AbsState*, int, int);
   virtual AbsState* ificmpgt(AbsState*, int, int);
   virtual AbsState* ificmplt(AbsState*, int, int);
   virtual AbsState* ificmple(AbsState*, int, int);
   virtual AbsState* ificmpne(AbsState*, int, int);
   virtual AbsState* ifeq(AbsState*, int, int);
   virtual AbsState* ifge(AbsState*, int, int);
   virtual AbsState* ifgt(AbsState*, int, int);
   virtual AbsState* ifle(AbsState*, int, int);
   virtual AbsState* iflt(AbsState*, int, int);
   virtual AbsState* ifne(AbsState*, int, int);
   virtual AbsState* ifnonnull(AbsState*, int, int);
   virtual AbsState* ifnull(AbsState*, int, int);
   virtual AbsState* iinc(AbsState*, int, int);
   virtual AbsState* iload(AbsState*, int);
   virtual AbsState* iload0(AbsState*);
   virtual AbsState* iload1(AbsState*);
   virtual AbsState* iload2(AbsState*);
   virtual AbsState* iload3(AbsState*);
   virtual AbsState* imul(AbsState*);
   virtual AbsState* ineg(AbsState*);
   virtual AbsState* instanceof(AbsState*, int, int, TR_ResolvedMethod*);
   virtual AbsState* invokedynamic(AbsState*, int, int, TR_ResolvedMethod*, TR::Block*);
   virtual AbsState* invokeinterface(AbsState*, int, int, TR_ResolvedMethod*, TR::Block*);
   virtual AbsState* invokespecial(AbsState*, int, int, TR_ResolvedMethod*,TR::Block*);
   virtual AbsState* invokestatic(AbsState*, int, int, TR_ResolvedMethod*,TR::Block*);
   virtual AbsState* invokevirtual(AbsState*, int, int, TR_ResolvedMethod*,TR::Block*);
   virtual AbsState* ior(AbsState*);
   virtual AbsState* irem(AbsState*);
   //virtual AbsState* ireturn(AbsState*);
   virtual AbsState* ishl(AbsState*);
   virtual AbsState* ishr(AbsState*);
   virtual AbsState* istore(AbsState*, int);
   virtual AbsState* istore0(AbsState*);
   virtual AbsState* istore1(AbsState*);
   virtual AbsState* istore2(AbsState*);
   virtual AbsState* istore3(AbsState*);
   virtual AbsState* isub(AbsState*);
   virtual AbsState* iushr(AbsState*);
   virtual AbsState* ixor(AbsState*);
   //virtual AbsState* jsr(AbsState*);
   //virtual AbsState* jsrw(AbsState*);
   virtual AbsState* l2d(AbsState*);
   virtual AbsState* l2f(AbsState*);
   virtual AbsState* l2i(AbsState*);
   virtual AbsState* ladd(AbsState*);
   virtual AbsState* laload(AbsState*);
   virtual AbsState* land(AbsState*);
   virtual AbsState* lastore(AbsState*);
   virtual AbsState* lcmp(AbsState*);
   virtual AbsState* lconst0(AbsState*);
   virtual AbsState* lconst1(AbsState*);
   virtual AbsState* ldc(AbsState*, int, TR_ResolvedMethod*);
   virtual AbsState* ldcw(AbsState*, int, TR_ResolvedMethod*);
   //virtual AbsState* ldc2w(AbsState*int);
   virtual AbsState* ldiv(AbsState*);
   virtual AbsState* lload(AbsState*, int);
   virtual AbsState* lload0(AbsState*);
   virtual AbsState* lload1(AbsState*);
   virtual AbsState* lload2(AbsState*);
   virtual AbsState* lload3(AbsState*);
   virtual AbsState* lmul(AbsState*);
   virtual AbsState* lneg(AbsState*);
   virtual AbsState* lookupswitch(AbsState*);
   virtual AbsState* lor(AbsState*);
   virtual AbsState* lrem(AbsState*);
   //virtual AbsState* lreturn(AbsState*);
   virtual AbsState* lshl(AbsState*);
   virtual AbsState* lshr(AbsState*);
   virtual AbsState* lstore(AbsState*, int);
   virtual AbsState* lstorew(AbsState*, int);
   virtual AbsState* lstore0(AbsState*);
   virtual AbsState* lstore1(AbsState*);
   virtual AbsState* lstore2(AbsState*);
   virtual AbsState* lstore3(AbsState*);
   virtual AbsState* lsub(AbsState*);
   virtual AbsState* lushr(AbsState*);
   virtual AbsState* lxor(AbsState*);
   virtual AbsState* monitorenter(AbsState*);
   virtual AbsState* monitorexit(AbsState*);
   virtual AbsState* multianewarray(AbsState*, int, int);
   virtual AbsState* _new(AbsState*, int, TR_ResolvedMethod*);
   virtual AbsState* newarray(AbsState*, int);
   //virtual AbsState* nop(AbsState*);
   //virtual AbsState* pop(AbsState*);
   virtual AbsState* pop2(AbsState*);
   virtual AbsState* putfield(AbsState*);
   virtual AbsState* putstatic(AbsState*);
   //virtual AbsState* ret(AbsState*);
   //virtual AbsState* return(AbsState*);
   virtual AbsState* saload(AbsState*);
   virtual AbsState* sastore(AbsState*);
   virtual AbsState* sipush(AbsState*, int16_t);
   virtual AbsState* swap(AbsState*);
   virtual AbsState* tableswitch(AbsState*);
   //virtual AbsState* wide(AbsState*);

   };



#endif