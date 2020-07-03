#include "optimizer/AbsInterpreter.hpp"
#include "optimizer/CallInfo.hpp"

AbsInterpreter::AbsInterpreter(
   IDTNode* node, 
   IDTBuilder* idtBuilder, 
   TR::ValuePropagation* valuePropagation,
   TR::Region& region,
   TR::Compilation* comp
   ):
      _idtNode(node),
      _idtBuilder(idtBuilder),
      _region(region),
      _comp(comp),
      _valuePropagation(valuePropagation),
      _bcIterator(node->getResolvedMethodSymbol(),static_cast<TR_ResolvedJ9Method*>(node->getCallTarget()->_calleeMethod),static_cast<TR_J9VMBase*>(this->comp()->fe()), this->comp())
   {
   if (idtBuilder)
       _methodSummary = new (_region) MethodSummaryExtension(_region, valuePropagation);
   else
      _methodSummary = NULL;
   }

TR::Compilation* AbsInterpreter::comp()
   {
   return _comp;
   }

TR::Region& AbsInterpreter::region()
   {
   return _region;
   }

bool AbsInterpreter::isForBuildingIDT()
   {
   return _idtBuilder != NULL;
   }

bool AbsInterpreter::isForUpdatingIDT()   
   {
   return _idtBuilder == NULL;
   }

UDATA AbsInterpreter::maxOpStackDepth(TR::ResolvedMethodSymbol* symbol)
   {
   TR_ResolvedJ9Method *method = (TR_ResolvedJ9Method *)symbol->getResolvedMethod();
   J9ROMMethod *romMethod = method->romMethod();
   return J9_MAX_STACK_FROM_ROM_METHOD(romMethod);
   }

IDATA AbsInterpreter::maxVarArrayDepth(TR::ResolvedMethodSymbol* symbol)
   {
   TR_ResolvedJ9Method *method = (TR_ResolvedJ9Method *)symbol->getResolvedMethod();
   J9ROMMethod *romMethod = method->romMethod();
   return J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
   }

void AbsInterpreter::interpret()
   {
   TR_CallTarget* callTarget = _idtNode->getCallTarget();
   TR_ASSERT_FATAL(callTarget,"Call Target is NULL!");

   TR::CFG* cfg = callTarget->_cfg;
   TR_ASSERT_FATAL(cfg, "CFG is NULL!");

   traverseBasicBlocks(cfg);
   }

AbsValue* AbsInterpreter::getClassAbsValue(TR_OpaqueClassBlock* opaqueClass, TR::VPClassPresence *presence, TR::VPArrayInfo *info)
   {
   if (!opaqueClass)
      return new (region()) AbsValue(NULL,TR::Address);

   TR::VPClassType *fixedClass = TR::VPFixedClass::create(_valuePropagation, opaqueClass);
   TR::VPConstraint *classConstraint = TR::VPClass::create(_valuePropagation, fixedClass, presence, NULL, info, NULL);
   return new (region()) AbsValue (classConstraint, TR::Address);
   }

AbsValue* AbsInterpreter::getTOPAbsValue(TR::DataType dataType)
   {
   return new (region()) AbsValue(NULL, dataType);
   }

//get the abstract state of the start block of CFG
AbsState* AbsInterpreter::enterMethod(TR::ResolvedMethodSymbol* symbol)
   {
   if (comp()->getOption(TR_TraceAbstractInterpretation)) 
      traceMsg(comp(), "- 1. Abstract Interpreter: Enter method: %s\n", symbol->signature(comp()->trMemory()));
   printf("- 1. Abstract Interpreter: Enter method: %s\n", symbol->signature(comp()->trMemory()));

   AbsState* absState = new (region()) AbsState(region(), maxVarArrayDepth(symbol),maxOpStackDepth(symbol));

   TR_ResolvedMethod *callerResolvedMethod = _bcIterator.method();
   TR::ResolvedMethodSymbol* callerResolvedMethodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), callerResolvedMethod, comp());
   TR_ResolvedMethod *resolvedMethod = callerResolvedMethodSymbol->getResolvedMethod();

   int32_t numberOfParameters = resolvedMethod->numberOfParameters();
   int32_t numberOfExplicitParameters = resolvedMethod->numberOfExplicitParameters();
   int32_t numberOfImplicitParameters = numberOfParameters - numberOfExplicitParameters;
   int32_t hasImplicitParameter = numberOfImplicitParameters > 0;

   //set the implicit parameter
   if (hasImplicitParameter)
      {
      TR_OpaqueClassBlock *implicitParameterClass = resolvedMethod->containingClass();
      AbsValue* value = getClassAbsValue(implicitParameterClass);
      absState->set(0, value);
      }
    
    TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*comp());

   //setting the rest explicit parameters
   for (int32_t i = numberOfImplicitParameters; !parameterIterator->atEnd(); parameterIterator->advanceCursor(), i++)
      {
      TR::DataType dataType = parameterIterator->getDataType();
      AbsValue* temp;

      //primitive types
      switch (dataType)
         {
         case TR::Int8:
            temp = new (region()) AbsValue(NULL, TR::Int32);
            temp->setParamPosition(i);
            absState->set(i,temp);
            continue;
            break;

         case TR::Int16:
            temp = new (region()) AbsValue(NULL, TR::Int32);
            temp->setParamPosition(i);
            absState->set(i, temp);
            continue;
            break;

         case TR::Int32:
            temp = new (region()) AbsValue(NULL, dataType);
            temp->setParamPosition(i);
            absState->set(i, temp);
            continue;
            break;

         case TR::Int64:
            temp = new (region()) AbsValue(NULL, dataType);
            temp->setParamPosition(i);
            absState->set(i, temp);
            i = i+1;
            absState->set(i, new (region()) AbsValue(NULL, TR::NoType));
            continue;
            break;

         case TR::Float:
            temp = new (region()) AbsValue(NULL, dataType);
            temp->setParamPosition(i);
            absState->set(i, temp);
            continue;
            break;

         case TR::Double:
            temp = new (region()) AbsValue(NULL, dataType);
            temp->setParamPosition(i);
            absState->set(i, temp);
            i = i+1;
            absState->set(i, new (region()) AbsValue(NULL, TR::NoType));
            continue;
            break;

         default:
            //TODO: what about vectors and aggregates?
            break;
         }

      //Reference types
      bool isClass = parameterIterator->isClass();
      if (!isClass) // not a class
         {
         temp = new (region()) AbsValue(NULL, TR::Address);
         temp->setParamPosition(i);
         absState->set(i,temp);
         continue;
         }
      else // a class
         {
         TR_OpaqueClassBlock *parameterClass = parameterIterator->getOpaqueClass();
         if (!parameterClass)
            {
            temp = new (region()) AbsValue(NULL, TR::Address);
            temp->setParamPosition(i);
            absState->set(i,temp);
            continue;
            }
         else 
            {
            temp = getClassAbsValue(parameterClass);
            temp->setParamPosition(i);
            absState->set(i,temp);
            continue;
            }   
         }
      

      }
   return absState;
   }

AbsState* AbsInterpreter::mergeAllPredecessors(TR::Block* block)
   {
   printf("--merge predecessor of block %d\n", block->getNumber());
   TR::CFGEdgeList &predecessors = block->getPredecessors();
   AbsState* absState = NULL;

   bool first = true;

   for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i)
      {
      auto *edge = *i;
      TR::Block *aBlock = edge->getFrom()->asBlock();
      TR::Block *check = edge->getTo()->asBlock();
      if (check != block) {
         continue;
      }
      //TODO: can aBlock->_absState be null?
      TR_ASSERT_FATAL(aBlock->_absState, "absState is NULL, i don't think this is possible");
      printf(  "   pre : %d\n", aBlock->getNumber());
      if (first) 
         {
         first = false;
         absState = new (region()) AbsState(aBlock->_absState);

         if (comp()->trace(OMR::benefitInliner))
            absState->trace(_valuePropagation);
         continue;
         }

      // merge with the rest;    
       
      absState->merge(aBlock->_absState,_valuePropagation);
      
      if (comp()->trace(OMR::benefitInliner))
         absState->trace(_valuePropagation);
      }

      return absState;
   }

