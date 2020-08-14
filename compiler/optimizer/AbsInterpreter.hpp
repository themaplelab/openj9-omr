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
   enum BinaryOperator
      {
      plus, minus, mul, div, rem,
      and_, or_, xor_
      };

   enum UnaryOperator
      {
      neg
      };

   enum ShiftOperator
      {
      shl,
      shr,
      ushr
      };

   enum ConditionalBranchOperator
      {
      eq, ge, gt, le, lt, ne,
      null, nonnull,
      cmpeq, cmpge, cmpgt, cmple, cmplt, cmpne
      };

   enum ComparisonOperator //fcmpl, fcmpg, dcmpl, dcmpg. The 'g' and 'l' here are ommitted since float and double are not being modeled. 
      {
      cmp
      };

   TR::Compilation* comp() {  return _comp; }
   TR::Region& region() {  return _region;  }

   OMR::ValuePropagation *vp();

   void moveToNextBasicBlock();

   void setStartBlockState();

   void transferBlockStatesFromPredeccesors();
   void mergePredecessorsAbsStates();
   
   bool interpretByteCode();
      
   //For interpreting bytecode and updating AbsState

   void constant(int32_t i);
   
   void constant(int64_t l);
 
   void constant(float f);

   void constant(double d);

   void constantNull();
   
   void load(TR::DataType type, int32_t index);

   void store(TR::DataType type, int32_t index);

   void arrayLoad(TR::DataType type);

   void arrayStore(TR::DataType type);

   void ldc(bool wide);

   void binaryOperation(TR::DataType type, BinaryOperator op);

   void unaryOperation(TR::DataType type, UnaryOperator op);

   void pop(int32_t size);

   void swap();

   void dup(int32_t size, int32_t delta);

   void nop();

   void shift(TR::DataType type, ShiftOperator op);

   void conversion(TR::DataType fromType, TR::DataType toType);

   void comparison(TR::DataType type, ComparisonOperator op);

   void goto_(int32_t label);

   void conditionalBranch(TR::DataType type, int32_t label, ConditionalBranchOperator op);

   void return_(TR::DataType type, bool oneBit=false);

   void new_();

   void newarray();

   void anewarray();

   void multianewarray(int32_t dimension);

   void arraylength();

   void instanceof();

   void checkcast();

   void get(bool isStatic);
   void put(bool isStatic);

   void iinc(int32_t index, int32_t incVal);

   void monitor(bool kind); //true: enter, false: exit
   void switch_(bool kind); //true: lookup, false: table

   void athrow();

   void invoke(TR::MethodSymbol::Kinds kind);

  

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