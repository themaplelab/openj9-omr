#include "compiler/optimizer/IDTConstructor.hpp"

#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/il/OMRBlock.hpp"
#include "il/Block.hpp"
#include "il/OMRBlock_inlines.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/infra/ILWalk.hpp"
#include "compiler/ilgen/J9ByteCodeIterator.hpp"
#include "compiler/optimizer/MethodSummary.hpp"
#include "optimizer/AbsState.hpp"


void AbsFrameIDTConstructor::interpret()
{
  TR::CFG* cfg = this->getCFG();
  _target->_calleeSymbol->setFlowGraph(cfg);
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
           TR_ResolvedMethod *callerResolvedMethod = this->_bci.method();
           TR::ResolvedMethodSymbol* callerResolvedMethodSymbol = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), callerResolvedMethod, TR::comp());
           block->_absEnv = IDTConstructor::enterMethod(this->getRegion(), this->_node, this, callerResolvedMethodSymbol);
           first = false;
           block->_absEnv->_block = block;
        }
        block->setVisitCount(0);
        queue.push_back(block);
     }

  while (!queue.empty()) {
    OMR::Block *block = queue.front();
    queue.pop_front();
    AbsFrame::interpret(block);
  }
}

IDTConstructor::IDTConstructor(TR::Region &region, IDTNode *node, AbsFrame *absFrame)
  : AbsEnvStatic(region, node, absFrame)
{
}

IDTConstructor::IDTConstructor(IDTConstructor &other)
  : AbsEnvStatic(other)
{
}

IDTConstructor::IDTConstructor(AbsEnvStatic &other)
  : AbsEnvStatic(other)
{
}

int
IDTConstructor::getCallerIndex() const
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->getCallerIndex();
}

int
AbsFrameIDTConstructor::getCallerIndex() const
{
  return this->_callerIndex;
}

AbsState&
IDTConstructor::invokevirtual(AbsState& absState, int bcIndex, int cpIndex)
{
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "invoke virtual\n");
     }
  AbsEnvStatic::invokevirtual(absState, bcIndex, cpIndex);
  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  TR::ResolvedMethodSymbol *rms = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), method, TR::comp());
  TR_CallSite *callsite = this->findCallSiteTargets(rms, bcIndex, cpIndex, TR::MethodSymbol::Kinds::Virtual, this->getBlock(), this->getFrame()->getCFG());
  this->inliner()->obtainIDT(this->getDeque(), this->getNode(), callsite, this->inliner()->budget(), cpIndex);
  return absState;
}

AbsState&
IDTConstructor::instanceof(AbsState& absState, int cpIndex, int bytecodeIndex)
{
  if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
     {
     traceMsg(TR::comp(), "ZZZ: instanceof \n");
     }

  AbsValue *absValueOriginal = absState.top();
  if (absValueOriginal->getParamPosition() >= 0) {

  AbsEnvStatic::instanceof(absState, cpIndex, bytecodeIndex);
  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  TR_OpaqueClassBlock *block = method->getClassFromConstantPool(TR::comp(), cpIndex);

  // we don't know what we are testing for so just return...
  if (!block) { return absState; }

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->getVP(), block);
  TR::VPNonNullObject *nonnull = TR::VPNonNullObject::create(this->getFrame()->getValuePropagation());
  TR::VPNullObject *null = TR::VPNullObject::create(this->getFrame()->getValuePropagation());
  TR::VPConstraint *typeConstraint= TR::VPClass::create(this->getVP(), fixedClass, nonnull, NULL, NULL, NULL);
  AbsValue *absValue = new (getRegion()) AbsValue(typeConstraint, TR::Address);
  AbsValue *absValue2 = new (getRegion()) AbsValue(null, TR::Address);
  this->addInstanceof(bytecodeIndex, absValueOriginal->getParamPosition(), absValue);
  this->addIfNonNull(bytecodeIndex, absValueOriginal->getParamPosition(), absValue2);
  
  }
  return absState;
}

AbsState&
IDTConstructor::ifeq(AbsState& absState, int branchOffset, int bytecodeIndex)
{
  AbsValue *absValue = absState.top();
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "ifeq: absValue->getParamPosition() %d\n", absValue->getParamPosition());
     }
  // what is the argument position?
  if (absValue->getParamPosition() < 0) goto normal;
  this->addIfeq(bytecodeIndex, absValue->getParamPosition());