void AbsInterpreter::transferAbsStates(TR::Block* block)
   {
   bool traceAbstractInterpretion = comp()->getOption(TR_TraceAbstractInterpretation);

   if (traceAbstractInterpretion) 
      traceMsg(comp(), "-    4. Abstract Interpreter: Transfer abstract states\n");

   if (block->getNumber() == 4 || block->getPredecessors().size() == 0) //Is first block or has no predecessors
      {
      if (traceAbstractInterpretion)
         traceMsg(comp(), "      No predecessors. Stop.\n");
      return;
      }
      
   //Case 1:
   // this can happen I think if we have a loop that has some CFG inside. So its better to just return without assigning anything
   // as we should only visit if we actually have abs state to propagate
   if (block->hasOnlyOnePredecessor() && !block->getPredecessors().front()->getFrom()->asBlock()->_absState)
      //TODO: put it at the end of the queue?
      {
      if (traceAbstractInterpretion) 
         traceMsg(comp(), "      There is a loop. Stop.\n");
      return;
      }
      
   //Case: 2
   // If we only have one predecessor and there are no loops.
   if (block->hasOnlyOnePredecessor() && block->getPredecessors().front()->getFrom()->asBlock()->_absState) 
      {
      if (traceAbstractInterpretion) 
         traceMsg(comp(), "      There is only one predecessor and interpreted. Pass this abstract state.\n");

      AbsState *absState = new (region()) AbsState(block->getPredecessors().front()->getFrom()->asBlock()->_absState);
      block->_absState = absState;
      return;
      }

   //Case: 3
   // multiple predecessors...all interpreted
   if (block->hasAbstractInterpretedAllPredecessors()) 
      {
      if (traceAbstractInterpretion) 
         traceMsg(comp(), "      There are multiple predecessors and all interpreted. Merge their abstract states.\n");
      block->_absState = mergeAllPredecessors(block);
      return;
      }

   //Case: 4
   // we have not interpreted all predecessors...
   // look for a predecessor that has been interpreted
   if (traceAbstractInterpretion) 
      traceMsg(comp(), "      Not all predecessors are interpreted. Finding one interpretd...\n");
   TR::CFGEdgeList &predecessors = block->getPredecessors();
   for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i)
      {
      auto *edge = *i;
      TR::Block *parentBlock = edge->getFrom()->asBlock();
      TR::Block *check = edge->getTo()->asBlock();
      if (check != block)
         {
         if (traceAbstractInterpretion)
            traceMsg(comp(), "      fail check\n");
         continue;
         }

      if (!parentBlock->_absState)
         continue;
         

      if (traceAbstractInterpretion)
         traceMsg(comp(), "      Find a predecessor interpreted. Use its type info and setting all abstract values to be TOP\n");

      // We find a predecessor interpreted. Use its type info with all AbsValues being TOP (unkown)
      AbsState *parentState = parentBlock->_absState;
 
      parentState->trace(_valuePropagation);
      AbsState *newState = new (region()) AbsState(parentState);

      size_t stackSize = parentState->getStackSize();
      AbsValue *array[stackSize]; //temp storage array

      for (size_t i = 0; i < stackSize; i++)
         array[stackSize - i - 1] = parentState->pop();
      
      for (size_t i = 0; i < stackSize; i++)
         {
         AbsValue *a = array[i];
         newState->push(getTOPAbsValue(a ? array[i]->getDataType() : TR::Address));
         parentState->push(array[i]);
         }

      size_t arraySize = parentState->getArraySize();
      for (size_t i = 0; i < arraySize; i++)
         {
         AbsValue *a = parentState->at(i);
         AbsValue *value1 = getTOPAbsValue(a ? parentState->at(i)->getDataType() : TR::Address);
         newState->set(i, value1);
         }

      block->_absState = newState;
      return;
      }
      
   if (traceAbstractInterpretion)
      traceMsg(comp(), "      No predecessor is interpreted. Stop.");
   }

void AbsInterpreter::traverseBasicBlocks(TR::CFG* cfg)
   {
   //get start block's AbsState
   AbsState *startBlockState = enterMethod(_idtNode->getResolvedMethodSymbol());

   if (comp()->getOption(TR_TraceAbstractInterpretation)) 
      traceMsg(comp(), "-  2. Abstract Interpreter: Traverse basic blocks\n");
  
   TR::CFGNode *startNode = cfg->getStartForReverseSnapshot();
   TR_ASSERT_FATAL(startNode, "Start Node is NULL");

   TR::Block* startBlock = startNode->asBlock();

   for (TR::ReversePostorderSnapshotBlockIterator blockIt (startBlock, comp()); blockIt.currentBlock(); ++blockIt)
      {
      TR::Block *block = blockIt.currentBlock();
      //set start block's absState
      if (block == startBlock)
         block->_absState = startBlockState;
         
      block->setVisitCount(0);
      traverseByteCode(block);
      }
   }

void AbsInterpreter::traverseByteCode(TR::Block* block)
   {
      printf("-   3. Abstract Interpreter: Traverse bytecode in basic block #:%d\n",block->getNumber());
   bool traceAbstractInterpretation = comp()->getOption(TR_TraceAbstractInterpretation);
   if (traceAbstractInterpretation) 
      traceMsg(comp(), "-   3. Abstract Interpreter: Traverse bytecode in basic block #:%d\n",block->getNumber());

   int32_t start = block->getBlockBCIndex();
   int32_t end = start + block->getBlockSize();
   if (start <0 || end < 1)
      return;

   if (block->getNumber() == 3) //Exit block
      return;

   transferAbsStates(block);

   _bcIterator.setIndex(start);

   for (TR_J9ByteCode bc = _bcIterator.current(); bc != J9BCunknown && _bcIterator.currentByteCodeIndex() < end; bc = _bcIterator.next())
      {
      if (block->_absState != NULL)
         {
         if (traceAbstractInterpretation)
            {
            _bcIterator.printByteCode();
            traceMsg(comp(),"\n");
            }
            
         interpretByteCode(block->_absState, bc, _bcIterator); 
         }
      else
         {
         if (comp()->getOption(TR_TraceAbstractInterpretation)) 
            traceMsg(comp(), "    Basic block: #%d does not have Abstract state. Does not interpret byte code.\n",block->getNumber());
         break;
         }
      
         
      }

   }

