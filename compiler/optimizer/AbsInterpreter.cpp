#include "optimizer/AbsInterpreter.hpp"
#include "optimizer/CallInfo.hpp"

AbsInterpreter::AbsInterpreter(
   IDTNode* node, 
   int callerIndex,
   IDTBuilder* idtBuilder, 
   TR_CallStack* callStack,
   TR::Region& region,
   TR::Compilation* comp
   ):
      _idtNode(node),
      _callerMethodSymbol(node->getResolvedMethodSymbol()),
      _callerMethod( node->getResolvedMethod()),
      _idtBuilder(idtBuilder),
      _callerIndex(callerIndex),
      _region(region),
      _comp(comp),
      _callStack(callStack),
      _vp(NULL)
   {
   _methodSummary = new (_region) MethodSummary(_region, vp(), comp);
   }


OMR::ValuePropagation* AbsInterpreter::vp()
   {
   if (!_vp)
      {
      TR::OptimizationManager* manager = comp()->getOptimizer()->getOptimization(OMR::globalValuePropagation);
      _vp = (OMR::ValuePropagation*) manager->factory()(manager);
      _vp->initialize();
      }
   return _vp;
   }

//Steps of interpret()
//1. Walk basic blocks of the cfg
//2. For each basic block, walk its byte code
//3. interptet each byte code.
bool AbsInterpreter::interpret()
   {
   bool traceAbstractInterpretation = comp()->getOption(TR_TraceAbstractInterpretation);
   
   //TR_CallTarget* callTarget = _idtNode->getCallTarget();

   // TR_ASSERT_FATAL(_callerMethod, "Caller method is NULL!");
   // TR_ASSERT_FATAL(_callerMethodSymbol->getFlowGraph(), "CFG is NULL!");
   TR::CFG* cfg = _idtNode->getCallTarget()->_cfg;

   TR_J9ByteCodeIterator bci(_callerMethodSymbol, static_cast<TR_ResolvedJ9Method*>(_callerMethod), static_cast<TR_J9VMBase*>(comp()->fe()), comp());

   if (traceAbstractInterpretation)
      traceMsg(comp(), "-1. Abstract Interpreter: Initialize AbsState of method: %s\n", _idtNode->getResolvedMethodSymbol()->signature(comp()->trMemory()));

   AbsState *startBlockState = initializeAbsState();

   TR::Block* startBlock = cfg->getStart()->asBlock();

   // Walk the basic blocks in reverse post oder
   for (TR::ReversePostorderSnapshotBlockIterator blockIt (startBlock, comp()); blockIt.currentBlock(); ++blockIt) 
      {
      TR::Block *block = blockIt.currentBlock();

      if (block == startBlock) //entry block
         {
         block->setAbsState(startBlockState);   
         continue;
         }

      if (block == cfg->getEnd()->asBlock()) //exit block
         continue;
      
      if (traceAbstractInterpretation) 
         traceMsg(comp(), "-2. Abstract Interpreter: Interpret basic block #:%d\n",block->getNumber());
      //printf("-2. Abstract Interpreter: Interpret basic block #:%d\n",block->getNumber());

      if (traceAbstractInterpretation) 
         traceMsg(comp(), "-3. Abstract Interpreter: Transfer abstract states\n");

      transferAbsStates(block);

      int32_t start = block->getBlockBCIndex();
      int32_t end = start + block->getBlockSize();

      // if (start <0 || end < 1) //empty block
      //    continue;

      bci.setIndex(start);

      //Walk the bytecodes
      for (TR_J9ByteCode bc = bci.current(); bc != J9BCunknown && bci.currentByteCodeIndex() < end; bc = bci.next()) 
         {
         if (block->getAbsState() != NULL)
            {
            if (traceAbstractInterpretation)
               {
               bci.printByteCode();
               traceMsg(comp(),"\n");
               }
               
            bool successfullyInterpreted = interpretByteCode(block->getAbsState(), bc, bci, block); 

            if (!successfullyInterpreted)
               {
               if (traceAbstractInterpretation)
                  traceMsg(comp(), "Fail to interpret this bytecode!\n");
               return false;
               }
              
            }
         else //Blocks that cannot be reached (dead code)
            {
            if (traceAbstractInterpretation) 
               traceMsg(comp(), "Basic block: #%d does not have Abstract state. Do not interpret byte code.\n",block->getNumber());
            break;
            }
      
         }

      if (traceAbstractInterpretation && block->getAbsState() != NULL ) //trace the abstate of the block after abstract interpretation
         {
         traceMsg(comp(), "Basic Block: %d in %s finishes Abstract Interpretation", block->getNumber(), _idtNode->getName(comp()->trMemory()));
         block->getAbsState()->print(comp(), vp());
         }
      }


   _idtNode->setMethodSummary(_methodSummary);
   _methodSummary->trace(); 
   return true;
   }

//Get the abstract state of the START block of CFG
AbsState* AbsInterpreter::initializeAbsState()
   {  
   //printf("- 1. Abstract Interpreter: Enter method: %s\n", _callerMethodSymbol->signature(comp()->trMemory()));
   AbsState* absState = new (region()) AbsState(region());

   TR_ResolvedMethod *resolvedMethod = _callerMethodSymbol->getResolvedMethod();

   int32_t numberOfParameters = resolvedMethod->numberOfParameters();
   int32_t numberOfExplicitParameters = resolvedMethod->numberOfExplicitParameters();
   int32_t numberOfImplicitParameters = numberOfParameters - numberOfExplicitParameters;

   //set the implicit parameter
   if (numberOfImplicitParameters == 1)
      {
      TR_OpaqueClassBlock *implicitParameterClass = resolvedMethod->containingClass();
      AbsValue* value = AbsValue::createClassObject(implicitParameterClass, true, comp(), region(), vp());
      value->setParamPosition(0);
      value->setImplicitParam();
      absState->set(0, value);
      }

   //setting the rest explicit parameters
   uint32_t paramPos = numberOfImplicitParameters; 
   uint32_t slot = numberOfImplicitParameters;

   for (TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*comp()); !parameterIterator->atEnd(); parameterIterator->advanceCursor(), slot++, paramPos++)
      {
      
      TR::DataType dataType = parameterIterator->getDataType();
      //printf("Param %d, Type %s\n", paramPos, TR::DataType::getName(dataType));
      AbsValue* paramValue;

      switch (dataType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            paramValue = AbsValue::createTopInt(region());
            paramValue->setParamPosition(paramPos);
            absState->set(slot, paramValue);
            break;

         case TR::Int64:
            paramValue = AbsValue::createTopLong(region());
            paramValue->setParamPosition(paramPos);
            absState->set(slot, paramValue);
            slot++;
            absState->set(slot, AbsValue::createDummyLong(region()));
            break;

         case TR::Float:
            paramValue = AbsValue::createTopFloat(region());
            paramValue->setParamPosition(paramPos);
            absState->set(slot, paramValue);
            break;

         case TR::Double:
            paramValue = AbsValue::createTopDouble(region());
            paramValue->setParamPosition(paramPos);
            absState->set(slot, paramValue);
            slot++;
            absState->set(slot, AbsValue::createDummyDouble(region()));
            break;

         case TR::Aggregate:  //TR::Aggregate for parameters (not TR::Address). 
            {
            TR_OpaqueClassBlock *classBlock = parameterIterator->getOpaqueClass();
            if (!classBlock)
               {
               paramValue = AbsValue::createTopObject(region());
               paramValue->setParamPosition(paramPos);
               absState->set(slot,paramValue);
               break;
               }
            else 
               {
               paramValue = AbsValue::createClassObject(classBlock, false, comp(), region(), vp());
               paramValue->setParamPosition(paramPos);
               absState->set(slot, paramValue);
               break;
               }     
            }
            
         default:
            TR_ASSERT(false, "wrong type");
            break;
         }
      }

   return absState;
   }

AbsState* AbsInterpreter::mergeAllPredecessors(TR::Block* block)
   {
   //printf("Merge all predecesor of block %d\n", block->getNumber());
   TR::CFGEdgeList &predecessors = block->getPredecessors();
   AbsState* absState = NULL;

   bool first = true;

   for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i)
      {
      auto *edge = *i;
      TR::Block *aBlock = edge->getFrom()->asBlock();
      TR::Block *check = edge->getTo()->asBlock();
      if (check != block)
         continue;

      if (comp()->trace(OMR::benefitInliner))
         {
         traceMsg(comp(), "Merge Abstract State Predecessor: #%d\n", aBlock->getNumber());
         //absState->trace(vp());
         }
      //printf("Merge Abstract State Predecessor: #%d\n", aBlock->getNumber());

      if (first) 
         {
         first = false;
         absState = new (region()) AbsState(aBlock->getAbsState(), region());

         continue;
         }

      // merge with the rest;
      absState->merge(aBlock->getAbsState(), vp());
      }

      traceMsg(comp(), "Merged Abstract State:\n");
      absState->print(comp(), vp());
      return absState;
   }