normal:
  AbsEnvStatic::ifeq(absState, branchOffset, bytecodeIndex);
  return absState;
}

AbsState&
IDTConstructor::ifne(AbsState& absState, int branchOffset, int bytecodeIndex)
{
  AbsValue *absValue = absState.top();
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "ifeq: absValue->getParamPosition() %d\n", absValue->getParamPosition());
     }
  // what is the argument position?
  if (absValue->getParamPosition() < 0) goto normal;
  this->addIfne(bytecodeIndex, absValue->getParamPosition());
normal:
  AbsEnvStatic::ifne(absState, branchOffset, bytecodeIndex);
  return absState;
}

AbsState&
IDTConstructor::ifge(AbsState& absState, int branchOffset, int bytecodeIndex)
{
  AbsValue *absValue = absState.top();
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "ifeq: absValue->getParamPosition() %d\n", absValue->getParamPosition());
     }
  // what is the argument position?
  if (absValue->getParamPosition() < 0) goto normal;
  this->addIfge(bytecodeIndex, absValue->getParamPosition());
normal:
  AbsEnvStatic::ifge(absState, branchOffset, bytecodeIndex);
  return absState;
}

AbsState&
IDTConstructor::ifgt(AbsState& absState, int branchOffset, int bytecodeIndex)
{
  AbsValue *absValue = absState.top();
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "ifeq: absValue->getParamPosition() %d\n", absValue->getParamPosition());
     }
  // what is the argument position?
  if (absValue->getParamPosition() < 0) goto normal;
  this->addIfgt(bytecodeIndex, absValue->getParamPosition());
normal:
  AbsEnvStatic::ifgt(absState, branchOffset, bytecodeIndex);
  return absState;

}
AbsState&
IDTConstructor::ifle(AbsState& absState, int branchOffset, int bytecodeIndex)
{
  AbsValue *absValue = absState.top();
  // what is the argument position?
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "ifeq: absValue->getParamPosition() %d\n", absValue->getParamPosition());
     }
  if (absValue->getParamPosition() < 0) goto normal;
  this->addIfle(bytecodeIndex, absValue->getParamPosition());
normal:
  AbsEnvStatic::ifle(absState, branchOffset, bytecodeIndex);
  return absState;
}

AbsState&
IDTConstructor::iflt(AbsState& absState, int branchOffset, int bytecodeIndex)
{
  AbsValue *absValue = absState.top();
  // what is the argument position?
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "ifeq: absValue->getParamPosition() %d\n", absValue->getParamPosition());
     }
  if (absValue->getParamPosition() < 0) goto normal;
  this->addIflt(bytecodeIndex, absValue->getParamPosition());
normal:
  AbsEnvStatic::iflt(absState, branchOffset, bytecodeIndex);
  return absState;
}

AbsState&
IDTConstructor::ifnonnull(AbsState& absState, int branchOffset, int bytecodeIndex)
{
  AbsValue *absValue = absState.top();
  // what is the argument position?
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "ifeq: absValue->getParamPosition() %d\n", absValue->getParamPosition());
     }
  TR::VPNonNullObject *nonnull = NULL;
  TR::VPNullObject *null = NULL;
  AbsValue *absValue2 = NULL;
  AbsValue *absValue3 = NULL;
  if (absValue->getParamPosition() < 0) goto normal;

  nonnull = TR::VPNonNullObject::create(this->getFrame()->getValuePropagation());
  null = TR::VPNullObject::create(this->getFrame()->getValuePropagation());
  absValue2 = new (getRegion()) AbsValue(nonnull, TR::Address);
  absValue3 = new (getRegion()) AbsValue(null, TR::Address);
  this->addIfNonNull(bytecodeIndex, absValue->getParamPosition(), absValue2);
  this->addIfNonNull(bytecodeIndex, absValue->getParamPosition(), absValue3);

normal:
  AbsEnvStatic::ifnonnull(absState, branchOffset, bytecodeIndex);
  return absState;
}