void AbsInterpreter::interpretByteCode(AbsState* state, TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci)
   {
      char *J9_ByteCode_Strings[] =
{
    "J9BCnop",
   "J9BCaconstnull",
   "J9BCiconstm1",
   "J9BCiconst0", "J9BCiconst1", "J9BCiconst2", "J9BCiconst3", "J9BCiconst4", "J9BCiconst5", 
   "J9BClconst0", "J9BClconst1", 
   "J9BCfconst0", "J9BCfconst1", "J9BCfconst2",
   "J9BCdconst0", "J9BCdconst1",
   "J9BCbipush", "J9BCsipush",
   "J9BCldc", "J9BCldcw", "J9BCldc2lw", "J9BCldc2dw",
   "J9BCiload", "J9BClload", "J9BCfload", "J9BCdload", "J9BCaload",
   "J9BCiload0", "J9BCiload1", "J9BCiload2", "J9BCiload3", 
   "J9BClload0", "J9BClload1", "J9BClload2", "J9BClload3",
   "J9BCfload0", "J9BCfload1", "J9BCfload2", "J9BCfload3",
   "J9BCdload0", "J9BCdload1", "J9BCdload2", "J9BCdload3",
   "J9BCaload0", "J9BCaload1", "J9BCaload2", "J9BCaload3",
   "J9BCiaload", "J9BClaload", "J9BCfaload", "J9BCdaload", "J9BCaaload", "J9BCbaload", "J9BCcaload", "J9BCsaload",
   "J9BCiloadw", "J9BClloadw", "J9BCfloadw", "J9BCdloadw", "J9BCaloadw", 
   "J9BCistore", "J9BClstore", "J9BCfstore", "J9BCdstore", "J9BCastore",
   "J9BCistorew", "J9BClstorew", "J9BCfstorew", "J9BCdstorew", "J9BCastorew",
   "J9BCistore0", "J9BCistore1", "J9BCistore2", "J9BCistore3",
   "J9BClstore0", "J9BClstore1", "J9BClstore2", "J9BClstore3",
   "J9BCfstore0", "J9BCfstore1", "J9BCfstore2", "J9BCfstore3",
   "J9BCdstore0", "J9BCdstore1", "J9BCdstore2", "J9BCdstore3",
   "J9BCastore0", "J9BCastore1", "J9BCastore2", "J9BCastore3",
   "J9BCiastore", "J9BClastore", "J9BCfastore", "J9BCdastore", "J9BCaastore", "J9BCbastore", "J9BCcastore", "J9BCsastore",
   "J9BCpop", "J9BCpop2",
   "J9BCdup", "J9BCdupx1", "J9BCdupx2", "J9BCdup2", "J9BCdup2x1", "J9BCdup2x2",
   "J9BCswap",
   "J9BCiadd", "J9BCladd", "J9BCfadd", "J9BCdadd",
   "J9BCisub", "J9BClsub", "J9BCfsub", "J9BCdsub",
   "J9BCimul", "J9BClmul", "J9BCfmul", "J9BCdmul",
   "J9BCidiv", "J9BCldiv", "J9BCfdiv", "J9BCddiv",
   "J9BCirem", "J9BClrem", "J9BCfrem", "J9BCdrem",
   "J9BCineg", "J9BClneg", "J9BCfneg", "J9BCdneg",
   "J9BCishl", "J9BClshl", "J9BCishr", "J9BClshr", "J9BCiushr", "J9BClushr",
   "J9BCiand", "J9BCland",
   "J9BCior", "J9BClor",
   "J9BCixor", "J9BClxor",
   "J9BCiinc", "J9BCiincw", 
   "J9BCi2l", "J9BCi2f", "J9BCi2d", 
   "J9BCl2i", "J9BCl2f", "J9BCl2d", "J9BCf2i", "J9BCf2l", "J9BCf2d",
   "J9BCd2i", "J9BCd2l", "J9BCd2f",
   "J9BCi2b", "J9BCi2c", "J9BCi2s",
   "J9BClcmp", "J9BCfcmpl", "J9BCfcmpg", "J9BCdcmpl", "J9BCdcmpg",
   "J9BCifeq", "J9BCifne", "J9BCiflt", "J9BCifge", "J9BCifgt", "J9BCifle",
   "J9BCificmpeq", "J9BCificmpne", "J9BCificmplt", "J9BCificmpge", "J9BCificmpgt", "J9BCificmple", "J9BCifacmpeq", "J9BCifacmpne",
   "J9BCifnull", "J9BCifnonnull",
   "J9BCgoto", 
   "J9BCgotow", 
   "J9BCtableswitch", "J9BClookupswitch",
   "J9BCgenericReturn",
   "J9BCgetstatic", "J9BCputstatic",
   "J9BCgetfield", "J9BCputfield",
   "J9BCinvokevirtual", "J9BCinvokespecial", "J9BCinvokestatic", "J9BCinvokeinterface", "J9BCinvokedynamic", "J9BCinvokehandle", "J9BCinvokehandlegeneric","J9BCinvokespecialsplit", 

   /** \brief
    *      Pops 1 int32_t argument off the stack and truncates to a uint16_t.
    */
	"J9BCReturnC",

	/** \brief
    *      Pops 1 int32_t argument off the stack and truncates to a int16_t.
    */
	"J9BCReturnS",

	/** \brief
    *      Pops 1 int32_t argument off the stack and truncates to a int8_t.
    */
	"J9BCReturnB",

	/** \brief
    *      Pops 1 int32_t argument off the stack returns the single lowest order bit.
    */
	"J9BCReturnZ",

	"J9BCinvokestaticsplit", "J9BCinvokeinterface2",
   "J9BCnew", "J9BCnewarray", "J9BCanewarray", "J9BCmultianewarray",
   "J9BCarraylength",
   "J9BCathrow",
   "J9BCcheckcast",
   "J9BCinstanceof",
   "J9BCmonitorenter", "J9BCmonitorexit",
   "J9BCwide",
   "J9BCasyncCheck",
   "J9BCdefaultvalue",
   "J9BCwithfield",
   "J9BCbreakpoint",
   "J9BCunknown"
};
   bool traceAbstractInterpretation = comp()->getOption(TR_TraceAbstractInterpretation);
   if (traceAbstractInterpretation) 
      traceMsg(comp(), "-     5. Abstract Interpreter: interpret Byte Code\n");
   printf("+Bytecode: %s\n",J9_ByteCode_Strings[bc]);
   switch(bc)
      {
      //alphabetical order
      case J9BCaaload: aaload(state); break;
      case J9BCaastore: aastore(state); break;
      case J9BCaconstnull: aconstnull(state); break;
      case J9BCaload: aload(state, bci.nextByte()); break;
      case J9BCaload0: aload0(state); break;
      case /* aload0getfield */ 215: aload0getfield(state, 0); break;
      case J9BCaload1: aload1(state); break;
      case J9BCaload2: aload2(state); break;
      case J9BCaload3: aload3(state); break;
      case J9BCanewarray: anewarray(state, bci.next2Bytes(), bci.method()); break;
      //case J9BCareturn: 
      case J9BCarraylength: arraylength(state); break;
      case J9BCastore: astore(state, bci.nextByte()); break;
      case J9BCastore0: astore0(state); break;
      case J9BCastore1: astore1(state); break;
      case J9BCastore2: astore2(state); break;
      case J9BCastore3: astore3(state); break;
      case J9BCastorew: astore(state, bci.next2Bytes()); break; // WARN: internal bytecode
      case J9BCathrow: athrow(state); break;
      case J9BCbaload: baload(state); break;
      case J9BCbastore: bastore(state); break;
      case J9BCbipush: bipush(state, bci.nextByte()); break;
      case J9BCcaload : caload(state); break;
      case J9BCcastore: castore(state); break;
      case J9BCcheckcast: checkcast(state, bci.next2Bytes(), bci.currentByteCodeIndex(), bci.method()); break;
      case J9BCd2f: d2f(state); break;
      case J9BCd2i: d2i(state); break;
      case J9BCd2l: d2l(state); break;
      case J9BCdadd: dadd(state); break;
      case J9BCdaload: daload(state); break;
      case J9BCdastore: dastore(state); break;
      case J9BCdcmpl: dcmpl(state); break;
      case J9BCdcmpg: dcmpg(state); break;
      case J9BCdconst0: dconst0(state); break;
      case J9BCdconst1: dconst1(state); break;
      case J9BCddiv: ddiv(state); break;
      case J9BCdload: dload(state, bci.nextByte()); break;
      case J9BCdload0: dload0(state); break;
      case J9BCdload1: dload1(state); break;
      case J9BCdload2: dload2(state); break;
      case J9BCdload3: dload3(state); break;
      case J9BCdmul: dmul(state); break;
      case J9BCdneg: dneg(state); break;
      case J9BCdrem: drem(state); break;
      //case J9BCdreturn
      case J9BCdstore: dstore(state, bci.nextByte()); break;
      case J9BCdstorew: dstore(state, bci.next2Bytes()); break; // WARN: internal bytecode
      case J9BCdstore0: dstore0(state); break;
      case J9BCdstore1: dstore1(state); break;
      case J9BCdstore2: dstore2(state); break;
      case J9BCdstore3: dstore3(state); break;
      case J9BCdsub: dsub(state); break;
      case J9BCdup: dup(state); break;
      case J9BCdupx1: dupx1(state); break;
      case J9BCdupx2: dupx2(state); break;
      case J9BCdup2: dup2(state); break;
      case J9BCdup2x1: dup2x1(state); break;
      case J9BCdup2x2: dup2x2(state); break;
      case J9BCf2d: f2d(state); break;
      case J9BCf2i: f2i(state); break;
      case J9BCf2l: f2l(state); break;
      case J9BCfadd: fadd(state); break;
      case J9BCfaload: faload(state); break;
      case J9BCfastore: fastore(state); break;
      case J9BCfcmpl: fcmpl(state); break;
      case J9BCfcmpg: fcmpg(state); break;
      case J9BCfconst0: fconst0(state); break;
      case J9BCfconst1: fconst1(state); break;
      case J9BCfconst2: fconst2(state); break;
      case J9BCfdiv: fdiv(state); break;
      case J9BCfload: fload(state, bci.nextByte()); break;
      case J9BCfload0: fload0(state); break;
      case J9BCfload1: fload1(state); break;
      case J9BCfload2: fload2(state); break;
      case J9BCfload3: fload3(state); break;
      case J9BCfmul: fmul(state); break;
      case J9BCfneg: fneg(state); break;
      case J9BCfrem: frem(state); break;
      //case J9BCfreturn: 
      case J9BCfstore: fstore(state, bci.nextByte()); break;
      case J9BCfstorew: fstore(state, bci.next2Bytes()); break; // WARN: internal bytecode
      case J9BCfstore0: fstore0(state); break;
      case J9BCfstore1: fstore1(state); break;
      case J9BCfstore2: fstore2(state); break;
      case J9BCfstore3: fstore3(state); break;
      case J9BCfsub: fsub(state); break;
      case J9BCgenericReturn: state->getStackSize() != 0 ? state->pop() : 0; break; 
      case J9BCgetfield: getfield(state, bci.next2Bytes(), bci.method()); break;
      case J9BCgetstatic: getstatic(state, bci.next2Bytes(), bci.method()); break;
      case J9BCgoto: _goto(state, bci.next2BytesSigned()); break;
      case J9BCgotow: _gotow(state, bci.next4BytesSigned()); break;
      case J9BCi2b: i2b(state); break;
      case J9BCi2c: i2c(state); break;
      case J9BCi2d: i2d(state); break;
      case J9BCi2f: i2f(state); break;
      case J9BCi2l: i2l(state); break;
      case J9BCi2s: i2s(state); break;
      case J9BCiadd: iadd(state); break;
      case J9BCiaload: iaload(state); break;
      case J9BCiand: iand(state); break;
      case J9BCiastore: iastore(state); break;
      case J9BCiconstm1: iconstm1(state); break;
      case J9BCiconst0: iconst0(state); break;
      case J9BCiconst1: iconst1(state); break;
      case J9BCiconst2: iconst2(state); break;
      case J9BCiconst3: iconst3(state); break;
      case J9BCiconst4: iconst4(state); break;
      case J9BCiconst5: iconst5(state); break;
      case J9BCidiv: idiv(state); break;
      case J9BCifacmpeq: ificmpeq(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break; //TODO own function
      case J9BCifacmpne: ificmpne(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break; //TODO own function
      case J9BCificmpeq: ificmpeq(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmpge: ificmpge(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmpgt: ificmpgt(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmple: ificmple(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmplt: ificmplt(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmpne: ificmpne(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifeq: ifeq(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifge: ifge(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifgt: ifgt(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifle: ifle(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCiflt: iflt(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifne: ifne(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifnonnull: ifnonnull(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifnull: ifnull(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCiinc: iinc(state, bci.nextByte(), bci.nextByteSigned(2)); break;
      case J9BCiload: iload(state, bci.nextByte()); break;
      case J9BCiload0: iload0(state); break;
      case J9BCiload1: iload1(state); break;
      case J9BCiload2: iload2(state); break;
      case J9BCiload3: iload3(state); break;
      case J9BCimul: imul(state); break;
      case J9BCineg: ineg(state); break;
      case J9BCinstanceof: instanceof(state, bci.next2Bytes(), bci.currentByteCodeIndex(), bci.method()); break;
      // invokes...
      case J9BCinvokedynamic: TR_ASSERT_FATAL(false, "no inoke dynamic"); break;
      case J9BCinvokeinterface: invokeinterface(state, bci.currentByteCodeIndex(), bci.next2Bytes(),bci.method()); break;
      case J9BCinvokespecial: invokespecial(state, bci.currentByteCodeIndex(), bci.next2Bytes(),bci.method()); break;
      case J9BCinvokestatic: invokestatic(state, bci.currentByteCodeIndex(), bci.next2Bytes(),bci.method()); break;
      case J9BCinvokevirtual: invokevirtual(state, bci.currentByteCodeIndex(), bci.next2Bytes(),bci.method());break;
      case J9BCinvokespecialsplit: invokespecial(state, bci.currentByteCodeIndex(), bci.next2Bytes() | J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG, bci.method()); break;
      case J9BCinvokestaticsplit: invokestatic(state, bci.currentByteCodeIndex(), bci.next2Bytes() | J9_STATIC_SPLIT_TABLE_INDEX_FLAG, bci.method()); break;
      case J9BCior: ior(state); break;
      case J9BCirem: irem(state); break;
      //case J9BCireturn:
      case J9BCishl: ishl(state); break;
      case J9BCishr: ishr(state); break;
      case J9BCistore: istore(state, bci.nextByte()); break;
      case J9BCistorew: istore(state, bci.next2Bytes()); break; //WARN: internal bytecode
      case J9BCistore0: istore0(state); break;
      case J9BCistore1: istore1(state); break;
      case J9BCistore2: istore2(state); break;
      case J9BCistore3: istore3(state); break;
      case J9BCisub: isub(state); break;
      case J9BCiushr: iushr(state); break;
      case J9BCixor: ixor(state); break;
      // jsr
      // jsr_w
      case J9BCl2d: l2d(state); break;
      case J9BCl2f: l2f(state); break;
      case J9BCl2i: l2i(state); break;
      case J9BCladd: ladd(state); break;
      case J9BClaload: laload(state); break;
      case J9BCland: land(state); break;
      case J9BClastore: lastore(state); break;
      case J9BClcmp: lcmp(state); break;
      case J9BClconst0: lconst0(state); break;
      case J9BClconst1: lconst1(state); break;
      case J9BCldc: ldc(state, bci.nextByte(), bci.method()); break;
      case J9BCldcw: ldcw(state, bci.next2Bytes(), bci.method()); break;
      case J9BCldc2lw: ldc(state, bci.next2Bytes(), bci.method()); break; //WARN: internal bytecode equivalent to ldc2_w
      case J9BCldc2dw: ldc(state, bci.next2Bytes(), bci.method()); break; //WARN: internal bytecode equivalent to ldc2_w
      case J9BCldiv: ldiv(state); break;
      case J9BClload: lload(state, bci.nextByte()); break;
      case J9BClload0: lload0(state); break;
      case J9BClload1: lload1(state); break;
      case J9BClload2: lload2(state); break;
      case J9BClload3: lload3(state); break;
      case J9BClmul: lmul(state); break;
      case J9BClneg: lneg(state); break;
      case J9BClookupswitch : lookupswitch(state); break;
      case J9BClor: lor(state); break;
      case J9BClrem: lrem(state); break;
      //case J9BClreturn: 
      case J9BClshl: lshl(state); break;
      case J9BClshr: lshr(state); break;
      case J9BClsub: lsub(state); break;
      case J9BClushr: lushr(state); break;
      case J9BClstore: lstore(state, bci.nextByte()); break;
      case J9BClstorew: lstorew(state, bci.next2Bytes()); break; // WARN: internal bytecode
      case J9BClstore0: lstore0(state); break;
      case J9BClstore1: lstore1(state); break;
      case J9BClstore2: lstore2(state); break;
      case J9BClstore3: lstore3(state); break;
      case J9BClxor: lxor(state); break;
      case J9BCmonitorenter: monitorenter(state); break;
      case J9BCmonitorexit: monitorexit(state); break;
      case J9BCmultianewarray: multianewarray(state, bci.next2Bytes(), bci.nextByteSigned(3)); break;
      case J9BCnew: _new(state, bci.next2Bytes(), bci.method()); break;
      case J9BCnewarray: newarray(state, bci.nextByte()); break;
      case J9BCnop: /* yeah nothing */ break;
      case J9BCpop: state->pop(); break;
      case J9BCpop2: pop2(state); break;
      case J9BCputfield: putfield(state); break;
      case J9BCputstatic: putstatic(state); break;
      // ret
      // return
      case J9BCsaload: saload(state); break;
      case J9BCsastore: sastore(state); break;
      case J9BCsipush: sipush(state, bci.next2Bytes()); break;
      case J9BCswap: swap(state); break;
      case J9BCReturnC: state->pop(); break;
      case J9BCReturnS: state->pop(); break;
      case J9BCReturnB: state->pop(); break;
      case J9BCReturnZ: /*yeah nothing */ break;
      case J9BCtableswitch: tableswitch(state); break;
      //TODO: clean me
      case J9BCwide:
      {
      //TODO: iincw
      bci.setIndex(bci.currentByteCodeIndex() + 1);
      TR_J9ByteCode bc2 = bci.current();
      switch (bc2)
      {
      case J9BCiload: iload(state, bci.next2Bytes()); break;
      case J9BClload: lload(state, bci.next2Bytes()); break;
      case J9BCfload: fload(state, bci.next2Bytes()); break;
      case J9BCdload: dload(state, bci.next2Bytes()); break;
      case J9BCaload: aload(state, bci.next2Bytes()); break;
      case J9BCistore: istore(state, bci.next2Bytes()); break; 
      case J9BClstore: dstore(state, bci.next2Bytes()); break;  // TODO: own function?
      case J9BCfstore: fstore(state, bci.next2Bytes()); break; 
      case J9BCdstore: dstore(state, bci.next2Bytes()); break; 
      case J9BCastore: astore(state, bci.next2Bytes()); break; 
      }
      }
      break;
      default:
      break;
      }
   
   }

AbsState* AbsInterpreter::multianewarray(AbsState* absState, int cpIndex, int dimensions)
   {
   for (int i = 0; i < dimensions; i++)
      {
      absState->pop();
      }
   absState->push(getTOPAbsValue(TR::Address));
   return absState;
   }

AbsState* AbsInterpreter::caload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value1 = getTOPAbsValue(TR::Int32);
   absState->push(value1);
   return absState;
   }


AbsState* AbsInterpreter::faload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value1 = getTOPAbsValue(TR::Float);
   absState->push(value1);
   return absState;
   }

AbsState* AbsInterpreter::iaload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value1 = getTOPAbsValue(TR::Int32);
   absState->push(value1);
   return absState;
   }

AbsState* AbsInterpreter::saload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value1 = getTOPAbsValue(TR::Int32);
   absState->push(value1);
   return absState;
   }

//Jack: is this correct?
AbsState* AbsInterpreter::aaload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   // if (!arrayRef || !arrayRef->getConstraint())
   //    {
   //    AbsValue *value1 = getTOPAbsValue(TR::Address);
   //    absState->push(value1);
   //    return absState;
   //    }
   AbsValue *value1 = getTOPAbsValue(TR::Address);
   absState->push(value1);
   return absState;
   // absState->push(arrayRef);
   // return absState;
   }

AbsState* AbsInterpreter::laload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value1 = getTOPAbsValue(TR::Int64);
   AbsValue *value2 = getTOPAbsValue(TR::NoType);
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

AbsState* AbsInterpreter::daload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value1 = getTOPAbsValue(TR::Double);
   AbsValue *value2 = getTOPAbsValue(TR::NoType);
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

AbsState* AbsInterpreter::castore(AbsState* absState)
   {
   //TODO:
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   // if we have arrayRef, we know arrayRef is not null
   // if we have index and arrayRef length, we may know if safe to remove bounds checking
   return absState;
   }

AbsState* AbsInterpreter::dastore(AbsState* absState)
   {
   //TODO:
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   // if we have arrayRef, we know arrayRef is not null
   // if we have index and arrayRef length, we may know if safe to remove bounds checking
   return absState;
   }

AbsState* AbsInterpreter::fastore(AbsState* absState)
   {
   //TODO:
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   // if we have arrayRef, we know arrayRef is not null
   // if we have index and arrayRef length, we may know if safe to remove bounds checking
   return absState;
   }

AbsState* AbsInterpreter::iastore(AbsState* absState)
   {
   //TODO:
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   // if we have arrayRef, we know arrayRef is not null
   // if we have index and arrayRef length, we may know if safe to remove bounds checking
   return absState;
   }

AbsState* AbsInterpreter::lastore(AbsState* absState)
   {
   //TODO:
   AbsValue *value = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   // if we have arrayRef, we know arrayRef is not null
   // if we have index and arrayRef length, we may know if safe to remove bounds checking
   return absState;
   }

AbsState* AbsInterpreter::sastore(AbsState* absState)
   {
   //TODO:
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   // if we have arrayRef, we know arrayRef is not null
   // if we have index and arrayRef length, we may know if safe to remove bounds checking
   return absState;
   }

AbsState* AbsInterpreter::aastore(AbsState* absState)
   {
   //TODO:
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   // if we have arrayRef, we know arrayRef is not null
   // if we have index and arrayRef length, we may know if safe to remove bounds checking
   return absState;
   }

AbsState* AbsInterpreter::aconstnull(AbsState* absState) 
   {
   TR::VPConstraint *null = TR::VPNullObject::create(_valuePropagation);
   AbsValue *absValue = new (region()) AbsValue(null, TR::Address);
   absState->push(absValue);
   return absState;
   }

AbsState* AbsInterpreter::aload(AbsState* absState, int n)
   {
   AbsValue *constraint = absState->at(n);
   absState->push(constraint);
   }

AbsState* AbsInterpreter::aload0getfield(AbsState* absState, int i)
   {
   aload0(absState);
   return absState;
   }

AbsState* AbsInterpreter::aload0(AbsState* absState) 
   {
   aload(absState, 0);
   return absState;
   }

AbsState* AbsInterpreter::aload1(AbsState* absState) 
   {
   aload(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::aload2(AbsState* absState) 
   {
   aload(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::aload3(AbsState* absState) 
   {
   aload(absState, 3);
   return absState;
   }

AbsState* AbsInterpreter::astore(AbsState* absState, int varIndex)
   {
   AbsValue* constraint = absState->pop();
   absState->set(varIndex, constraint);
   return absState;
   }

AbsState* AbsInterpreter::astore0(AbsState* absState) 
   {
   astore(absState, 0);
   return absState;
   }

AbsState* AbsInterpreter::astore1(AbsState* absState)
   {
   astore(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::astore2(AbsState* absState) 
   {
   astore(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::astore3(AbsState* absState) 
   {
   astore(absState, 3);
   return absState;
   }

//jack modified
AbsState* AbsInterpreter::bipush(AbsState* absState, int byte) 
   {
   TR::VPIntConst *data = TR::VPIntConst::create(_valuePropagation, byte);
   AbsValue *absValue = new (region()) AbsValue(data, TR::Int32);
   absState->push(absValue);
   return absState;
   }

//Jack: Modify logic.
AbsState* AbsInterpreter::bastore(AbsState* absState) 
   {
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   return absState;
   }

//Jack: Modify logic.
AbsState* AbsInterpreter::baload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value1 = getTOPAbsValue(TR::Int32);
   absState->push(value1);
   return absState;
   }

//Jack: need inspection
AbsState* AbsInterpreter::checkcast(AbsState* absState, int cpIndex, int bytecodeIndex, TR_ResolvedMethod* method) 
   {
   AbsValue *absValue = absState->pop();
   if (!absValue)
      {
      AbsValue *value1 = getTOPAbsValue(TR::Address);
      absState->push(absValue);
      return absState;
      }
   TR::VPConstraint *objectRef = absValue->getConstraint();

   if (!objectRef)
      {
      absState->push(absValue);
      return absState;
      }

   // TODO: can we do this some other way? I don't want to pass TR::comp();
   TR_OpaqueClassBlock* type = method->getClassFromConstantPool(comp(), cpIndex);
   if (!type)
      {
      absState->push(absValue);
      return absState;
      }

   TR::VPClassType *fixedClass = TR::VPFixedClass::create(_valuePropagation, type);
   if (!fixedClass)
      {
      absState->push(absValue);
      return absState;
      }
   TR::VPConstraint *typeConstraint= TR::VPClass::create(_valuePropagation, fixedClass, NULL, NULL, NULL, NULL);
   if (!typeConstraint)
      {
      absState->push(absValue);
      return absState;
      }
   // If objectRef is not assignable to a class of type type then throw an exception.

   absState->push(new (region()) AbsValue(typeConstraint, TR::Address));
   return absState;
   }

AbsState* AbsInterpreter::dup(AbsState* absState) 
   {
   AbsValue *value = absState->pop();
   absState->push(value);
   absState->push(value); 
   return absState;
   }

AbsState* AbsInterpreter::dupx1(AbsState* absState) 
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   absState->push(value1);
   absState->push(value2);
   absState->push(value1);
   return absState;
   }

AbsState* AbsInterpreter::dupx2(AbsState* absState) 
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   absState->push(value1);
   absState->push(value3);
   absState->push(value2);
   absState->push(value1);
   return absState;
   }

AbsState* AbsInterpreter::dup2(AbsState* absState) 
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   absState->push(value2);
   absState->push(value1);
   absState->push(value2);
   absState->push(value1);
   return absState;
   }

AbsState* AbsInterpreter::dup2x1(AbsState *absState) 
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   absState->push(value2);
   absState->push(value1);
   absState->push(value3);
   absState->push(value2);
   absState->push(value1);
   return absState;
   }

AbsState* AbsInterpreter::dup2x2(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   AbsValue *value4 = absState->pop();
   absState->push(value2);
   absState->push(value1);
   absState->push(value4);
   absState->push(value3);
   absState->push(value2);
   absState->push(value1);
   return absState;
   }

AbsState* AbsInterpreter::_goto(AbsState* absState, int branch)
   {
   return absState;
   }


AbsState* AbsInterpreter::_gotow(AbsState* absState, int branch)
   {
   return absState;
   }

//Jack: modify logic
AbsState* AbsInterpreter::getstatic(AbsState* absState, int cpIndex, TR_ResolvedMethod* method) 
   {
   void* staticAddress;

   TR::DataType type = TR::NoType;

   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   bool isResolved = method->staticAttributes(
                        comp(),
                        cpIndex,
                        &staticAddress,
                        &type,
                        &isVolatile,
                        &isFinal,
                        &isPrivate, 
                        false, // isStore
                        &isUnresolvedInVP,
                        false); //needsAOTValidation

   if (type != TR::Address) 
      {
      AbsValue *value1 = getTOPAbsValue(type);
      absState->push(value1);

      if (!value1->isType2())
         return absState;

      AbsValue *value2= getTOPAbsValue(TR::NoType);
      absState->push(value2);
      return absState;
      }

   AbsValue *value1 = getTOPAbsValue(type);
   absState->push(value1);
   return absState;
   }


AbsState* AbsInterpreter::getfield(AbsState* absState, int cpIndex, TR_ResolvedMethod* method)
   {
   AbsValue *objectref = absState->pop();
   uint32_t fieldOffset;
   TR::DataType type = TR::NoType;
   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   bool isResolved = method->fieldAttributes(
         comp(),
         cpIndex,
         &fieldOffset,
         &type,
         &isVolatile,
         &isFinal,
         &isPrivate, 
         false, // isStore
         &isUnresolvedInVP,
         false); //needsAOTValidation

   AbsValue *value1 = getTOPAbsValue(type);
   absState->push(value1);

   if (!value1->isType2())
      return absState; 

   AbsValue *value2= getTOPAbsValue(TR::NoType);
   absState->push(value2);
   return absState;
   }

//Jack: issue?
AbsState* AbsInterpreter::iand(AbsState* absState)
   {
   AbsValue* value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   int result = value1->getConstraint()->asIntConst()->getLow() & value2->getConstraint()->asIntConst()->getLow();
   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

//Jack: issue?
AbsState* AbsInterpreter::instanceof(AbsState* absState, int cpIndex, int byteCodeIndex, TR_ResolvedMethod* method)
   {
   AbsValue *objectRef = absState->pop();
   if (!objectRef->getConstraint())
      {
      absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, 0), TR::Int32));
      return absState;
      }


   TR_OpaqueClassBlock *block = method->getClassFromConstantPool(comp(), cpIndex);
   if (!block)
      {
      absState->push(new (region()) AbsValue(TR::VPIntRange::create(_valuePropagation, 0, 1), TR::Int32));
      return absState;
      }

   TR::VPClassType *fixedClass = TR::VPFixedClass::create(_valuePropagation, block);
   TR::VPNonNullObject *nonnull = TR::VPNonNullObject::create(_valuePropagation);
   TR::VPConstraint *typeConstraint= TR::VPClass::create(_valuePropagation, fixedClass, nonnull, NULL, NULL, NULL);
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
      if(typeConstraint->intersect(objectRef->getConstraint(), _valuePropagation))
         absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, 1), TR::Int32));
      else
         absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, 0), TR::Int32));
      return absState;
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

   absState->push(new (region()) AbsValue(TR::VPIntRange::create(_valuePropagation, 0, 1), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::ior(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   int result = value1->getConstraint()->asIntConst()->getLow() | value2->getConstraint()->asIntConst()->getLow();
   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::ixor(AbsState* absState) 
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   int result = value1->getConstraint()->asIntConst()->getLow() ^ value2->getConstraint()->asIntConst()->getLow();
   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::irem(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }
   bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   int int1 = value1->getConstraint()->asIntConst()->getLow();
   int int2 = value2->getConstraint()->asIntConst()->getLow();
   int result = int1 - (int1/ int2) * int2;
   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::ishl(AbsState* absState)
   {
   AbsValue* value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }
   bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   int int1 = value1->getConstraint()->asIntConst()->getLow();
   int int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;
   int result = int1 << int2;
   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::ishr(AbsState* absState)
   {

   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }
   bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   int int1 = value1->getConstraint()->asIntConst()->getLow();
   int int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;

   //arithmetic shift.
   int result = int1 >> int2;

   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::iushr(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }
   bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   int int1 = value1->getConstraint()->asIntConst()->getLow();
   int int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;
   int result = int1 >> int2;
   //logical shift, gets rid of the sign.
   result &= 0x7FFFFFFF;
   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::idiv(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }
   bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   int int1 = value1->getConstraint()->asIntConst()->getLow();
   int int2 = value2->getConstraint()->asIntConst()->getLow();
   if (int2 == 0)
      {
      // this should throw an exception.
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }
   int result = int1 / int2;
   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::imul(AbsState* absState)
   {
   AbsValue* value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }
   bool allConstants = value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   int int1 = value1->getConstraint()->asIntConst()->getLow();
   int int2 = value2->getConstraint()->asIntConst()->getLow();
   int result = int1 * int2;
   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::ineg(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   bool allConstants = value1->getConstraint() && value1->getConstraint()->asIntConst();
   if (!allConstants)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   //TODO: more precision for ranges, subtract VPIntConst 0 from value1
   int int1 = value1->getConstraint()->asIntConst()->getLow();
   int result = -int1;
   absState->push(new (region()) AbsValue(TR::VPIntConst::create(_valuePropagation, result), TR::Int32));
   return absState;
   }

AbsState* AbsInterpreter::iconstm1(AbsState* absState)
   {
   iconst(absState, -1);
   return absState;
   }

AbsState* AbsInterpreter::iconst0(AbsState* absState) 
   {
   iconst(absState, 0);
   return absState;
   }

AbsState* AbsInterpreter::iconst1(AbsState* absState)
   {
   iconst(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::iconst2(AbsState* absState)
   {
   iconst(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::iconst3(AbsState* absState)
   {
   iconst(absState, 3);
   return absState;
   }

AbsState* AbsInterpreter::iconst4(AbsState* absState) 
   {
   iconst(absState, 4);
   return absState;
   }

AbsState* AbsInterpreter::iconst5(AbsState* absState)
   {
   iconst(absState, 5);
   return absState;
   }

void AbsInterpreter::iconst(AbsState* absState, int n)
   {
   TR::VPIntConst *intConst = TR::VPIntConst::create(_valuePropagation, n);
   absState->push(new (region()) AbsValue(intConst, TR::Int32));
   }

AbsState* AbsInterpreter::ifeq(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ifne(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::iflt(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ifle(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ifgt(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ifge(AbsState* absState, int branchOffset, int bytecodeIndex) 
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ifnull(AbsState* absState, int branchOffset, int bytecodeIndex) 
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ifnonnull(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ificmpge(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ificmpeq(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ificmpne(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ificmplt(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ificmpgt(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ificmple(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ifacmpeq(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::ifacmpne(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::iload(AbsState* absState, int n)
   {
   AbsValue *constraint = absState->at(n);
   absState->push(constraint);
   return absState;
   }

AbsState* AbsInterpreter::iload0(AbsState* absState) 
   {
   iload(absState, 0);
   return absState;
   }

AbsState* AbsInterpreter::iload1(AbsState* absState)
   {
   iload(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::iload2(AbsState* absState)
   {
   iload(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::iload3(AbsState* absState)
   {
   iload(absState, 3);
   return absState;
   }

AbsState* AbsInterpreter::istore(AbsState* absState, int n)
   {
   absState->set(n, absState->pop());
   return absState;
   }

AbsState* AbsInterpreter::istore0(AbsState* absState)
   {
   istore(absState,0);
   return absState;
   }

AbsState* AbsInterpreter::istore1(AbsState* absState)
   {
   istore(absState,1);
   return absState;
   }

AbsState* AbsInterpreter::istore2(AbsState* absState)
   {
   istore(absState,2);
   return absState;
   }

AbsState* AbsInterpreter::istore3(AbsState* absState)
   {
   istore(absState,3);
   return absState;
   }

AbsState* AbsInterpreter::isub(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   TR::VPConstraint *result_vp = value1->getConstraint()->subtract(value2->getConstraint(), value2->getDataType(), _valuePropagation);
   AbsValue *result = new (region()) AbsValue(result_vp, value2->getDataType());
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::iadd(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   bool nonnull = value1->getConstraint() && value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   TR::VPConstraint *result_vp = value1->getConstraint()->add(value2->getConstraint(), value2->getDataType(), _valuePropagation);
   AbsValue *result = new (region()) AbsValue(result_vp, value2->getDataType());
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::i2d(AbsState* absState)
   {
   AbsValue *value = absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Double);
   absState->push(result);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::i2f(AbsState* absState)
   {
   AbsValue *value = absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Float);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::i2l(AbsState* absState) 
   {
   AbsValue *value = absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Int64);
   absState->push(result);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::i2s(AbsState* absState)
   {
   //empty?
   return absState;
   }

AbsState* AbsInterpreter::i2c(AbsState* absState)
   {
   //empty?
   return absState;
   }

AbsState* AbsInterpreter::i2b(AbsState* absState)
   {
   // empty?
   return absState;
   }

AbsState* AbsInterpreter::dadd(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   AbsValue *value4 = absState->pop();
   AbsValue *result1 = getTOPAbsValue(TR::Double);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::dsub(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   AbsValue *value4 = absState->pop();
   AbsValue *result1 = getTOPAbsValue(TR::Double);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::fsub(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Float);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::fadd(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Float);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::ladd(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   AbsValue *value4 = absState->pop();
   bool nonnull = value2->getConstraint() && value4->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   TR::VPConstraint *result_vp = value2->getConstraint()->add(value4->getConstraint(), value4->getDataType(), _valuePropagation);
   AbsValue *result = new (region()) AbsValue(result_vp, TR::Int64);
   absState->push(result);
   getTOPAbsValue(TR::NoType);
   return absState;
   }

AbsState* AbsInterpreter::lsub(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   AbsValue *value4 = absState->pop();
   bool nonnull = value2->getConstraint() && value4->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   TR::VPConstraint *result_vp = value2->getConstraint()->subtract(value4->getConstraint(), value4->getDataType(), _valuePropagation);
   AbsValue *result = new (region()) AbsValue(result_vp, TR::Int64);
   absState->push(result);
   getTOPAbsValue(TR::NoType);
   return absState;
   }

AbsState* AbsInterpreter::l2i(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   bool nonnull = value2->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int32);
      absState->push(result1);
      return absState;
      }

   bool canCompute = value2->getConstraint() && value2->getConstraint()->asLongConstraint();
   if (!canCompute)
      {
      AbsValue *result = getTOPAbsValue(TR::Int32);
      absState->push(result);
      return absState;
      }

   TR::VPConstraint *intConst = TR::VPIntRange::create(_valuePropagation, value2->getConstraint()->asLongConstraint()->getLow(), value2->getConstraint()->asLongConstraint()->getHigh());
   AbsValue *result = new (region()) AbsValue(intConst, TR::Int32);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::land(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   AbsValue *value4 = absState->pop();
   bool nonnull = value2->getConstraint() && value4->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   int result = value2->getConstraint()->asLongConst()->getLow() & value4->getConstraint()->asLongConst()->getLow();
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

   AbsState* AbsInterpreter::ldiv(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue* value3 = absState->pop();
   AbsValue* value4 = absState->pop();
   bool nonnull = value2->getConstraint() && value4->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   long int1 = value2->getConstraint()->asLongConst()->getLow();
   long int2 = value4->getConstraint()->asLongConst()->getLow();
   if (int2 == 0)
      {
      // this should throw an exception.
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   long result = int1 / int2;
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lmul(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue* value3 = absState->pop();
   AbsValue* value4 = absState->pop();
   bool nonnull = value2->getConstraint() && value4->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   long int1 = value1->getConstraint()->asLongConst()->getLow();
   long int2 = value2->getConstraint()->asLongConst()->getLow();
   long result = int1 * int2;
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lookupswitch(AbsState* absState)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::tableswitch(AbsState* absState)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::lneg(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   bool nonnull = value2->getConstraint();
   if (!nonnull)
      {
      absState->push(value2);
      absState->push(value1);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   long int1 = value1->getConstraint()->asLongConst()->getLow();
   long result = -int1;
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lor(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue* value3 = absState->pop();
   AbsValue* value4 = absState->pop();
   bool nonnull = value2->getConstraint() && value4->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asLongConst() && value2->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   long result = value1->getConstraint()->asLongConst()->getLow() | value2->getConstraint()->asLongConst()->getLow();
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lrem(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue* value3 = absState->pop();
   AbsValue* value4 = absState->pop();
   bool nonnull = value2->getConstraint() && value4->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   long int1 = value2->getConstraint()->asLongConst()->getLow();
   long int2 = value4->getConstraint()->asLongConst()->getLow();
   long result = int1 - (int1/ int2) * int2;
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lshl(AbsState* absState)
   {
   AbsValue *value2 = absState->pop(); // int
   AbsValue* value0 = absState->pop(); // nothing
   AbsValue* value1 = absState->pop(); // long
   bool nonnull = value2->getConstraint() && value1->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asIntConst() && value1->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   long int1 = value1->getConstraint()->asLongConst()->getLow();
   long int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;
   long result = int1 << int2;
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lshr(AbsState* absState)
   {
   AbsValue *value2 = absState->pop(); // int
   AbsValue* value0 = absState->pop(); // nothing
   AbsValue* value1 = absState->pop(); // long
   bool nonnull = value2->getConstraint() && value1->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asIntConst() && value1->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   long int1 = value1->getConstraint()->asLongConst()->getLow();
   long int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;
   long result = int1 >> int2;
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

//Jack: Modifiy logic. issue?
AbsState* AbsInterpreter::lushr(AbsState* absState)
   {
   AbsValue *value2 = absState->pop(); // int
   AbsValue* value0 = absState->pop(); // nothing
   AbsValue* value1 = absState->pop(); // long
   bool nonnull = value2->getConstraint() && value1->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asIntConst() && value1->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   long int1 = value1->getConstraint()->asLongConst()->getLow();
   long int2 = value2->getConstraint()->asIntConst()->getLow() & 0x1f;
   long result = int1 >> int2;
   result &= 0x7FFFFFFFFFFFFFFF;
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lxor(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue* value3 = absState->pop();
   AbsValue* value4 = absState->pop();
   bool nonnull = value2->getConstraint() && value4->getConstraint();
   if (!nonnull)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }
   bool allConstants = value2->getConstraint()->asLongConst() && value4->getConstraint()->asLongConst();
   if (!allConstants)
      {
      AbsValue *result1 = getTOPAbsValue(TR::Int64);
      AbsValue *result2 = getTOPAbsValue(TR::NoType);
      absState->push(result1);
      absState->push(result2);
      return absState;
      }

   long result = value1->getConstraint()->asLongConst()->getLow() ^ value2->getConstraint()->asLongConst()->getLow();
   absState->push(new (region()) AbsValue(TR::VPLongConst::create(_valuePropagation, result), TR::Int64));
   return absState;
   }

AbsState* AbsInterpreter::l2d(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Double);
   absState->push(result);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::l2f(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Float);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::d2f(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Float);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::f2d(AbsState* absState)
   {
   absState->pop();
   AbsValue *result1 = getTOPAbsValue(TR::Double);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::f2i(AbsState* absState)
   {
   absState->pop();
   AbsValue *result1 = getTOPAbsValue(TR::Int32);
   absState->push(result1);
   return absState;
   }

AbsState* AbsInterpreter::f2l(AbsState* absState)
   {
   absState->pop();
   AbsValue *result1 = getTOPAbsValue(TR::Int64);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::d2i(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Int32);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::d2l(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result1 = getTOPAbsValue(TR::Int64);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lload(AbsState* absState, int n)
   {
   AbsValue *value1 = absState->at(n);
   AbsValue *value2 = absState->at(n+1);
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

AbsState* AbsInterpreter::lload0(AbsState* absState)
   {
   lload(absState, 0);
   return absState;
   }

AbsState* AbsInterpreter::lload1(AbsState* absState)
   {
   lload(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::lload2(AbsState* absState)
   {
   lload(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::lload3(AbsState* absState)
   {
   lload(absState, 3);
   return absState;
   }

AbsState* AbsInterpreter::dconst0(AbsState* absState)
   {
   AbsValue *result1 = getTOPAbsValue(TR::Double);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::dconst1(AbsState* absState)
   {
   AbsValue *result1 = getTOPAbsValue(TR::Double);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::fconst0(AbsState* absState)
   {
   AbsValue *result1 = getTOPAbsValue(TR::Float);
   absState->push(result1);
   return absState;
   }

AbsState* AbsInterpreter::fconst1(AbsState* absState)
   {
   AbsValue *result1 = getTOPAbsValue(TR::Float);
   absState->push(result1);
   return absState;
   }

AbsState* AbsInterpreter::fconst2(AbsState* absState)
   {
   AbsValue *result1 = getTOPAbsValue(TR::Float);
   absState->push(result1);
   return absState;
   }

AbsState* AbsInterpreter::dload(AbsState* absState, int n)
   {
   AbsValue *value1 = absState->at(n);
   AbsValue *value2 = absState->at(n+1);
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

AbsState* AbsInterpreter::dload0(AbsState* absState)
   {
   dload(absState, 0);
   dload(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::dload1(AbsState* absState)
   {
   dload(absState, 1);
   dload(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::dload2(AbsState* absState)
   {
   dload(absState, 2);
   dload(absState, 3);
   return absState;
   }

AbsState* AbsInterpreter::dload3(AbsState* absState)
   {
   dload(absState, 3);
   dload(absState, 4);
   return absState;
   }

AbsState* AbsInterpreter::lstorew(AbsState* absState, int n)
   {
   lstore(absState, n);
   return absState;
   }

AbsState* AbsInterpreter::lstore0(AbsState* absState)
   {
   lstore(absState, 0);
   return absState;
   }

AbsState* AbsInterpreter::lstore1(AbsState* absState)
   {
   lstore(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::lstore2(AbsState* absState)
   {
   lstore(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::lstore3(AbsState* absState)
   {
   lstore(absState, 3);
   return absState;
   }


AbsState* AbsInterpreter::lstore(AbsState* absState, int n)
   {
   AbsValue *top = absState->pop();
   AbsValue *bottom = absState->pop();
   absState->set(n, bottom);
   absState->set(n + 1, top);
   return absState;
   }

AbsState* AbsInterpreter::dstore(AbsState* absState, int n)
   {
   AbsValue *top = absState->pop();
   AbsValue *bottom = absState->pop();
   absState->set(n, bottom);
   absState->set(n + 1, top);
   return absState;
   }

AbsState* AbsInterpreter::dstore0(AbsState* absState)
   {
   dstore(absState, 0);
   return absState;
   }

AbsState* AbsInterpreter::dstore1(AbsState* absState)
   {
   dstore(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::dstore2(AbsState* absState)
   {
   dstore(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::dstore3(AbsState* absState)
   {
   dstore(absState, 3);
   return absState;
   }

AbsState* AbsInterpreter::lconst0(AbsState* absState)
   {
   AbsValue *result = getTOPAbsValue(TR::Int64);
   absState->push(result);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lconst1(AbsState* absState)
   {
   AbsValue *result = getTOPAbsValue(TR::Int64);
   absState->push(result);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   return absState;
   }

AbsState* AbsInterpreter::lcmp(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   absState->pop();
   absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Int32);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::pop2(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::fload(AbsState* absState, int n)
   {
   AbsValue *constraint = absState->at(n);
   absState->push(constraint);
   }

AbsState* AbsInterpreter::fload0(AbsState* absState)
   {
   fload(absState, 0);
   return absState;
   }

AbsState* AbsInterpreter::fload1(AbsState* absState)
   {
   fload(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::fload2(AbsState* absState)
   {
   fload(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::fload3(AbsState* absState)
   {
   fload(absState, 3);
   return absState;
   }

AbsState* AbsInterpreter::swap(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

AbsState* AbsInterpreter::fstore(AbsState* absState, int n)
   {
   absState->set(n, absState->pop());
   return absState;
   }

AbsState* AbsInterpreter::fstore0(AbsState* absState)
   {
   fstore(absState, 0);
   return absState;
   }

AbsState* AbsInterpreter::fstore1(AbsState* absState)
   {
   fstore(absState, 1);
   return absState;
   }

AbsState* AbsInterpreter::fstore2(AbsState* absState)
   {
   fstore(absState, 2);
   return absState;
   }

AbsState* AbsInterpreter::fstore3(AbsState* absState)
   {
   fstore(absState, 3);
   return absState;
   }

AbsState* AbsInterpreter::fmul(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   return absState;
   }

//jack modified
AbsState* AbsInterpreter::dmul(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::dneg(AbsState* absState)
   {
   return absState;
   }

AbsState* AbsInterpreter::dcmpl(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   AbsValue* value4 = absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Int32);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::dcmpg(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   AbsValue* value4 = absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Int32);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::fcmpg(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Int32);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::fcmpl(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Int32);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::ddiv(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::fdiv(AbsState*absState)
   {
   AbsValue *value1 = absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::drem(AbsState*absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue* value2 = absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::fneg(AbsState*absState)
   {
   return absState;
   }

AbsState* AbsInterpreter::freturn(AbsState* absState)
   {
   return absState;
   }

AbsState* AbsInterpreter::frem(AbsState*absState)
   {
   AbsValue *value = absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::sipush(AbsState* absState, int16_t _short)
   {
   TR::VPIntConst *data = TR::VPIntConst::create(_valuePropagation, _short);
   AbsValue *result = new (region()) AbsValue(data, TR::Int32);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::iinc(AbsState* absState, int index, int incval)
   {
   AbsValue *value1 = absState->at(index);
   TR::VPIntConstraint *value = value1->getConstraint() ? value1->getConstraint()->asIntConstraint() : NULL;
   if (!value)
      {
      return absState;
      }

   TR::VPIntConst *inc = TR::VPIntConst::create(_valuePropagation, incval);
   TR::VPConstraint *result = value->add(inc, TR::Int32, _valuePropagation);
   AbsValue *result2 = new (region()) AbsValue(result, TR::Int32);
   absState->set(index, result2);
   return absState;
   }

AbsState* AbsInterpreter::putfield(AbsState* absState)
   {
   // WONTFIX we do not model the heap
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   if(value2->isType2()) {
      AbsValue *value3 = absState->pop();
   }
   return absState;
   }  

//jack modified
AbsState* AbsInterpreter::putstatic(AbsState* absState)
   {
   // WONTFIX we do not model the heap
   AbsValue *value1 = absState->pop();
   if (value1->isType2()) { // category type 2
      AbsValue *value2 = absState->pop();
   }
   return absState;
   }

void AbsInterpreter::ldcInt32(int cpIndex, TR_ResolvedMethod* method, AbsState* absState)
   {
   auto value = method->intConstant(cpIndex);
   TR::VPIntConst *constraint = TR::VPIntConst::create(_valuePropagation, value);
   AbsValue *result = new (region()) AbsValue(constraint, TR::Int32);
   absState->push(result);
   }

void AbsInterpreter::ldcInt64(int cpIndex, TR_ResolvedMethod* method, AbsState* absState)
   {
   auto value = method->intConstant(cpIndex);
   TR::VPLongConst *constraint = TR::VPLongConst::create(_valuePropagation, value);
   AbsValue *result = new (region()) AbsValue(constraint, TR::Int64);
   absState->push(result);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result2);
   }

void AbsInterpreter::ldcFloat(AbsState* absState)
   {
   AbsValue *result = getTOPAbsValue(TR::Float);
   absState->push(result);
   }

void AbsInterpreter::ldcDouble(AbsState* absState)
   {
   AbsValue *result = getTOPAbsValue(TR::Double);
   AbsValue *result2 = getTOPAbsValue(TR::NoType);
   absState->push(result);
   absState->push(result2);
   }

void AbsInterpreter::ldcAddress(int cpIndex, TR_ResolvedMethod* method, AbsState* absState) 
   {
   bool isString = method->isStringConstant(cpIndex);
   if (isString) 
   {
   ldcString(cpIndex, method, absState); 
   return;
   }
   //TODO: non string case
   TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
   AbsValue* value = getClassAbsValue(type);
   absState->push(value);
   }

void AbsInterpreter::ldcString(int cpIndex, TR_ResolvedMethod* method, AbsState* absState)
   {
   // TODO: we might need the resolved method symbol here
   // TODO: aAbsState* _rms    
   auto callerResolvedMethodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), method, comp());
   TR::SymbolReference *symRef = comp()->getSymRefTab()->findOrCreateStringSymbol(callerResolvedMethodSymbol, cpIndex);
   if (symRef->isUnresolved())
         {
         TR_OpaqueClassBlock* type = method->getClassFromConstantPool(comp(), cpIndex);
         AbsValue* value = getClassAbsValue(type);
         absState->push(value);
         return;
         }
   TR::VPConstraint *constraint = TR::VPConstString::create(_valuePropagation, symRef);
   AbsValue *result = new (region()) AbsValue(constraint, TR::Address);
   absState->push(result);
   }

AbsState* AbsInterpreter::ldcw(AbsState*absState, int cpIndex,  TR_ResolvedMethod* method)
   {
   ldc(absState, cpIndex, method);
   return absState;
   }

AbsState* AbsInterpreter::ldc(AbsState* absState, int cpIndex,  TR_ResolvedMethod* method)
   {
   TR::DataType datatype = method->getLDCType(cpIndex);
   switch(datatype) 
      {
      case TR::Int32: this->ldcInt32(cpIndex, method, absState); break;
      case TR::Int64: this->ldcInt64(cpIndex,method, absState); break;
      case TR::Float: this->ldcFloat(absState); break;
      case TR::Double: this->ldcDouble(absState); break;
      case TR::Address: this->ldcAddress(cpIndex, method, absState); break;
      default: 
         {
         //TODO: arrays and what nots.
         AbsValue *result = getTOPAbsValue(TR::Address);
         absState->push(result);
         } break;
      }
   return absState;
   }

AbsState* AbsInterpreter::monitorenter(AbsState* absState)
   {
   // TODO: possible optimization
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::monitorexit(AbsState* absState)
   {
   // TODO: possible optimization
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::areturn(AbsState* absState)
   {
   return absState;
   }

AbsState* AbsInterpreter::dreturn(AbsState* absState)
   {
   return absState;
   }

AbsState* AbsInterpreter::athrow(AbsState* absState)
   {
   return absState;
   }

AbsState* AbsInterpreter::anewarray(AbsState* absState, int cpIndex, TR_ResolvedMethod* method)
   {
   TR_OpaqueClassBlock* type = method->getClassFromConstantPool(comp(), cpIndex);
   TR::VPNonNullObject *nonnull = TR::VPNonNullObject::create(_valuePropagation);
   AbsValue *count = absState->pop();

   if (count->getConstraint() && count->getConstraint()->asIntConstraint())
      {
      TR::VPArrayInfo *info = TR::VPArrayInfo::create(_valuePropagation,  ((TR::VPIntConstraint*)count->getConstraint())->getLow(), ((TR::VPIntConstraint*)count->getConstraint())->getHigh(), 4);
      AbsValue* value = getClassAbsValue(type, nonnull, info);
      absState->push(value);
      return absState;
      }

   TR::VPArrayInfo *info = TR::VPArrayInfo::create(_valuePropagation,  0, INT_MAX, 4);
   AbsValue* value = getClassAbsValue(type, nonnull, info);
   absState->push(value);
   return absState;
   }

AbsState* AbsInterpreter::arraylength(AbsState* absState)
   {
   //TODO: actually make use of the value
   absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Int32);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::_new(AbsState* absState, int cpIndex, TR_ResolvedMethod* method)
   {
   //TODO: actually look at the semantics
   TR_OpaqueClassBlock* type = method->getClassFromConstantPool(TR::comp(), cpIndex);
   TR::VPNonNullObject *nonnull = TR::VPNonNullObject::create(_valuePropagation);
   AbsValue* value = getClassAbsValue(type,  nonnull, NULL);

   absState->push(value);
   return absState;
   }

AbsState* AbsInterpreter::newarray(AbsState* absState, int atype)
   {
   //TODO: actually impement the sematncis
   absState->pop();
   AbsValue *result = getTOPAbsValue(TR::Address);
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::invokevirtual(AbsState* absState, int bcIndex, int cpIndex, TR_ResolvedMethod* method)
   {
   invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Virtual, method, absState);
   return absState;
   }

AbsState* AbsInterpreter::invokestatic(AbsState* absState, int bcIndex, int cpIndex, TR_ResolvedMethod* method)
   {
   invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Static, method, absState);
   return absState;
   }

AbsState* AbsInterpreter::invokespecial(AbsState* absState, int bcIndex, int cpIndex, TR_ResolvedMethod* method)
   {
   invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Special, method, absState);
   return absState;
   }

AbsState* AbsInterpreter::invokedynamic(AbsState* absState, int bcIndex, int cpIndex, TR_ResolvedMethod* method)
   {
   invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::ComputedVirtual, method, absState);
   return absState;
   }

AbsState* AbsInterpreter::invokeinterface(AbsState* absState, int bcIndex, int cpIndex, TR_ResolvedMethod* method)
   {
   invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Interface, method, absState);
   return absState;
   }

void AbsInterpreter::invoke(int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind, TR_ResolvedMethod* method, AbsState* absState) 
{
   printf("invoke!!!\n");
   printf("Containing method: %s\n",method->signature(comp()->trMemory()));
  auto callerResolvedMethodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), method, comp());

  TR::SymbolReference *symRef;
  switch(kind) {
    case TR::MethodSymbol::Kinds::Virtual:
      symRef = comp()->getSymRefTab()->findOrCreateVirtualMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
    case TR::MethodSymbol::Kinds::Interface:
      symRef =comp()->getSymRefTab()->findOrCreateInterfaceMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
    case TR::MethodSymbol::Kinds::Static:
      symRef = comp()->getSymRefTab()->findOrCreateStaticMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
    case TR::MethodSymbol::Kinds::Special:
      symRef = comp()->getSymRefTab()->findOrCreateSpecialMethodSymbol(callerResolvedMethodSymbol, cpIndex);
    break;
  }
  TR::Method *j9Method = comp()->fej9()->createMethod(comp()->trMemory(), method->containingClass(), cpIndex);
  if (comp()->getOption(TR_TraceAbstractInterpretation)) {
    traceMsg(TR::comp(), "\n%s:%d:%s callsite invariants for %s\n", __FILE__, __LINE__, __func__, j9Method->signature(TR::comp()->trMemory()));
  }
  if (false ) {
    IDTNode * callee = _idtNode->findChildWithBytecodeIndex(bcIndex);
    if (callee) traceMsg(TR::comp(), "\n%s:%d:%s callsite invariants for (FROM IDT) %s\n", __FILE__, __LINE__, __func__, callee->getName());
    if (!callee) goto withoutCallee;
    // TODO: can I place these on the stack?
    
   //  AbsFrame absFrame(this->getRegion(), callee);
   //  absFrame.interpret(this, kind);
    //TODO: What I need to do is to create a new AbsEnvStatic with the contents of operand stack as the variable array.
    // how many pops?
  
    return;
  }
withoutCallee:
  // how many pops?
  uint32_t params = j9Method->numberOfExplicitParameters();
  AbsValue *absValue = NULL;
  printf("%s Param #:%d\n",j9Method->signature(comp()->trMemory()),params);
  for (int i = 0; i < params; i++) {
    absValue = absState->pop();
    AbsValue *absValue2 = NULL;
    TR::DataType datatype = j9Method->parmType(i);
    switch (datatype) {
      case TR::Double:
      case TR::Int64:
        absValue2 = absState->pop();
        break;
      default:
        break;
    }
    if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
    {
      traceMsg(TR::comp(), "\n%s:%d:%s \n", __FILE__, __LINE__, __func__);
      if (absValue) absValue->print(_valuePropagation);
      if (absValue2) absValue2->print(_valuePropagation);
    }
  }
  int isStatic = kind == TR::MethodSymbol::Kinds::Static;
  int numberOfImplicitParameters = isStatic ? 0 : 1;
  if (numberOfImplicitParameters == 1) absValue = absState->pop();

  if (comp()->getOption(TR_TraceAbstractInterpretation) && !isStatic)
  {
      traceMsg(comp(), "\n%s:%d:%s \n", __FILE__, __LINE__, __func__);
      if (absValue) absValue->print(_valuePropagation);
  }

  //pushes?
  if (j9Method->returnTypeWidth() == 0) return;

  //how many pushes?
  TR::DataType datatype = j9Method->returnType();
  switch(datatype) {
      case TR::Float:
      case TR::Int32:
      case TR::Int16:
      case TR::Int8:
      case TR::Address:
        {
        AbsValue *result = getTOPAbsValue(datatype);
        absState->push(result);
        }
        break;
      case TR::Double:
      case TR::Int64:
        {
        AbsValue *result = getTOPAbsValue(datatype);
        absState->push(result);
        AbsValue *result2 = getTOPAbsValue(TR::NoType);
        absState->push(result2);
        }
        break;
      case TR::NoType:
         break;
      default:
        {
        //TODO: 
        AbsValue *result = getTOPAbsValue(TR::Address);
        absState->push(result);
        }
        break;
  }
  
}