void AbsInterpreter::transferAbsStates(TR::Block* block)
   {
   bool traceAbstractInterpretation = comp()->getOption(TR_TraceAbstractInterpretation);
   //printf("-    4. Abstract Interpreter: Transfer abstract states\n");

   if (block->getPredecessors().size() == 0) //has no predecessors
      {
      if (traceAbstractInterpretation)
         traceMsg(comp(), "No predecessors. Stop.\n");
      //printf("No predecessors. Stop.\n");
      return;
      }
      
   //Case 1:
   // A loop in dead code area
   if (block->hasOnlyOnePredecessor() && !block->getPredecessors().front()->getFrom()->asBlock()->getAbsState())
      {
      //printf("      There is a loop. Stop.\n");
      if (traceAbstractInterpretation) 
         traceMsg(comp(), "Loop in dead code area. Stop.\n");
      return;
      }
      
   //Case: 2
   // If we only have one interpreted predecessor.
   if (block->hasOnlyOnePredecessor() && block->getPredecessors().front()->getFrom()->asBlock()->getAbsState()) 
      {
      AbsState *absState = new (region()) AbsState(block->getPredecessors().front()->getFrom()->asBlock()->getAbsState(), region());
      block->setAbsState(absState);
      if (traceAbstractInterpretation) 
         {
         traceMsg(comp(), "There is only one predecessor: #%d and interpreted. Pass this abstract state.\n",block->getPredecessors().front()->getFrom()->asBlock()->getNumber() );
         //absState->trace(vp());
         }
      //printf("      There is only one predecessor: #%d and interpreted. Pass this abstract state.\n",block->getPredecessors().front()->getFrom()->asBlock()->getNumber());
        
      return;
      }

   //Case: 3
   // multiple predecessors...all interpreted
   if (block->hasAbstractInterpretedAllPredecessors()) 
      {
      //printf("      There are multiple predecessors and all interpreted. Merge their abstract states.\n");
      if (traceAbstractInterpretation) 
         traceMsg(comp(), "There are multiple predecessors and all interpreted. Merge their abstract states.\n");

      block->setAbsState( mergeAllPredecessors(block) );
      return;
      }

   //Case: 4
   // we have not interpreted all predecessors...
   // look for a predecessor that has been interpreted
   //printf("      Not all predecessors are interpreted. Finding one interpretd...\n");
   if (traceAbstractInterpretation) 
      traceMsg(comp(), "Not all predecessors are interpreted. Finding one interpretd...\n");
  
   TR::CFGEdgeList &predecessors = block->getPredecessors();
   for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i)
      {
      auto *edge = *i;
      TR::Block *parentBlock = edge->getFrom()->asBlock();
      TR::Block *check = edge->getTo()->asBlock();
      if (check != block)
         {
         if (traceAbstractInterpretation)
            traceMsg(comp(), "fail check\n");
         continue;
         }

      if (!parentBlock->getAbsState())
         continue;
         

      if (traceAbstractInterpretation)
         traceMsg(comp(), "Find a predecessor: Block:#%d interpreted. Use its type info and setting all abstract values to be TOP\n", parentBlock->getNumber());
   
      //printf("      Find a predecessor: #%d interpreted. Use its type info and setting all abstract values to be TOP\n", parentBlock->getNumber());

      // We find a predecessor interpreted. Use its type info with all AbsValues being TOP (unkown)
      AbsState *parentState = parentBlock->getAbsState();

      AbsState *newState = new (region()) AbsState(parentState, region());

      TR::deque<AbsValue*, TR::Region&> deque(comp()->trMemory()->currentStackRegion());

      size_t stackSize = newState->getStackSize();
      for (size_t i = 0; i < stackSize; i++)
         {
         AbsValue *value = newState->pop();
         value->setToTop();
         deque.push_back(value);
         }
         
      for (size_t i = 0; i < stackSize; i++)
         {
         newState->push(deque.back());
         deque.pop_back();
         }
        
      size_t arraySize = newState->getArraySize();

      for (size_t i = 0; i < arraySize; i++)
         {
         if (newState->at(i) != NULL)
            newState->at(i)->setToTop();
         }      

      block->setAbsState(newState);
      return;
      }
      
   if (traceAbstractInterpretation)
      traceMsg(comp(), "No predecessor is interpreted. Stop.\n");
   }