AbsState&
IDTConstructor::ifnull(AbsState& absState, int branchOffset, int bytecodeIndex)
{
  AbsValue *absValue = absState.top();
  // what is the argument position?
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "ifeq: absValue->getParamPosition() %d\n", absValue->getParamPosition());
     }
  TR::VPNonNullObject *nonnull = NULL;
  TR::VPNullObject *null = NULL;
  AbsValue *absValue2 = NULL;
  AbsValue *absValue3 = NULL;
  if (absValue->getParamPosition() < 0) goto normal;
  nonnull = TR::VPNonNullObject::create(this->getFrame()->getValuePropagation());
  null = TR::VPNullObject::create(this->getFrame()->getValuePropagation());
  absValue2 = new (getRegion()) AbsValue(nonnull, TR::Address);
  absValue3 = new (getRegion()) AbsValue(null, TR::Address);
  this->addIfNonNull(bytecodeIndex, absValue->getParamPosition(), absValue2);
  this->addIfNonNull(bytecodeIndex, absValue->getParamPosition(), absValue3);
normal:
  AbsEnvStatic::ifnull(absState, branchOffset, bytecodeIndex);
  return absState;
}

void
IDTConstructor::addIfNull(int bcIndex, int argPos, AbsValue *absValue)
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->_summary->addNullCheckFolding(bcIndex, absValue, argPos);
}

void
IDTConstructor::addIfNonNull(int bcIndex, int argPos, AbsValue *absValue)
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->_summary->addNullCheckFolding(bcIndex, absValue, argPos);
}

void
IDTConstructor::addInstanceof(int bcIndex, int argPos, AbsValue *absValue)
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->_summary->addInstanceOfFolding(bcIndex, absValue, argPos);
}

void
IDTConstructor::addIfeq(int bcIndex, int argPos)
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->_summary->addIfeq(bcIndex, argPos);
}

void
IDTConstructor::addIfne(int bcIndex, int argPos)
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->_summary->addIfne(bcIndex, argPos);
}

void
IDTConstructor::addIfgt(int bcIndex, int argPos)
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->_summary->addIfgt(bcIndex, argPos);
}

void
IDTConstructor::addIfge(int bcIndex, int argPos)
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->_summary->addIfge(bcIndex, argPos);
}

void
IDTConstructor::addIflt(int bcIndex, int argPos)
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->_summary->addIflt(bcIndex, argPos);
}

void
IDTConstructor::addIfle(int bcIndex, int argPos)
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->_summary->addIfle(bcIndex, argPos);
}

AbsState&
IDTConstructor::invokespecial(AbsState& absState, int bcIndex, int cpIndex)
{
  traceMsg(TR::comp(), "invoke special %s\n", this->getResolvedMethodSymbol()->signature(TR::comp()->trMemory()));
  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  TR::ResolvedMethodSymbol *rms = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), method, TR::comp());
  traceMsg(TR::comp(), "invoke special %s\n", rms->signature(TR::comp()->trMemory()));
  AbsEnvStatic::invokespecial(absState, bcIndex, cpIndex);
  TR_CallSite *callsite = this->findCallSiteTargets(rms, bcIndex, cpIndex, TR::MethodSymbol::Kinds::Special, this->getBlock(), this->getFrame()->getCFG());
  this->inliner()->obtainIDT(this->getDeque(), this->getNode(), callsite, this->inliner()->budget(), cpIndex);
  return absState;
}

AbsState&
IDTConstructor::invokestatic(AbsState& absState, int bcIndex, int cpIndex)
{
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "invoke static\n");
     }
  AbsEnvStatic::invokestatic(absState, bcIndex, cpIndex);
  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  TR::ResolvedMethodSymbol *rms = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), method, TR::comp());
  TR_CallSite* callsite = this->findCallSiteTargets(rms, bcIndex, cpIndex, TR::MethodSymbol::Kinds::Static, this->getBlock(), this->getFrame()->getCFG());
  this->inliner()->obtainIDT(this->getDeque(), this->getNode(), callsite, this->inliner()->budget(), cpIndex);
  return absState;
}

IDTNodeIndices *
IDTConstructor::getDeque()
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->getDeque();
}

AbsState&
IDTConstructor::invokeinterface(AbsState& absState, int bcIndex, int cpIndex)
{
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
     {
     traceMsg(TR::comp(), "invoke interface\n");
     }
  AbsEnvStatic::invokeinterface(absState, bcIndex, cpIndex);
  TR_ResolvedMethod *method = this->getFrame()->_bci.method();
  TR::ResolvedMethodSymbol *rms = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), method, TR::comp());
  
  TR_CallSite* callsite = this->findCallSiteTargets(rms, bcIndex, cpIndex, TR::MethodSymbol::Kinds::Interface, this->getBlock(), this->getFrame()->getCFG());
  this->inliner()->obtainIDT(this->getDeque(), this->getNode(), callsite, this->inliner()->budget(), cpIndex);
  return absState;
}

TR_CallSite*
IDTConstructor::findCallSiteTargets(TR::ResolvedMethodSymbol *callerSymbol, int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind, OMR::Block *block, TR::CFG * callerCFG)
{
      TR_ByteCodeInfo *infoMem = new (getRegion()) TR_ByteCodeInfo();
      TR_ByteCodeInfo &info = *infoMem;
      TR_ResolvedMethod *caller = callerSymbol->getResolvedMethod();
      uint32_t methodSize = TR::Compiler->mtd.bytecodeSize(caller->getPersistentIdentifier());
      TR::SymbolReference *symRef = this->getSymbolReference(callerSymbol, cpIndex, kind);
      TR::Symbol *sym = symRef->getSymbol();
      bool isInterface = kind == TR::MethodSymbol::Kinds::Interface;
      if (symRef->isUnresolved() && !isInterface) 
         {
         //TODO: remove TR::comp()
         if (TR::comp()->getOption(TR_TraceBIIDTGen))
            {
            traceMsg(TR::comp(), "not considering: method is unresolved and is not interface\n");
            }
         return NULL;
         }

      TR::ResolvedMethodSymbol *calleeSymbol = !isInterface ? sym->castToResolvedMethodSymbol() : NULL;
      TR_ResolvedMethod *callee = !isInterface ? calleeSymbol->getResolvedMethod() : NULL;
      //TODO: fixme TR::comp
      TR::Method *calleeMethod = !isInterface ? calleeSymbol->getMethod() : TR::comp()->fej9()->createMethod(TR::comp()->trMemory(), caller->containingClass(), cpIndex);

      info.setByteCodeIndex(bcIndex);
      info.setDoNotProfile(false);
      info.setCallerIndex(this->getCallerIndex());
      TR_OpaqueClassBlock *callerClass = caller->classOfMethod();
      TR_OpaqueClassBlock *calleeClass = callee ? callee->classOfMethod() : NULL;
      info.setIsSameReceiver(callerClass == calleeClass);
      
      bool isIndirect = kind == TR::MethodSymbol::Kinds::Static || TR::MethodSymbol::Kinds::Special;
      int32_t offset = kind == TR::MethodSymbol::Virtual ? symRef->getOffset() : -1;
      TR_CallSite *callsite = this->getCallSite
         (
            kind,
            caller,
            NULL,
            NULL,
            NULL,
            calleeMethod,
            calleeClass,
            offset,
            cpIndex,
            callee,
            calleeSymbol,
            isIndirect,
            isInterface,
            info,
            TR::comp(),
            -1,
            false,
            symRef
         );
      //TODO: Sometimes these were not set, why?
      callsite->_byteCodeIndex = bcIndex;
      callsite->_bcInfo = info; //info has to be a reference, so it is being deleted after node exits.
      callsite->_cpIndex= cpIndex;
      callsite->findCallSiteTarget(this->getCallStack(), this->inliner());
      //TODO: Sometimes these were not set, why?
      this->getCallStack()->_methodSymbol = this->getCallStack()->_methodSymbol ? this->getCallStack()->_methodSymbol : callerSymbol;
      
      this->inliner()->applyPolicyToTargets(this->getCallStack(), callsite, block->self(), callerCFG);
      return callsite;
}