bool AbsInterpreter::interpretByteCode(AbsState* state, TR_J9ByteCode bc, TR_J9ByteCodeIterator &bci, TR::Block* block)
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
   //printf("+Bytecode: %s | %d\n",J9_ByteCode_Strings[bc], bci.nextByte());
   switch(bc)
      {
      case J9BCnop: nop(state); break;

      case J9BCaconstnull: aconstnull(state); break;

      //iconst_x
      case J9BCiconstm1: iconstm1(state); break;
      case J9BCiconst0: iconst0(state); break;
      case J9BCiconst1: iconst1(state); break;
      case J9BCiconst2: iconst2(state); break;
      case J9BCiconst3: iconst3(state); break;
      case J9BCiconst4: iconst4(state); break;
      case J9BCiconst5: iconst5(state); break;

      //lconst_x
      case J9BClconst0: lconst0(state); break;
      case J9BClconst1: lconst1(state); break;

      //fconst_x
      case J9BCfconst0: fconst0(state); break;
      case J9BCfconst1: fconst1(state); break;
      case J9BCfconst2: fconst2(state); break;

      //dconst_x
      case J9BCdconst0: dconst0(state); break;
      case J9BCdconst1: dconst1(state); break;

      //x_push
      case J9BCbipush: bipush(state, bci.nextByteSigned()); break;
      case J9BCsipush: sipush(state, bci.next2BytesSigned()); break;

      //ldc_x
      case J9BCldc: ldc(state, bci.nextByte()); break;
      case J9BCldcw: ldcw(state, bci.next2Bytes()); break;
      case J9BCldc2lw: ldc(state, bci.next2Bytes()); break; //internal bytecode equivalent to ldc2_w
      case J9BCldc2dw: ldc(state, bci.next2Bytes()); break; //internal bytecode equivalent to ldc2_w

      //iload_x
      case J9BCiload: iload(state, bci.nextByte()); break;
      case J9BCiload0: iload0(state); break;
      case J9BCiload1: iload1(state); break;
      case J9BCiload2: iload2(state); break;
      case J9BCiload3: iload3(state); break;
      case J9BCiloadw: iload(state, bci.next2Bytes()); break;

      //lload_x
      case J9BClload: lload(state, bci.nextByte()); break;
      case J9BClload0: lload0(state); break;
      case J9BClload1: lload1(state); break;
      case J9BClload2: lload2(state); break;
      case J9BClload3: lload3(state); break;
      case J9BClloadw: lload(state, bci.next2Bytes()); break;

      //fload_x
      case J9BCfload: fload(state, bci.nextByte()); break;
      case J9BCfload0: fload0(state); break;
      case J9BCfload1: fload1(state); break;
      case J9BCfload2: fload2(state); break;
      case J9BCfload3: fload3(state); break;
      case J9BCfloadw: fload(state,bci.next2Bytes()); break;

      //dload_x
      case J9BCdload: dload(state, bci.nextByte()); break;
      case J9BCdload0: dload0(state); break;
      case J9BCdload1: dload1(state); break;
      case J9BCdload2: dload2(state); break;
      case J9BCdload3: dload3(state); break;
      case J9BCdloadw: dload(state, bci.next2Bytes()); break;

      //aload_x
      case J9BCaload: aload(state, bci.nextByte()); break;
      case J9BCaload0: aload0(state); break;
      case J9BCaload1: aload1(state); break;
      case J9BCaload2: aload2(state); break;
      case J9BCaload3: aload3(state); break;
      case J9BCaloadw: aload(state, bci.next2Bytes()); break;

      //x_aload
      case J9BCiaload: iaload(state); break;
      case J9BClaload: laload(state); break;
      case J9BCfaload: faload(state); break;
      case J9BCaaload: aaload(state); break;
      case J9BCdaload: daload(state); break;
      case J9BCcaload: caload(state); break;
      case J9BCsaload: saload(state); break;
      case J9BCbaload: baload(state); break;

      //istore_x
      case J9BCistore: istore(state, bci.nextByte()); break;
      case J9BCistore0: istore0(state); break;
      case J9BCistore1: istore1(state); break;
      case J9BCistore2: istore2(state); break;
      case J9BCistore3: istore3(state); break;
      case J9BCistorew: istore(state, bci.next2Bytes()); break;

      //lstore_x
      case J9BClstore: lstore(state, bci.nextByte()); break;
      case J9BClstore0: lstore0(state); break;
      case J9BClstore1: lstore1(state); break;
      case J9BClstore2: lstore2(state); break;
      case J9BClstore3: lstore3(state); break;
      case J9BClstorew: lstorew(state, bci.next2Bytes()); break; 

      //fstore_x
      case J9BCfstore: fstore(state, bci.nextByte()); break;
      case J9BCfstore0: fstore0(state); break;
      case J9BCfstore1: fstore1(state); break;
      case J9BCfstore2: fstore2(state); break;
      case J9BCfstore3: fstore3(state); break;
      case J9BCfstorew: fstore(state, bci.next2Bytes()); break; 
      
      //dstore_x
      case J9BCdstore: dstore(state, bci.nextByte()); break;
      case J9BCdstorew: dstore(state, bci.next2Bytes()); break; 
      case J9BCdstore0: dstore0(state); break;
      case J9BCdstore1: dstore1(state); break;
      case J9BCdstore2: dstore2(state); break;
      case J9BCdstore3: dstore3(state); break;

      //astore_x
      case J9BCastore: astore(state, bci.nextByte()); break;
      case J9BCastore0: astore0(state); break;
      case J9BCastore1: astore1(state); break;
      case J9BCastore2: astore2(state); break;
      case J9BCastore3: astore3(state); break;
      case J9BCastorew: astore(state, bci.next2Bytes()); break;

      //x_astore
      case J9BCiastore: iastore(state); break;
      case J9BCfastore: fastore(state); break;
      case J9BCbastore: bastore(state); break;
      case J9BCdastore: dastore(state); break;
      case J9BClastore: lastore(state); break;
      case J9BCsastore: sastore(state); break;
      case J9BCcastore: castore(state); break;
      case J9BCaastore: aastore(state); break;

      //pop_x
      case J9BCpop: pop(state); break;
      case J9BCpop2: pop2(state); break;

      //dup_x
      case J9BCdup: dup(state); break;
      case J9BCdupx1: dupx1(state); break;
      case J9BCdupx2: dupx2(state); break;
      case J9BCdup2: dup2(state); break;
      case J9BCdup2x1: dup2x1(state); break;
      case J9BCdup2x2: dup2x2(state); break;

      case J9BCswap: swap(state); break;

      //x_add
      case J9BCiadd: iadd(state); break;
      case J9BCdadd: dadd(state); break;
      case J9BCfadd: fadd(state); break;
      case J9BCladd: ladd(state); break;

      //x_sub
      case J9BCisub: isub(state); break;
      case J9BCdsub: dsub(state); break;
      case J9BCfsub: fsub(state); break;
      case J9BClsub: lsub(state); break;

      //x_mul
      case J9BCimul: imul(state); break;
      case J9BClmul: lmul(state); break;
      case J9BCfmul: fmul(state); break;
      case J9BCdmul: dmul(state); break;

      //x_div
      case J9BCidiv: idiv(state); break;
      case J9BCddiv: ddiv(state); break;
      case J9BCfdiv: fdiv(state); break;
      case J9BCldiv: ldiv(state); break;

      //x_rem
      case J9BCirem: irem(state); break;
      case J9BCfrem: frem(state); break;
      case J9BClrem: lrem(state); break;
      case J9BCdrem: drem(state); break;

      //x_neg
      case J9BCineg: ineg(state); break;
      case J9BCfneg: fneg(state); break;
      case J9BClneg: lneg(state); break;
      case J9BCdneg: dneg(state); break;

      //x_sh_x
      case J9BCishl: ishl(state); break;
      case J9BCishr: ishr(state); break;
      case J9BCiushr: iushr(state); break;
      case J9BClshl: lshl(state); break;
      case J9BClshr: lshr(state); break;
      case J9BClushr: lushr(state); break;

      //x_and
      case J9BCiand: iand(state); break;
      case J9BCland: land(state); break;

      //x_or
      case J9BCior: ior(state); break;
      case J9BClor: lor(state); break;

      //x_xor
      case J9BCixor: ixor(state); break;
      case J9BClxor: lxor(state); break;

      //iinc_x
      case J9BCiinc: iinc(state, bci.nextByte(), bci.nextByteSigned()); break;
      case J9BCiincw: iinc(state, bci.next2Bytes(), bci.next2BytesSigned()); break;

      //i2_x
      case J9BCi2b: i2b(state); break;
      case J9BCi2c: i2c(state); break;
      case J9BCi2d: i2d(state); break;
      case J9BCi2f: i2f(state); break;
      case J9BCi2l: i2l(state); break;
      case J9BCi2s: i2s(state); break;
      
      //l2_x
      case J9BCl2d: l2d(state); break;
      case J9BCl2f: l2f(state); break;
      case J9BCl2i: l2i(state); break;

      //d2_x
      case J9BCd2f: d2f(state); break;
      case J9BCd2i: d2i(state); break;
      case J9BCd2l: d2l(state); break;

      //f2_x
      case J9BCf2d: f2d(state); break;
      case J9BCf2i: f2i(state); break;
      case J9BCf2l: f2l(state); break;

      //x_cmp_x
      case J9BCdcmpl: dcmpl(state); break;
      case J9BCdcmpg: dcmpg(state); break;
      case J9BCfcmpl: fcmpl(state); break;
      case J9BCfcmpg: fcmpg(state); break;
      case J9BClcmp: lcmp(state); break;

      //if_x
      case J9BCifeq: ifeq(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifge: ifge(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifgt: ifgt(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifle: ifle(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCiflt: iflt(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifne: ifne(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;

      //if_x_null
      case J9BCifnonnull: ifnonnull(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCifnull: ifnull(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;

      //ificmp_x
      case J9BCificmpeq: ificmpeq(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmpge: ificmpge(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmpgt: ificmpgt(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmple: ificmple(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmplt: ificmplt(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;
      case J9BCificmpne: ificmpne(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;

      //ifacmp_x
      case J9BCifacmpeq: ifacmpeq(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break; 
      case J9BCifacmpne: ifacmpne(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break; 

      //goto_x
      case J9BCgoto: _goto(state, bci.next2BytesSigned()); break;
      case J9BCgotow: _gotow(state, bci.next4BytesSigned()); break;

      //x_switch
      case J9BClookupswitch: lookupswitch(state); break;
      case J9BCtableswitch: tableswitch(state); break;

      //get_x
      case J9BCgetfield: getfield(state, bci.next2Bytes()); break;
      case J9BCgetstatic: getstatic(state, bci.next2Bytes()); break;

      //put_x
      case J9BCputfield: putfield(state, bci.next2Bytes()); break;
      case J9BCputstatic: putstatic(state, bci.next2Bytes()); break;

      //x_newarray
      case J9BCnewarray: newarray(state, bci.nextByte()); break;
      case J9BCanewarray: anewarray(state, bci.next2Bytes()); break;
      case J9BCmultianewarray: multianewarray(state, bci.next2Bytes(), bci.nextByte(3)); break;

      //monitor_x
      case J9BCmonitorenter: monitorenter(state); break;
      case J9BCmonitorexit: monitorexit(state); break;
      
      case J9BCnew: _new(state, bci.next2Bytes()); break;

      case J9BCarraylength: arraylength(state); break;

      case J9BCathrow: athrow(state); break;
      
      case J9BCcheckcast: checkcast(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;

      case J9BCinstanceof: instanceof(state, bci.next2Bytes(), bci.currentByteCodeIndex()); break;

      case J9BCwide: /* does this need to be handled? */ break;

      //invoke_x
      case J9BCinvokedynamic: return false; break; //Encounter an invokedynamic. 
      case J9BCinvokeinterface: invokeinterface(state, bci.currentByteCodeIndex(), bci.next2Bytes(), block); break;
      case J9BCinvokeinterface2: /*how should we handle invokeinterface2? */ break;
      case J9BCinvokespecial: invokespecial(state, bci.currentByteCodeIndex(), bci.next2Bytes(),block); break;
      case J9BCinvokestatic: invokestatic(state, bci.currentByteCodeIndex(), bci.next2Bytes(), block); break;
      case J9BCinvokevirtual: invokevirtual(state, bci.currentByteCodeIndex(), bci.next2Bytes(),block);break;
      case J9BCinvokespecialsplit: invokespecial(state, bci.currentByteCodeIndex(), bci.next2Bytes() | J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG, block); break;
      case J9BCinvokestaticsplit: invokestatic(state, bci.currentByteCodeIndex(), bci.next2Bytes() | J9_STATIC_SPLIT_TABLE_INDEX_FLAG, block); break;
   
      //return_x: to be implemented in the future for backward analysis
      case J9BCReturnC: state->pop(); break;
      case J9BCReturnS: state->pop(); break;
      case J9BCReturnB: state->pop(); break;
      case J9BCReturnZ: state->pop(); break;
      case J9BCgenericReturn: state->getStackSize() != 0 ? state->pop() && state->getStackSize() && state->pop() : 0; break; 
      
      default:
      //printf("%s\n", J9_ByteCode_Strings[bc]);
      break;
      }

   return true;
   }

//TODO: Add Type of the array to the contraint
//-- Checked
AbsState* AbsInterpreter::multianewarray(AbsState* absState, int cpIndex, int dimensions)
   {
   TR_OpaqueClassBlock* arrayType = _callerMethod->getClassFromConstantPool(comp(), cpIndex);

   //get the outer-most length
   for (int i = 0; i < dimensions-1; i++)
      {
      absState->pop();
      }

   AbsValue* length = absState->pop(); 

   TR::VPNonNullObject *presence = TR::VPNonNullObject::create(vp());

   if (length->hasConstraint())
      {
      if (length->getConstraint()->asIntConstraint())
         {
         AbsValue* value = AbsValue::createArrayObject(
                              arrayType,
                              true,
                              length->getConstraint()->asIntConstraint()->getLowInt(),
                              length->getConstraint()->asIntConstraint()->getHighInt(),
                              4,
                              comp(),
                              region(),
                              vp());
         absState->push(value);
         return absState;
         }
      }

   AbsValue* value = AbsValue::createArrayObject(
                        arrayType,
                        true,
                        0,
                        INT32_MAX,
                        4,
                        comp(),
                        region(),
                        vp());
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::caload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value = AbsValue::createTopInt(region());
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::faload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value = AbsValue::createTopFloat(region());
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iaload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value = AbsValue::createTopInt(region());
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::saload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   AbsValue *value = AbsValue::createTopInt(region());
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::aaload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   AbsValue *value = AbsValue::createTopObject(region());
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::laload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   AbsValue *value1 = AbsValue::createTopLong(region());
   AbsValue *value2 = AbsValue::createDummyLong(region());
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::daload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   AbsValue *value1 = AbsValue::createTopDouble(region());
   AbsValue *value2 = AbsValue::createDummyDouble(region());
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::castore(AbsState* absState)
   {
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *arrayRef = absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dastore(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fastore(AbsState* absState)
   {
   //TODO:
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iastore(AbsState* absState)
   {
   
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lastore(AbsState* absState)
   {
   AbsValue *value = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::sastore(AbsState* absState)
   {
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::aastore(AbsState* absState)
   {
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::aconstnull(AbsState* absState) 
   {
   AbsValue *value = AbsValue::createNullObject(region(), vp());
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::aload(AbsState* absState, int n)
   {
   AbsValue *value = absState->at(n);
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::aload0(AbsState* absState) 
   {
   aload(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::aload1(AbsState* absState) 
   {
   aload(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::aload2(AbsState* absState) 
   {
   aload(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::aload3(AbsState* absState) 
   {
   aload(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::astore(AbsState* absState, int index)
   {
   AbsValue* value = absState->pop();
   absState->set(index, value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::astore0(AbsState* absState) 
   {
   astore(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::astore1(AbsState* absState)
   {
   astore(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::astore2(AbsState* absState) 
   {
   astore(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::astore3(AbsState* absState) 
   {
   astore(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::bipush(AbsState* absState, int byte) 
   {
   AbsValue *value = AbsValue::createIntConst(byte, region(), vp());
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::bastore(AbsState* absState) 
   {
   AbsValue *value = absState->pop();
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::baload(AbsState* absState)
   {
   AbsValue *index = absState->pop();
   AbsValue *ref = absState->pop();
   AbsValue *value = AbsValue::createTopInt(region());
   absState->push(value);
   return absState;
   }

//TODO: checkcast array types, primitive arrays
//-- Checked
AbsState* AbsInterpreter::checkcast(AbsState* absState, int cpIndex, int bytecodeIndex) 
   {
   AbsValue *objRef = absState->pop();

   TR_OpaqueClassBlock* classBlock = _callerMethod->getClassFromConstantPool(comp(), cpIndex);

   //adding to method summary
   if ( objRef->isParameter() )
      {
      _methodSummary->addCheckCast(objRef->getParamPosition(), classBlock);
      }

   if (objRef->hasConstraint())
      {
      if (objRef->getConstraint()->asNullObject()) //Check cast null object, always succeed
         {
         absState->push(objRef);
         return absState;
         }

      if (objRef->getConstraint()->asClass()) //check cast object
         {
         TR::VPClass *classConstraint = objRef->getConstraint()->asClass();
         TR::VPClassType *classType = classConstraint->getClassType();
         if (classType && classType->getClass() && classBlock)
            {
            TR_YesNoMaybe yesNoMaybe = comp()->fe()->isInstanceOf(classType->getClass(), classBlock, true, true);
            if (yesNoMaybe == TR_yes)
               {
               if (classBlock == classType->getClass()) //cast into the same type, no change
                  {
                  absState->push(objRef);
                  return absState;
                  }
               else //can cast into a different type
                  {
                  absState->push(AbsValue::createClassObject(classBlock, true, comp(), region(), vp()));
                  return absState;   
                  }
               }
            }
         }
      }

   absState->push(AbsValue::createTopObject(region()));
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dup(AbsState* absState) 
   {
   AbsValue *value = absState->pop();
   absState->push(value);
   absState->push(new (region()) AbsValue(value)); 
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dupx1(AbsState* absState) 
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   absState->push(value1);
   absState->push(value2);
   absState->push(new (region()) AbsValue(value1));
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dupx2(AbsState* absState) 
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   absState->push(value1);
   absState->push(value3);
   absState->push(value2);
   absState->push(new (region()) AbsValue(value1));
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dup2(AbsState* absState) 
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   absState->push(value2);
   absState->push(value1);
   absState->push(new (region()) AbsValue(value2));
   absState->push(new (region()) AbsValue(value1));
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dup2x1(AbsState *absState) 
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   AbsValue *value3 = absState->pop();
   absState->push(value2);
   absState->push(value1);
   absState->push(value3);
   absState->push(new (region()) AbsValue(value2));
   absState->push(new (region()) AbsValue(value1));
   return absState;
   }

//-- Checked
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
   absState->push(new (region()) AbsValue(value2));
   absState->push(new (region()) AbsValue(value1));
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::_goto(AbsState* absState, int branch)
   {
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::_gotow(AbsState* absState, int branch)
   {
   return absState;
   }

//TODO: get object with actual type (Not just TR::Address)
//-- Checked
AbsState* AbsInterpreter::getstatic(AbsState* absState, int cpIndex) 
   {
   void* staticAddress;

   TR::DataType type;

   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   _callerMethod->staticAttributes(
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

   switch (type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         absState->push(AbsValue::createTopInt(region()));
         return absState;
      case TR::Float:
         absState->push(AbsValue::createTopFloat(region()));
         return absState;
      case TR::Double:
         absState->push(AbsValue::createTopDouble(region()));
         absState->push(AbsValue::createDummyDouble(region()));
         return absState;
      case TR::Int64:
         absState->push(AbsValue::createTopLong(region()));
         absState->push(AbsValue::createDummyLong(region()));
         return absState;
      case TR::Address:
         absState->push(AbsValue::createTopObject(region()));
         return absState;
      default:
         break;
      }

   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::getfield(AbsState* absState, int cpIndex)
   {
   if (absState->top()->isParameter() && !absState->top()->isImplicitParam()) //implict param won't be null checked.
      {
      _methodSummary->addNullCheck(absState->top()->getParamPosition());
      }

   AbsValue *objectref = absState->pop();
   uint32_t fieldOffset;
   TR::DataType type;

   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   bool isResolved = _callerMethod->fieldAttributes(
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
   
   switch (type)
      {
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
         absState->push(AbsValue::createTopInt(region()));
         return absState;
      case TR::Float:
         absState->push(AbsValue::createTopFloat(region()));
         return absState;
      case TR::Double:
         absState->push(AbsValue::createTopDouble(region()));
         absState->push(AbsValue::createDummyDouble(region()));
         return absState;
      case TR::Int64:
         absState->push(AbsValue::createTopLong(region()));
         absState->push(AbsValue::createDummyLong(region()));
         return absState;
      case TR::Address:
         absState->push(AbsValue::createTopObject(region()));
         return absState;
      default:
         break;
      }
   
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iand(AbsState* absState)
   {
   AbsValue* value1 = absState->pop();
   AbsValue* value2 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst()) //int consts
         {
         int32_t resultVal = value1->getConstraint()->asIntConst()->getInt() & value2->getConstraint()->asIntConst()->getInt();
         AbsValue* value = AbsValue::createIntConst(resultVal, region(), vp());
         absState->push(value);
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }


//TODO: instanceof <interface>. Current implementation does not support intereface. And instanceof array types
//-- Checked
AbsState* AbsInterpreter::instanceof(AbsState* absState, int cpIndex, int byteCodeIndex)
   {
   AbsValue *objectRef = absState->pop();

   TR_OpaqueClassBlock *block = _callerMethod->getClassFromConstantPool(comp(), cpIndex); //The cast class to be compared with
   
   //Add to the inlining summary
   if (objectRef->isParameter() )
      {
      _methodSummary->addInstanceOf(objectRef->getParamPosition(), block);
      }
      
   if (objectRef->hasConstraint())
      {
      if (objectRef->getConstraint()->asNullObject()) // is null object. false
         {
         absState->push(AbsValue::createIntConst(0,region(),vp()));
         return absState;
         }

      if (block && objectRef->getConstraint()->getClass()) //have the class types
         {
         if (objectRef->getConstraint()->asClass() || objectRef->getConstraint()->asConstString()) // is class object or string object
            {
            TR_YesNoMaybe yesNoMaybe = comp()->fe()->isInstanceOf(objectRef->getConstraint()->getClass(), block, false, true, true);

            if( yesNoMaybe == TR_yes) //Instanceof must be true;
               {
               absState->push(AbsValue::createIntConst(1,region(),vp()));
               return absState;
               } 
            else if (yesNoMaybe = TR_no) //Instanceof must be false;
               {
               absState->push(AbsValue::createIntConst(0,region(),vp()));
               return absState;
               }
            }
         }
      }

   absState->push(AbsValue::createIntRange(0,1,region(),vp()));
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ior(AbsState* absState)
   {
   AbsValue *value2 = absState->pop();
   AbsValue* value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();
   if (hasConstraint)
      {
      if (value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst()) //both int consts
         {
         int32_t resultVal = value1->getConstraint()->asIntConst()->getInt() | value2->getConstraint()->asIntConst()->getInt();
         AbsValue* result = AbsValue::createIntConst(resultVal, region(), vp());
         absState->push(result);
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ixor(AbsState* absState) 
   {
   AbsValue *value2 = absState->pop();
   AbsValue* value1 = absState->pop();
   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst())
         {
         int32_t resultVal = value1->getConstraint()->asIntConst()->getInt() ^ value2->getConstraint()->asIntConst()->getInt();
         absState->push(AbsValue::createIntConst(resultVal, region(), vp()));
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::irem(AbsState* absState)
   {
   AbsValue *value2 = absState->pop();
   AbsValue* value1 = absState->pop();
   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if ( value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst())
         {
         int32_t int1 = value1->getConstraint()->asIntConst()->getInt();
         int32_t int2 = value2->getConstraint()->asIntConst()->getInt();
         int32_t resultVal = int1 % int2;
         absState->push(AbsValue::createIntConst(resultVal, region(), vp()));
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ishl(AbsState* absState)
   {
   AbsValue* length = absState->pop();
   AbsValue* value = absState->pop();

   bool hasConstraint = length->hasConstraint() && value->hasConstraint();

   if (hasConstraint)
      {
      if (length->getConstraint()->asIntConst() && value->getConstraint()->asIntConst()) //Int consts
         {
         int32_t lengthVal = length->getConstraint()->asIntConst()->getInt(); 
         int32_t valueVal = value->getConstraint()->asIntConst()->getInt();
         int32_t resultVal = valueVal << lengthVal;
         AbsValue* result = AbsValue::createIntConst(resultVal, region(), vp());
         absState->push(result);
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ishr(AbsState* absState)
   {
   AbsValue* length = absState->pop();
   AbsValue* value = absState->pop();

   bool hasConstraint = value->hasConstraint() && length->hasConstraint();
   if (hasConstraint)
      {
      if (length->getConstraint()->asIntConst() && value->getConstraint()->asIntConst()) //Int consts
         {
         int32_t lengthVal = length->getConstraint()->asIntConst()->getInt(); 
         int32_t valueVal = value->getConstraint()->asIntConst()->getInt();
         int32_t resultVal = valueVal >> lengthVal;
         AbsValue* result = AbsValue::createIntConst(resultVal, region(), vp());
         absState->push(result);
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iushr(AbsState* absState)
   {
   AbsValue* length = absState->pop();
   AbsValue* value = absState->pop();

   bool hasConstraint = length->hasConstraint() && value->hasConstraint();
   if (hasConstraint)
      {
      if (length->getConstraint()->asIntConst() && value->getConstraint()->asIntConst()) //Int consts
         {
         int32_t lengthVal = length->getConstraint()->asIntConst()->getInt(); //can be greater than 32
         int32_t valueVal = value->getConstraint()->asIntConst()->getInt();
         int32_t resultVal = (uint32_t) valueVal >> lengthVal;
         AbsValue* result = AbsValue::createIntConst(resultVal, region(), vp());
         absState->push(result);
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
//TODO: range div
AbsState* AbsInterpreter::idiv(AbsState* absState)
   {
   AbsValue* value2 = absState->pop();
   AbsValue* value1 = absState->pop();
   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();
   if (hasConstraint)
      {
      if (value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst())
         {
         int32_t int1 = value1->getConstraint()->asIntConst()->getInt();
         int32_t int2 = value2->getConstraint()->asIntConst()->getInt();
         if (int2 == 0) //throw exception
            {
            AbsValue *result = AbsValue::createTopInt(region());
            absState->push(result);
            return absState;
            }
         
         int32_t resultVal = int1/int2;
         absState->push(AbsValue::createIntConst(resultVal, region(), vp()));
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
//TODO: range mul
AbsState* AbsInterpreter::imul(AbsState* absState)
   {
   AbsValue* value2 = absState->pop();
   AbsValue* value1 = absState->pop();
   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();
   if (hasConstraint)
      {
      if (value1->getConstraint()->asIntConst() && value2->getConstraint()->asIntConst())
         {
         int32_t int1 = value1->getConstraint()->asIntConst()->getInt();
         int32_t int2 = value2->getConstraint()->asIntConst()->getInt();
         
         int32_t resultVal = int1 * int2;
         absState->push(AbsValue::createIntConst(resultVal, region(), vp()));
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ineg(AbsState* absState)
   {
   AbsValue *value = absState->pop();

   if (value->hasConstraint())
      {
      if (value->getConstraint()->asIntConst()) //const
         {
         AbsValue* result = AbsValue::createIntConst( -value->getConstraint()->asIntConst()->getInt(), region(), vp());
         absState->push(result);
         return absState;
         }

      if (value->getConstraint()->asIntRange()) //range
         {
         AbsValue* result = AbsValue::createIntRange(-value->getConstraint()->getHighInt(), -value->getConstraint()->getLowInt(), region(), vp());
         absState->push(result);
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iconstm1(AbsState* absState)
   {
   iconst(absState, -1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iconst0(AbsState* absState) 
   {
   iconst(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iconst1(AbsState* absState)
   {
   iconst(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iconst2(AbsState* absState)
   {
   iconst(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iconst3(AbsState* absState)
   {
   iconst(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iconst4(AbsState* absState) 
   {
   iconst(absState, 4);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iconst5(AbsState* absState)
   {
   iconst(absState, 5);
   return absState;
   }

//-- Checked
void AbsInterpreter::iconst(AbsState* absState, int n)
   {
   absState->push(AbsValue::createIntConst(n, region(), vp()));
   }

//-- Checked
AbsState* AbsInterpreter::ifeq(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   AbsValue *absValue = absState->top();
   if (absValue->isParameter())
      _methodSummary->addIfEq(absValue->getParamPosition());
      
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ifne(AbsState* absState, int branchOffset, int bytecodeIndex)
   {

   AbsValue* absValue = absState->top();
   if (absValue->isParameter())
      _methodSummary->addIfNe(absValue->getParamPosition());
      
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iflt(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   AbsValue* absValue = absState->top();
   if (absValue->isParameter())
      _methodSummary->addIfLt(absValue->getParamPosition());
      
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ifle(AbsState* absState, int branchOffset, int bytecodeIndex)
   {

   AbsValue* absValue = absState->top();
   if (absValue->isParameter())
      _methodSummary->addIfLe( absValue->getParamPosition());
      
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ifgt(AbsState* absState, int branchOffset, int bytecodeIndex)
   {

   AbsValue* absValue = absState->top();
   if (absValue->isParameter())
      _methodSummary->addIfGt(absValue->getParamPosition());
      
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ifge(AbsState* absState, int branchOffset, int bytecodeIndex) 
   {
   AbsValue* absValue = absState->top();
   if (absValue->isParameter())
      _methodSummary->addIfGe(absValue->getParamPosition());
      
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ifnull(AbsState* absState, int branchOffset, int bytecodeIndex) 
   {
   AbsValue* absValue = absState->top();
   if (absValue->isParameter())
      _methodSummary->addIfNull(absValue->getParamPosition());

   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ifnonnull(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   AbsValue* absValue = absState->top();
   if (absValue->isParameter())
      _methodSummary->addIfNonNull(absValue->getParamPosition());

   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ificmpge(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ificmpeq(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ificmpne(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ificmplt(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ificmpgt(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ificmple(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ifacmpeq(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ifacmpne(AbsState* absState, int branchOffset, int bytecodeIndex)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iload(AbsState* absState, int n)
   {
   AbsValue *value = absState->at(n);
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iload0(AbsState* absState) 
   {
   iload(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iload1(AbsState* absState)
   {
   iload(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iload2(AbsState* absState)
   {
   iload(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iload3(AbsState* absState)
   {
   iload(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::istore(AbsState* absState, int n)
   {
   absState->set(n, absState->pop());
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::istore0(AbsState* absState)
   {
   istore(absState,0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::istore1(AbsState* absState)
   {
   istore(absState,1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::istore2(AbsState* absState)
   {
   istore(absState,2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::istore3(AbsState* absState)
   {
   istore(absState,3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::isub(AbsState* absState)
   {
   AbsValue *value2 = absState->pop();
   AbsValue *value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();
   if (hasConstraint)
      {
      if (value1->getConstraint()->asIntConstraint() && value2->getConstraint()->asIntConstraint()) 
         {
         AbsValue* result = AbsValue::create(value1->getConstraint()->subtract(value2->getConstraint(), TR::Int32, vp()), TR::Int32, region());
         absState->push(result);
         return absState;
         }
      }

   AbsValue* result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iadd(AbsState* absState)
   {
   AbsValue *value2 = absState->pop();
   AbsValue *value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();
   if (hasConstraint)
      {
      if (value1->getConstraint()->asIntConstraint() && value2->getConstraint()->asIntConstraint())
         {
         AbsValue* result =  AbsValue::create(value1->getConstraint()->add(value2->getConstraint(), TR::Int32, vp()), TR::Int32, region());
         absState->push(result);
         return absState;
         }
      }

   AbsValue* result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::i2d(AbsState* absState)
   {
   AbsValue *value = absState->pop();
   AbsValue *result1 = AbsValue::createTopDouble(region());
   AbsValue *result2 = AbsValue::createDummyDouble(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::i2f(AbsState* absState)
   {
   AbsValue *value = absState->pop();
   AbsValue *result = AbsValue::createTopFloat(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::i2l(AbsState* absState) 
   {
   AbsValue *value = absState->pop();

   if (value->hasConstraint())
      {
      if (value->getConstraint()->asIntConst()) //int const
         {
         AbsValue *result1 = AbsValue::createLongConst((int64_t)value->getConstraint()->asIntConst()->getInt(), region(), vp());
         AbsValue *result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }

      if (value->getConstraint()->asIntRange()) //int range
         {
         int64_t low = (int64_t) value->getConstraint()->getLowInt();
         int64_t high = (int64_t) value->getConstraint()->getHighInt(); 
         AbsValue* result1 = AbsValue::createLongRange(low, high, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::i2s(AbsState* absState)
   {
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::i2c(AbsState* absState)
   {
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::i2b(AbsState* absState)
   {
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dadd(AbsState* absState)
   {
   absState->pop();
   AbsValue *value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();
   AbsValue *result1 = AbsValue::createTopDouble(region());
   AbsValue *result2 = AbsValue::createDummyDouble(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dsub(AbsState* absState)
   {
   absState->pop();
   AbsValue *value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();
   AbsValue *result1 = AbsValue::createTopDouble(region());
   AbsValue *result2 = AbsValue::createDummyDouble(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fsub(AbsState* absState)
   {
   AbsValue *value2 = absState->pop();
   AbsValue *value1 = absState->pop();
   AbsValue *result = AbsValue::createTopFloat(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fadd(AbsState* absState)
   {
   AbsValue *value2 = absState->pop();
   AbsValue *value1 = absState->pop();
   AbsValue *result = AbsValue::createTopFloat(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ladd(AbsState* absState)
   {
   absState->pop();
   AbsValue *value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();
   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();
   if (hasConstraint)
      {
      if (value1->getConstraint()->asLongConstraint() && value2->getConstraint()->asLongConstraint())
         {
         AbsValue* result1 = AbsValue::create(value1->getConstraint()->add(value2->getConstraint(), TR::Int64, vp()), TR::Int64, region());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue* result1 = AbsValue::createTopLong(region());
   AbsValue* result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lsub(AbsState* absState)
   {
   absState->pop();
   AbsValue *value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();
   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asLongConstraint() && value2->getConstraint()->asLongConstraint())
         {
         AbsValue* result1 = AbsValue::create(value1->getConstraint()->subtract(value2->getConstraint(), TR::Int64, vp()), TR::Int64, region());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue* result1 = AbsValue::createTopLong(region());
   AbsValue* result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::l2i(AbsState* absState) 
   {
   absState->pop();
   AbsValue *value = absState->pop();

   bool hasConstraint = value->hasConstraint();
   if (hasConstraint)
      {
      if (value->getConstraint()->asLongConst()) //long const
         {
         int64_t longVal = value->getConstraint()->asLongConst()->getLong();
         AbsValue* result = AbsValue::createIntConst((int32_t)longVal, region(), vp());
         absState->push(result);
         return absState;
         }

      if (value->getConstraint()->asLongRange()) //long ranges
         {
         int64_t longValLow = value->getConstraint()->getLowLong();
         int64_t longValHigh = value->getConstraint()->getHighLong();
         AbsValue* result = AbsValue::createLongRange((int32_t)longValLow, (int32_t)longValHigh, region(), vp());
         absState->push(result);
         return absState;
         }
      }

   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::land(AbsState* absState)
   {
   absState->pop();
   AbsValue* value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asLongConst() && value2->getConstraint()->asLongConst()) //long consts
         {
         int64_t longVal1 = value1->getConstraint()->asLongConst()->getLong();
         int64_t longVal2 = value2->getConstraint()->asLongConst()->getLong();
         int64_t resultVal = longVal1 & longVal2;
         AbsValue* result1 = AbsValue::createLongConst(resultVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::ldiv(AbsState* absState)
   {
   absState->pop();
   AbsValue* value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asLongConst() && value2->getConstraint()->asLongConst()) //long consts
         {
         int64_t longVal1 = value1->getConstraint()->asLongConst()->getLong();
         int64_t longVal2 = value2->getConstraint()->asLongConst()->getLong();
         int64_t resultVal = longVal1 / longVal2;
         AbsValue* result1 = AbsValue::createLongConst(resultVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lmul(AbsState* absState)
   {
   absState->pop();
   AbsValue* value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asLongConst() && value2->getConstraint()->asLongConst()) //long consts
         {
         int64_t longVal1 = value1->getConstraint()->asLongConst()->getLong();
         int64_t longVal2 = value2->getConstraint()->asLongConst()->getLong();
         int64_t resultVal = longVal1 * longVal2;
         AbsValue* result1 = AbsValue::createLongConst(resultVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lookupswitch(AbsState* absState)
   {
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::tableswitch(AbsState* absState)
   {
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lneg(AbsState* absState)
   {
   absState->pop();
   AbsValue *value = absState->pop();
   
   if (value->hasConstraint())
      {
      if (value->getConstraint()->asLongConst()) //long const
         {
         int64_t longVal = value->getConstraint()->asLongConst()->getLong();
         AbsValue* result1 = AbsValue::createLongConst(-longVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }

      if (value->getConstraint()->asLongRange()) //long range
         {
         int64_t longValLow = value->getConstraint()->getLowLong();
         int64_t longValHigh = value->getConstraint()->getHighLong();
         AbsValue* result1 = AbsValue::createLongRange(-longValHigh, -longValLow, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lor(AbsState* absState)
   {
   absState->pop();
   AbsValue* value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asLongConst() && value2->getConstraint()->asLongConst()) //long consts
         {
         int64_t longVal1 = value1->getConstraint()->asLongConst()->getLong();
         int64_t longVal2 = value2->getConstraint()->asLongConst()->getLong();
         int64_t resultVal = longVal1 | longVal2;
         AbsValue* result1 = AbsValue::createLongConst(resultVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lrem(AbsState* absState)
   {
   absState->pop();
   AbsValue* value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asLongConst() && value2->getConstraint()->asLongConst()) //long consts
         {
         int64_t longVal1 = value1->getConstraint()->asLongConst()->getLong();
         int64_t longVal2 = value2->getConstraint()->asLongConst()->getLong();
         int64_t resultVal = longVal1 % longVal2;
         AbsValue* result1 = AbsValue::createLongConst(resultVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }


//-- Checked
AbsState* AbsInterpreter::lshl(AbsState* absState)
   {
   AbsValue* length = absState->pop(); 
   absState->pop();
   AbsValue* value = absState->pop();

   bool hasConstraint = length->hasConstraint() && value->hasConstraint();
   
   if (hasConstraint)
      {
      if (length->getConstraint()->asIntConst() && value->getConstraint()->asLongConst()) //Long const
         {
         int32_t lengthVal = length->getConstraint()->asIntConst()->getInt(); 
         int64_t valueVal = value->getConstraint()->asLongConst()->getLong();
         int64_t resultVal = valueVal << lengthVal;
         AbsValue* result1 = AbsValue::createLongConst(resultVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }


//-- Checked
AbsState* AbsInterpreter::lshr(AbsState* absState)
   {
   AbsValue* length = absState->pop(); 
   absState->pop();
   AbsValue* value = absState->pop();

   bool hasConstraint = length->hasConstraint() && value->hasConstraint();
   
   if (hasConstraint)
      {
      if (length->getConstraint()->asIntConst() && value->getConstraint()->asLongConst()) //Long const
         {
         int32_t lengthVal = length->getConstraint()->asIntConst()->getInt(); 
         int64_t valueVal = value->getConstraint()->asLongConst()->getLong();
         int64_t resultVal = valueVal >> lengthVal;
         AbsValue* result1 = AbsValue::createLongConst(resultVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }


//-- Checked
AbsState* AbsInterpreter::lushr(AbsState* absState)
   {
   AbsValue* length = absState->pop(); 
   absState->pop();
   AbsValue* value = absState->pop();

   bool hasConstraint = length->hasConstraint() && value->hasConstraint();
   
   if (hasConstraint)
      {
      if (length->getConstraint()->asIntConst() && value->getConstraint()->asLongConst()) //Long const
         {
         int32_t lengthVal = length->getConstraint()->asIntConst()->getInt(); 
         int64_t valueVal = value->getConstraint()->asLongConst()->getLong();
         int64_t resultVal = (uint64_t)valueVal >> lengthVal;
         AbsValue* result1 = AbsValue::createLongConst(resultVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }


//-- Checked
AbsState* AbsInterpreter::lxor(AbsState* absState)
   {
   absState->pop();
   AbsValue* value2 = absState->pop();
   absState->pop();
   AbsValue *value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asLongConst() && value2->getConstraint()->asLongConst()) //long consts
         {
         int64_t longVal1 = value1->getConstraint()->asLongConst()->getLong();
         int64_t longVal2 = value2->getConstraint()->asLongConst()->getLong();
         int64_t resultVal = longVal1 ^ longVal2;
         AbsValue* result1 =  AbsValue::createLongConst(resultVal, region(), vp());
         AbsValue* result2 = AbsValue::createDummyLong(region());
         absState->push(result1);
         absState->push(result2);
         return absState;
         }
      }

   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::l2d(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result1 = AbsValue::createTopDouble(region());
   AbsValue *result2 = AbsValue::createDummyDouble(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::l2f(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result = AbsValue::createTopFloat(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::d2f(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result = AbsValue::createTopFloat(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::f2d(AbsState* absState)
   {
   absState->pop();
   AbsValue *result1 = AbsValue::createTopDouble(region());
   AbsValue *result2 = AbsValue::createDummyDouble(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::f2i(AbsState* absState)
   {
   absState->pop();
   AbsValue *result1 = AbsValue::createTopInt(region());
   absState->push(result1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::f2l(AbsState* absState)
   {
   absState->pop();
   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::d2i(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::d2l(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result1 = AbsValue::createTopLong(region());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lload(AbsState* absState, int n)
   {
   AbsValue *value1 = absState->at(n);
   AbsValue *value2 = absState->at(n+1);
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lload0(AbsState* absState)
   {
   lload(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lload1(AbsState* absState)
   {
   lload(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lload2(AbsState* absState)
   {
   lload(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lload3(AbsState* absState)
   {
   lload(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dconst0(AbsState* absState)
   {
   AbsValue *result1 = AbsValue::createTopDouble(region());
   AbsValue *result2 = AbsValue::createDummyDouble(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dconst1(AbsState* absState)
   {
   AbsValue *result1 = AbsValue::createTopDouble(region());
   AbsValue *result2 = AbsValue::createDummyDouble(region());
   absState->push(result1);
   absState->push(result2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fconst0(AbsState* absState)
   {
   AbsValue *result1 = AbsValue::createTopFloat(region());
   absState->push(result1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fconst1(AbsState* absState)
   {
   AbsValue *result1 = AbsValue::createTopFloat(region());
   absState->push(result1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fconst2(AbsState* absState)
   {
   AbsValue *result1 = AbsValue::createTopFloat(region());
   absState->push(result1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dload(AbsState* absState, int n)
   {
   AbsValue *value1 = absState->at(n);
   AbsValue *value2 = absState->at(n+1);
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dload0(AbsState* absState)
   {
   dload(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dload1(AbsState* absState)
   {
   dload(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dload2(AbsState* absState)
   {
   dload(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dload3(AbsState* absState)
   {
   dload(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lstorew(AbsState* absState, int n)
   {
   lstore(absState, n);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lstore0(AbsState* absState)
   {
   lstore(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lstore1(AbsState* absState)
   {
   lstore(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lstore2(AbsState* absState)
   {
   lstore(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lstore3(AbsState* absState)
   {
   lstore(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lstore(AbsState* absState, int n)
   {
   AbsValue *value2 = absState->pop();
   AbsValue *value1 = absState->pop();
   absState->set(n, value1);
   absState->set(n + 1, value2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dstore(AbsState* absState, int n)
   {
   AbsValue *value2 = absState->pop();
   AbsValue *value1 = absState->pop();
   absState->set(n, value1);
   absState->set(n + 1, value2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dstore0(AbsState* absState)
   {
   dstore(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dstore1(AbsState* absState)
   {
   dstore(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dstore2(AbsState* absState)
   {
   dstore(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dstore3(AbsState* absState)
   {
   dstore(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lconst0(AbsState* absState)
   {
   AbsValue* value1 = AbsValue::createLongConst(0, region(), vp());
   AbsValue* value2 = AbsValue::createDummyLong(region());
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lconst1(AbsState* absState)
   {
   AbsValue* value1 = AbsValue::createLongConst(1, region(), vp());
   AbsValue* value2 = AbsValue::createDummyLong(region());
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::lcmp(AbsState* absState)
   {
   absState->pop();
   AbsValue* value2 = absState->pop();
   absState->pop();
   AbsValue* value1 = absState->pop();

   bool hasConstraint = value1->hasConstraint() && value2->hasConstraint();

   if (hasConstraint)
      {
      if (value1->getConstraint()->asLongConstraint() && value2->getConstraint()->asLongConstraint())
         {
         if (value1->getConstraint()->mustBeEqual(value2->getConstraint(), vp()))
            {
            AbsValue* result = AbsValue::createIntConst(0, region(), vp());
            absState->push(result);
            return absState;
            }

         if (value2->getConstraint()->mustBeLessThan(value1->getConstraint(), vp()))
            {
            AbsValue* result = AbsValue::createIntConst(1, region(), vp());
            absState->push(result);
            return absState;
            }

         if (value1->getConstraint()->mustBeLessThan(value2->getConstraint(),vp()))
            {
            AbsValue* result = AbsValue::createIntConst(-1, region(), vp());
            absState->push(result);
            return absState;
            }
         }
      }
   
   AbsValue *result =  AbsValue::createIntRange(-1,1, region(), vp());
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::pop(AbsState* absState)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::nop(AbsState* absState)
   {
   return absState;
   }
//-- Checked
AbsState* AbsInterpreter::pop2(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fload(AbsState* absState, int n)
   {
   AbsValue *value = absState->at(n);
   absState->push(value);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fload0(AbsState* absState)
   {
   fload(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fload1(AbsState* absState)
   {
   fload(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fload2(AbsState* absState)
   {
   fload(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fload3(AbsState* absState)
   {
   fload(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::swap(AbsState* absState)
   {
   AbsValue *value1 = absState->pop();
   AbsValue *value2 = absState->pop();
   absState->push(value1);
   absState->push(value2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fstore(AbsState* absState, int n)
   {
   absState->set(n, absState->pop());
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fstore0(AbsState* absState)
   {
   fstore(absState, 0);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fstore1(AbsState* absState)
   {
   fstore(absState, 1);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fstore2(AbsState* absState)
   {
   fstore(absState, 2);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fstore3(AbsState* absState)
   {
   fstore(absState, 3);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fmul(AbsState* absState)
   {
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dmul(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dneg(AbsState* absState)
   {
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dcmpl(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   absState->pop();
   absState->pop();
   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::dcmpg(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   absState->pop();
   absState->pop();
   AbsValue* result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fcmpg(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::fcmpl(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   AbsValue *result = AbsValue::createTopInt(region());
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::ddiv(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::fdiv(AbsState*absState)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::drem(AbsState*absState)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::fneg(AbsState*absState)
   {
   return absState;
   }

AbsState* AbsInterpreter::freturn(AbsState* absState)
   {
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::frem(AbsState*absState)
   {
   absState->pop();
   return absState;
   }


//-- Checked
AbsState* AbsInterpreter::sipush(AbsState* absState, int16_t value)
   {
   AbsValue *result = AbsValue::createIntConst(value, region(), vp());
   absState->push(result);
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::iinc(AbsState* absState, int index, int incVal)
   {
   AbsValue *value = absState->at(index);
   if (value->hasConstraint())
      {
      if (value->getConstraint()->asIntConst())
         {
         AbsValue* result = AbsValue::createIntConst(value->getConstraint()->asIntConst()->getInt() + incVal, region(), vp());
         absState->set(index, result);
         return absState;
         }
      
      if (value->getConstraint()->asIntRange())
         {
         AbsValue* result = AbsValue::createIntRange(value->getConstraint()->getLowInt() + incVal, value->getConstraint()->getHighInt() + incVal, region(), vp());
         absState->set(index, result);
         return absState;
         }
      }

   absState->set(index, AbsValue::createTopInt(region()));
   return absState;
   }

//-- Checked
AbsState* AbsInterpreter::putfield(AbsState* absState, int cpIndex)
   {
   if (absState->top()->isParameter() && !absState->top()->isImplicitParam())
      {
      _methodSummary->addNullCheck(absState->top()->getParamPosition());
      }

   absState->pop();
   
   uint32_t fieldOffset;
   TR::DataType type;
   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;

   _callerMethod->fieldAttributes(
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

   if (type == TR::Double || type == TR::Int64)
      {
      absState->pop();
      absState->pop();
      }
   else
      {
      absState->pop();
      }
   
   return absState;
   }  

//-- Checked
AbsState* AbsInterpreter::putstatic(AbsState* absState, int cpIndex)
   {
   void* staticAddress;

   TR::DataType type;

   bool isVolatile;
   bool isPrivate;
   bool isUnresolvedInVP;
   bool isFinal;
   _callerMethod->staticAttributes(
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

   if (type == TR::Double || type == TR::Int64)
      {
      absState->pop();
      absState->pop();
      }
   else
      {
      absState->pop();
      }
   
   return absState;
   }

void AbsInterpreter::ldcInt32(int cpIndex, AbsState* absState)
   {
   auto value = _callerMethod->intConstant(cpIndex);
   AbsValue *result = AbsValue::createIntConst(value, region(), vp());
   absState->push(result);
   }

void AbsInterpreter::ldcInt64(int cpIndex, AbsState* absState)
   {
   auto value = _callerMethod->longConstant(cpIndex);
   AbsValue *result1 = AbsValue::createLongConst(value, region(), vp());
   AbsValue *result2 = AbsValue::createDummyLong(region());
   absState->push(result1);
   absState->push(result2);
   }

void AbsInterpreter::ldcFloat(AbsState* absState)
   {
   AbsValue *result = AbsValue::createTopFloat(region());
   absState->push(result);
   }

void AbsInterpreter::ldcDouble(AbsState* absState)
   {
   AbsValue *result1 = AbsValue::createTopDouble(region());
   AbsValue *result2 = AbsValue::createDummyDouble(region());
   absState->push(result1);
   absState->push(result2);
   }

void AbsInterpreter::ldcAddress(int cpIndex, AbsState* absState) 
   {
   bool isString = _callerMethod->isStringConstant(cpIndex);
   if (isString) 
      {
      ldcString(cpIndex, absState); 
      return;
      }
   //TODO: non string case
   TR_OpaqueClassBlock* type = _callerMethod->getClassFromConstantPool(comp(), cpIndex);
   AbsValue* value = AbsValue::createClassObject(type, false, comp(), region(), vp());
   absState->push(value);
   }

void AbsInterpreter::ldcString(int cpIndex, AbsState* absState)
   {   
   TR::SymbolReference *symRef = comp()->getSymRefTab()->findOrCreateStringSymbol(_callerMethodSymbol, cpIndex);
   if (symRef->isUnresolved())
      {
      AbsValue* value = AbsValue::createTopObject(region());
      absState->push(value);
      return;
      }

   TR::VPConstraint *constraint = TR::VPConstString::create(vp(), symRef);
   AbsValue *result = AbsValue::createStringConst(symRef, region(), vp());
   absState->push(result);
   }

AbsState* AbsInterpreter::ldcw(AbsState*absState, int cpIndex)
   {
   ldc(absState, cpIndex);
   return absState;
   }

AbsState* AbsInterpreter::ldc(AbsState* absState, int cpIndex)
   {
   TR::DataType datatype = _callerMethod->getLDCType(cpIndex);
   switch(datatype) 
      {
      case TR::Int32: this->ldcInt32(cpIndex, absState); break;
      case TR::Int64: this->ldcInt64(cpIndex, absState); break;
      case TR::Float: this->ldcFloat(absState); break;
      case TR::Double: this->ldcDouble(absState); break;
      case TR::Address: this->ldcAddress(cpIndex, absState); break;
      default: 
         //printf("LDC Type %s\n", TR::DataType::getName(datatype));
         TR_ASSERT(false, "wrong type");
         break;
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
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::dreturn(AbsState* absState)
   {
   absState->pop();
   absState->pop();
   return absState;
   }

AbsState* AbsInterpreter::athrow(AbsState* absState)
   {
   return absState;
   }

AbsState* AbsInterpreter::anewarray(AbsState* absState, int cpIndex )
   {
   TR_OpaqueClassBlock* type = _callerMethod->getClassFromConstantPool(comp(), cpIndex);

   AbsValue *length = absState->pop();

   if (length->getConstraint() && length->getConstraint()->asIntConstraint())
      {
      AbsValue* value = AbsValue::createArrayObject(type, true, length->getConstraint()->asIntConstraint()->getLowInt(), length->getConstraint()->asIntConstraint()->getHighInt(),4, comp(), region(), vp());
      absState->push(value);
      return absState;
      }

   AbsValue* value = AbsValue::createArrayObject(type, true, 0, INT32_MAX ,4, comp(), region(), vp());
   absState->push(value);
   return absState;
   }

AbsState* AbsInterpreter::arraylength(AbsState* absState)
   {
   AbsValue* arrayRef = absState->pop();

   if (arrayRef->isParameter() && !arrayRef->isImplicitParam())
      _methodSummary->addNullCheck(arrayRef->getParamPosition());

   if (arrayRef->hasConstraint()&& arrayRef->getConstraint()->getArrayInfo())
      {
      TR::VPArrayInfo* info = arrayRef->getConstraint()->getArrayInfo();
      AbsValue* result;
      if (info->lowBound() == info->highBound())
         {
         result = AbsValue::createIntConst(info->lowBound(), region(), vp());
         }
      else
         {
         result = AbsValue::createIntRange(info->lowBound(), info->highBound(), region(), vp());
         }
      absState->push(result);
      return absState;
      }
   
   AbsValue *result = AbsValue::createIntRange(0, INT32_MAX, region(), vp());
   absState->push(result);
   return absState;
   }

AbsState* AbsInterpreter::_new(AbsState* absState, int cpIndex)
   {
   TR_OpaqueClassBlock* type = _callerMethod->getClassFromConstantPool(comp(), cpIndex);
   AbsValue* value = AbsValue::createClassObject(type, true, comp(), region(), vp());
   absState->push(value);
   return absState;
   }

AbsState* AbsInterpreter::newarray(AbsState* absState, int atype)
   {
   /**
    * atype
    * 4: boolean
    * 5: char
    * 6: float
    * 7: double
    * 8: byte
    * 9: short
    * 10: int
    * 11: long
    */
   int32_t elementSize = atype == 7 || atype == 11 ? 8 : 4;
   
   TR_OpaqueClassBlock* arrayType = comp()->fe()->getClassFromNewArrayType(atype);

   AbsValue *length = absState->pop();

   if (length->getConstraint() && length->getConstraint()->asIntConstraint())
      {
      AbsValue* value = AbsValue::createArrayObject(arrayType, true, length->getConstraint()->getLowInt(), length->getConstraint()->getHighInt(), elementSize, comp(), region(), vp());
      absState->push(value);
      return absState;
      }

   AbsValue* value = AbsValue::createArrayObject(arrayType, true, 0, INT32_MAX, elementSize, comp(), region(), vp());
   absState->push(value);
   return absState;
   }

AbsState* AbsInterpreter::invokevirtual(AbsState* absState, int bcIndex, int cpIndex, TR::Block* block)
   {
   AbsValue* absValue = absState->top();

   if (absValue->isParameter() && !absValue->isImplicitParam())
      {
      _methodSummary->addNullCheck(absValue->getParamPosition());
      }

   invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Virtual, absState, block);
   return absState;
   }

AbsState* AbsInterpreter::invokestatic(AbsState* absState, int bcIndex, int cpIndex,TR::Block* block)
   {
   invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Static, absState, block);
   return absState;
   }

AbsState* AbsInterpreter::invokespecial(AbsState* absState, int bcIndex, int cpIndex,  TR::Block* block)
   {
   AbsValue* absValue = absState->top();
   if (absValue->isParameter() && !absValue->isImplicitParam())
      {
      _methodSummary->addNullCheck(absValue->getParamPosition());
      }

   invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Special, absState, block);
   return absState;
   }

AbsState* AbsInterpreter::invokedynamic(AbsState* absState, int bcIndex, int cpIndex, TR::Block* block)
   {
   return NULL;
   }

AbsState* AbsInterpreter::invokeinterface(AbsState* absState, int bcIndex, int cpIndex, TR::Block* block)
   {
   AbsValue* absValue = absState->top();
   if (absValue->isParameter() && !absValue->isImplicitParam())
      {
      _methodSummary->addNullCheck(absValue->getParamPosition());
      }
   invoke(bcIndex, cpIndex, TR::MethodSymbol::Kinds::Interface, absState, block);
   return absState;
   }

void AbsInterpreter::invoke(int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind,  AbsState* absState, TR::Block* block) 
   {
   TR::Method *calleeMethod = comp()->fej9()->createMethod(comp()->trMemory(), _callerMethod->containingClass(), cpIndex);
   
   TR_CallSite* callsite = findCallSiteTargets(_callerMethod, bcIndex, cpIndex, kind, _callerIndex, _callStack, block); // callsite can be NULL, such case will be handled by addChild().

   uint32_t numExplicitParams = calleeMethod->numberOfExplicitParameters();
   uint32_t numImplicitParams = kind == TR::MethodSymbol::Kinds::Static ? 0 : 1; 

   AbsParameterArray* paramArray = new (region()) AbsParameterArray(region());

   for (uint32_t i = 0 ; i < numExplicitParams; i ++)
      {
      AbsValue* absValue = NULL;

      TR::DataType dataType = calleeMethod->parmType(numExplicitParams -i - 1);
      if (dataType == TR::Double || dataType == TR::Int64)
         {
         absState->pop();
         absValue = absState->pop();
         }
      else
         {
         absValue = absState->pop();
         }

      paramArray->push_front(absValue);
      }
   
   if (numImplicitParams == 1)
      paramArray->push_front(absState->pop());

   _idtBuilder->addChild(_idtNode, _callerIndex, callsite, paramArray, _callStack ,block);
   
   //For the return values
   if (calleeMethod->returnTypeWidth() == 0)
      return;

   //how many pushes?
   TR::DataType datatype = calleeMethod->returnType();
   switch(datatype) 
         {
         case TR::Int32:
         case TR::Int16:
         case TR::Int8:
            absState->push(AbsValue::createTopInt(region()));
            break;
         case TR::Float:
            absState->push(AbsValue::createTopFloat(region()));
            break;
         case TR::Address:
            absState->push(AbsValue::createTopObject(region()));
            break;
         case TR::Double:
            absState->push(AbsValue::createTopDouble(region()));
            absState->push(AbsValue::createDummyDouble(region()));
            break;
         case TR::Int64:
            absState->push(AbsValue::createTopLong(region()));
            absState->push(AbsValue::createDummyLong(region()));
            break;
            
         default:
            //printf("%s\n",TR::DataType::getName(datatype));
            break;
         }  
   }



   
TR_CallSite* AbsInterpreter::findCallSiteTargets(
      TR_ResolvedMethod *caller, 
      int bcIndex,
      int cpIndex,
      TR::MethodSymbol::Kinds kind, 
      int callerIndex, 
      TR_CallStack* callStack,
      TR::Block* block
      )
   {

   TR::ResolvedMethodSymbol* callerSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), caller, comp());
   
   TR_ByteCodeInfo *infoMem = new (region()) TR_ByteCodeInfo();
   TR_ByteCodeInfo &info = *infoMem;

   TR::SymbolReference *symRef = getSymbolReference(callerSymbol, cpIndex, kind);

   TR::Symbol *sym = symRef->getSymbol();
   bool isInterface = kind == TR::MethodSymbol::Kinds::Interface;

   if (symRef->isUnresolved() && !isInterface) 
      {
      if (comp()->getOption(TR_TraceBIIDTGen))
         {
         traceMsg(comp(), "not considering: method is unresolved and is not interface\n");
         }
      return NULL;
      }

   TR::ResolvedMethodSymbol *calleeSymbol = !isInterface ? sym->castToResolvedMethodSymbol() : NULL;
   TR_ResolvedMethod *callee = !isInterface ? calleeSymbol->getResolvedMethod() : NULL;

   TR::Method *calleeMethod = !isInterface ? calleeSymbol->getMethod() : comp()->fej9()->createMethod(comp()->trMemory(), caller->containingClass(), cpIndex);
   info.setByteCodeIndex(bcIndex);
   info.setDoNotProfile(false);
   
   info.setCallerIndex(callerIndex);
   TR_OpaqueClassBlock *callerClass = caller->classOfMethod();
   TR_OpaqueClassBlock *calleeClass = callee ? callee->classOfMethod() : NULL;
   info.setIsSameReceiver(callerClass == calleeClass);
   
   bool isIndirect = false;
   int32_t vftSlot = kind == TR::MethodSymbol::Virtual ? symRef->getOffset() : -1;

   TR_CallSite *callsite = getCallSite
      (
         kind,
         caller,
         NULL,
         NULL,
         NULL,
         calleeMethod,
         calleeClass,
         vftSlot,
         cpIndex,
         callee,
         calleeSymbol,
         isIndirect,
         isInterface,
         info,
         -1,
         false,
         symRef
      );

   callStack->_methodSymbol = callStack->_methodSymbol ? callStack->_methodSymbol : callerSymbol;
   
   if (!callsite)
      return NULL;

   callsite->_byteCodeIndex = bcIndex;
   callsite->_bcInfo = info;
   callsite->_cpIndex= cpIndex;

   return callsite;   
   }

TR_CallSite* AbsInterpreter::getCallSite(TR::MethodSymbol::Kinds kind,
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
                                       TR_ByteCodeInfo & bcInfo,
                                       int32_t depth,
                                       bool allConsts,
                                       TR::SymbolReference *symRef)
   {
   TR::ResolvedMethodSymbol *rms = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), callerResolvedMethod, comp());
   auto owningMethod = (TR_ResolvedJ9Method*) callerResolvedMethod;
   bool unresolvedInCP;
   if (kind == TR::MethodSymbol::Kinds::Virtual)
      {
      TR_ResolvedMethod *method = owningMethod->getResolvedPossiblyPrivateVirtualMethod(
         comp(),
         cpIndex,
         /* ignoreRtResolve = */ false,
         &unresolvedInCP);

      bool opposite = !(method != NULL && method->isPrivate());
      TR::SymbolReference * somesymref = NULL;
      if (!opposite) 
         {
         somesymref = comp()->getSymRefTab()->findOrCreateMethodSymbol(
         rms->getResolvedMethodIndex(),
         cpIndex,
         method,
         TR::MethodSymbol::Special,
         /* isUnresolvedInCP = */ false);
         }
      else 
         {
         somesymref = comp()->getSymRefTab()->findOrCreateVirtualMethodSymbol(rms, cpIndex);
         if (!somesymref->isUnresolved())
            {
            method = somesymref->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
            }
         }

      if (method) 
         {
         kind = somesymref->getSymbol()->isFinal() ||
         method->isPrivate() ||
         (debug("omitVirtualGuard") && !method->virtualMethodIsOverridden()) ? TR::MethodSymbol::Kinds::Static : kind;
         }
      }

   
   if (initialCalleeMethod)
      {
      kind = symRef->getSymbol()->isFinal() ||
      initialCalleeMethod->isPrivate() ||
      (debug("omitVirtualGuard") && !initialCalleeMethod->virtualMethodIsOverridden()) ? TR::MethodSymbol::Kinds::Static : kind;
      }

   TR_CallSite *callsite = NULL;
   switch (kind) 
      {
      case TR::MethodSymbol::Kinds::Virtual:
         callsite = new (region()) TR_J9VirtualCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp(), depth, allConsts);
         break;
      case TR::MethodSymbol::Kinds::Static:
      case TR::MethodSymbol::Kinds::Special:
         callsite = new (region()) TR_DirectCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp(), depth, allConsts);
         break;
      case TR::MethodSymbol::Kinds::Interface:
         callsite = new (region()) TR_J9InterfaceCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp(), depth, allConsts);
         break;
      }
   return callsite;
   }

TR::SymbolReference* AbsInterpreter::getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind)
   {
   TR::SymbolReference *symRef = NULL;
   switch(kind)
      {
      case (TR::MethodSymbol::Kinds::Virtual):
         symRef = comp()->getSymRefTab()->findOrCreateVirtualMethodSymbol(callerSymbol, cpIndex);
         break;
      case (TR::MethodSymbol::Kinds::Static):
         symRef = comp()->getSymRefTab()->findOrCreateStaticMethodSymbol(callerSymbol, cpIndex);
         break;
      case (TR::MethodSymbol::Kinds::Interface):
         symRef = comp()->getSymRefTab()->findOrCreateInterfaceMethodSymbol(callerSymbol, cpIndex);
         break;
      case (TR::MethodSymbol::Kinds::Special):
         symRef = comp()->getSymRefTab()->findOrCreateSpecialMethodSymbol(callerSymbol, cpIndex);
         break;
      }
   return symRef;
   }