AbsEnvStatic* IDTConstructor::enterMethod(TR::Region& region, IDTNode* node, AbsFrame* absFrame, TR::ResolvedMethodSymbol* rms)
{
  AbsEnvStatic *absEnv = new (region) IDTConstructor(region, node, absFrame);
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
     switch (dataType) {
        case TR::Int8:
          {
          AbsValue *absvalue = new (region) AbsValue(NULL, TR::Int32);
          absvalue->setParamPosition(i);
          absEnv->getState().set(i, absvalue);
          }
          continue;
        break;

        case TR::Int16:
          {
          AbsValue *absvalue = new (region) AbsValue(NULL, TR::Int32);
          absvalue->setParamPosition(i);
          absEnv->getState().set(i, absvalue);
          }
          continue;
        break;

        case TR::Int32:
          {
          AbsValue *absvalue = new (region) AbsValue(NULL, TR::Int32);
          absvalue->setParamPosition(i);
          absEnv->getState().set(i, absvalue);
          }
          continue;
        break;

        case TR::Int64:
          {
          AbsValue *absvalue = new (region) AbsValue(NULL, TR::Int64);
          absvalue->setParamPosition(i);
          absEnv->getState().set(i, absvalue);
          absvalue = new (region) AbsValue(NULL, TR::NoType);
          absvalue->setParamPosition(i);
          i = i+1;
          absEnv->getState().set(i, absvalue);
          }
          continue;
        break;

        case TR::Float:
          {
          AbsValue *absvalue = new (region) AbsValue(NULL, TR::Float);
          absvalue->setParamPosition(i);
          absEnv->getState().set(i, absvalue);
          }
          continue;
        break;

        case TR::Double:
          {
          AbsValue *absvalue = new (region) AbsValue(NULL, TR::Double);
          absvalue->setParamPosition(i);
          absEnv->getState().set(i, absvalue);
          absvalue = new (region) AbsValue(NULL, TR::NoType);
          absvalue->setParamPosition(i);
          i = i+1;
          absEnv->getState().set(i, absvalue);
          }
        continue;
        break;

        default:
        //TODO: what about vectors and aggregates?
        break;
     }
     const bool isClass = parameter->isClass();
     if (!isClass)
       {
       AbsValue *absvalue = new (region) AbsValue(NULL, TR::Address);
       absvalue->setParamPosition(i);
       absEnv->getState().set(i, absvalue);
       continue;
       }

     TR_OpaqueClassBlock *parameterClass = parameter->getOpaqueClass();
     if (!parameterClass)
       {
       AbsValue *absvalue = new (region) AbsValue(NULL, TR::Address);
       absvalue->setParamPosition(i);
       absEnv->getState().set(i, absvalue);
       continue;
       }

      AbsValue* value = AbsEnvStatic::getClassConstraint(parameterClass, absFrame->getValuePropagation(), region);
      value->setParamPosition(i);
      absEnv->getState().set(i, value);
     }
  return absEnv;
}

void
AbsFrameIDTConstructor::factFlow(OMR::Block *block) {
  if (block->getNumber() == 3) return;

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
    AbsEnvStatic *absEnv = new (this->getRegion()) IDTConstructor(*block->getPredecessors().front()->getFrom()->asBlock()->_absEnv);
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

  // what happens if nothing has been interpreted? For this abstract interpretation it doesn't really matter.
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

     traceMsg(TR::comp(), "am i entering here in  loop?\n");
     AbsEnvStatic *oldAbsEnv = aBlock->_absEnv;
     // how many 
     AbsEnvStatic *absEnv = new (this->getRegion()) IDTConstructor(*oldAbsEnv);
     traceMsg(TR::comp(), "exiting IDTConstructor\n");
     aBlock->_absEnv->zeroOut(absEnv);
     block->_absEnv = absEnv;
     block->_absEnv->_block = block;
     return;
     }
   traceMsg(TR::comp(), "predecessors found nothing\n");
}

AbsEnvStatic* AbsFrameIDTConstructor::mergeAllPredecessors(OMR::Block *block) {
  TR::CFGEdgeList &predecessors = block->getPredecessors();
  IDTConstructor *absEnv = nullptr;
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
        // copy the first one
        if (TR::comp()->trace(OMR::benefitInliner))
           {
           traceMsg(TR::comp(), "copy basic block %d\n", aBlock->getNumber());
           }
        absEnv = new (this->getRegion()) IDTConstructor(*aBlock->_absEnv);
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

TR::SymbolReference*
IDTConstructor::getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind)
   {
      TR::SymbolReference *symRef = NULL;
      switch(kind) {
         case (TR::MethodSymbol::Kinds::Virtual):
            symRef = TR::comp()->getSymRefTab()->findOrCreateVirtualMethodSymbol(callerSymbol, cpIndex);
         break;
         case (TR::MethodSymbol::Kinds::Static):
            symRef = TR::comp()->getSymRefTab()->findOrCreateStaticMethodSymbol(callerSymbol, cpIndex);
         break;
         case (TR::MethodSymbol::Kinds::Interface):
            symRef = TR::comp()->getSymRefTab()->findOrCreateInterfaceMethodSymbol(callerSymbol, cpIndex);
         break;
         case (TR::MethodSymbol::Kinds::Special):
            symRef = TR::comp()->getSymRefTab()->findOrCreateSpecialMethodSymbol(callerSymbol, cpIndex);
         break;
      }
      return symRef;
   }

TR_CallSite *
IDTConstructor::getCallSite(TR::MethodSymbol::Kinds kind,
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
                                    TR::Compilation *comp,
                                    int32_t depth,
                                    bool allConsts,
                                    TR::SymbolReference *symRef)
   {
      if (false) {
          return TR_CallSite::create(callNodeTreeTop, parent, callNode, receiverClass, symRef, initialCalleeMethod, comp, comp->trMemory(), heapAlloc, callerResolvedMethod, depth, allConsts);
      }

      TR::ResolvedMethodSymbol *rms = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), callerResolvedMethod, TR::comp());
      auto owningMethod = (TR_ResolvedJ9Method*) callerResolvedMethod;
      bool unresolvedInCP;
      if (kind == TR::MethodSymbol::Kinds::Virtual) {
      TR_ResolvedMethod *method = owningMethod->getResolvedPossiblyPrivateVirtualMethod(
         comp,
         cpIndex,
         /* ignoreRtResolve = */ false,
         &unresolvedInCP);

      bool opposite = !(method != NULL && method->isPrivate());
      TR::SymbolReference * somesymref = NULL;
      if (!opposite) {
         somesymref = comp->getSymRefTab()->findOrCreateMethodSymbol(
            rms->getResolvedMethodIndex(),
            cpIndex,
            method,
            TR::MethodSymbol::Special,
           /* isUnresolvedInCP = */ false);
      }
      else {
         somesymref = comp->getSymRefTab()->findOrCreateVirtualMethodSymbol(rms, cpIndex);
         if (!somesymref->isUnresolved()) {
            method = somesymref->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
         }
      }
      if (method) {
        kind = somesymref->getSymbol()->isFinal() ||
         method->isPrivate() ||
         (debug("omitVirtualGuard") && !method->virtualMethodIsOverridden()) ? TR::MethodSymbol::Kinds::Static : kind;
      }}

      
      if (initialCalleeMethod)
      {

        kind = symRef->getSymbol()->isFinal() ||
         initialCalleeMethod->isPrivate() ||
         (debug("omitVirtualGuard") && !initialCalleeMethod->virtualMethodIsOverridden()) ? TR::MethodSymbol::Kinds::Static : kind;
      }

      TR_CallSite *callsite = NULL;
      switch (kind) {
         case TR::MethodSymbol::Kinds::Virtual:
            callsite = new (this->getCallSitesRegion()) TR_J9VirtualCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp, depth, allConsts);
         break;
         case TR::MethodSymbol::Kinds::Static:
         case TR::MethodSymbol::Kinds::Special:
            callsite = new (this->getCallSitesRegion()) TR_DirectCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp, depth, allConsts);
         break;
         case TR::MethodSymbol::Kinds::Interface:
            callsite = new (this->getCallSitesRegion()) TR_J9InterfaceCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp, depth, allConsts);
         break;
      }
      return callsite;
   }

TR::Region&
AbsFrameIDTConstructor::getCallSitesRegion()
{
  return this->_callSitesRegion;
}

TR::Region&
IDTConstructor::getCallSitesRegion()
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->getCallSitesRegion();
}

AbsFrameIDTConstructor::AbsFrameIDTConstructor(TR::Region &region, IDTNode *node, int callerIndex, TR_CallStack *callStack, OMR::BenefitInliner *inliner)
  : AbsFrame(region, node)
  , _callerIndex(callerIndex)
  , _callSitesRegion(region)
  , _inliningCallStack(callStack)
  , _inliner(inliner)
  , _summary(nullptr)
{
  _summary = new (region) MethodSummaryExtension(region, this->getValuePropagation());
}

TR_CallStack*
IDTConstructor::getCallStack()
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->getCallStack();
}

TR_CallStack*
AbsFrameIDTConstructor::getCallStack()
{
  return this->_inliningCallStack;
}

OMR::BenefitInliner*
AbsFrameIDTConstructor::inliner()
{
  return this->_inliner;
}

OMR::BenefitInliner*
IDTConstructor::inliner()
{
  return static_cast<AbsFrameIDTConstructor*>(this->_absFrame)->inliner();
}

void
AbsFrameIDTConstructor::traceMethodSummary()
{
   this->_summary->trace();
}
