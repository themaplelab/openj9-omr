#include "MethodSummary.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"

const char * CheckCastFolding::name = "check cast folding";
const char * InstanceOfFolding::name = "instance of folding";
const char * NullCheckFolding::name = "null check folding";
const char * BranchFolding::name = "branch folding";
const char * BranchIfEqFolding::name = "branch folding if eq";
const char * BranchIfNeFolding::name = "branch folding if ne";
const char * BranchIfLtFolding::name = "branch folding if lt";
const char * BranchIfLeFolding::name = "branch folding if le";
const char * BranchIfGeFolding::name = "branch folding if ge";
const char * BranchIfGtFolding::name = "branch folding if gt";

// This works for integer range lattice...
int
PotentialOptimization::test(AbsValue *argumentEstimate, TR::ValuePropagation *valueProp)
{
  if (!argumentEstimate) return 0;
  if (!this->_constraint) return 0;

  TR::VPConstraint *argumentEstimateVP = argumentEstimate->getConstraint();
  if (!argumentEstimateVP) return 0;

  TR::VPConstraint *c = _constraint->getConstraint();
  if (!c) return 0;


  TR::VPConstraint *intersect = argumentEstimateVP->intersect(c, valueProp);
  if (!intersect) return 0;

  return intersect->mustBeEqual(argumentEstimateVP, valueProp) == false ? 0 : 1;
}

int
NullCheckFolding::test(AbsValue *argumentEstimate, TR::ValuePropagation *valueProp)
{
  if (!argumentEstimate) return 0;

  if (!this->_constraint) return 0;

  TR::VPConstraint *argumentEstimateVP = argumentEstimate->getConstraint(); // aClass...
  if (!argumentEstimateVP) return 0;

  TR::VPClass *argEst = argumentEstimateVP->asClass();
  if (!argEst) return 0;

  TR::VPClassPresence *argPres = argEst->getClassPresence();
  if (!argPres) return 0;
;
  TR::VPConstraint *c = _constraint->getConstraint(); // NULL or non-NULL...
  if (!c) return 0;

  return argPres->mustBeEqual(argumentEstimateVP, valueProp) == false ? 0 : 1;
}

int
MethodSummaryExtension::predicate(AbsValue *argumentEstimate, int parameter)
{
   if (!this->_potentialOpts.getSize()) { 
     return 0;
   }
   traceMsg(TR::comp(), "BBBB: method summary printing\n");

   int i = 0;
   ListIterator<PotentialOptimization> iter(&this->_potentialOpts);
   PotentialOptimization *popt = iter.getFirst();
   while (popt) {
     popt->trace(this->_vp);
     if (popt->_argPos == parameter) {
       traceMsg(TR::comp(), "CCC: argument position matches\n");
       int succuess =  popt->test(argumentEstimate, this->_vp);
       i+= succuess;
       if (succuess)
        traceMsg(TR::comp(), "CCC: Find optimizations\n");
     }
     popt = iter.getNext();
   }
   return i;
}

void
MethodSummaryExtension::trace()
{
   if (!this->_potentialOpts.getSize()) { 
     return;
   }
   traceMsg(TR::comp(), "XXXYYY: Method summary...is NOT empty...\n");
   ListIterator<PotentialOptimization> iter(&this->_potentialOpts);
   PotentialOptimization *popt = iter.getFirst();
   while (popt) {
     popt->trace(this->_vp);
     popt = iter.getNext();
   }
}

void
CheckCastFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", CheckCastFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   
}

void
InstanceOfFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", InstanceOfFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   
}

void
NullCheckFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", NullCheckFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
}

void
BranchFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", BranchFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
}

void
BranchIfEqFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", BranchIfEqFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
}

void
BranchIfNeFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", BranchIfNeFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
}

void
BranchIfGtFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", BranchIfGtFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
}

void
BranchIfGeFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", BranchIfGeFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
}

void
BranchIfLtFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", BranchIfLtFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
}

void
BranchIfLeFolding::trace(TR::ValuePropagation *vp)
{
   traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", BranchIfLeFolding::name, this->_bytecode_idx, this->_argPos);
   this->_constraint->print(vp);
   traceMsg(TR::comp(), "\n");
}

void
PotentialOptimization::trace(TR::ValuePropagation* vp)
{
   this->_constraint->print(vp);
}


MethodSummaryExtension::MethodSummaryExtension(TR::Region &region, TR::ValuePropagation *vp):
   _region(region),
   _potentialOpts(region),
   _vp(vp)
{
}

void
MethodSummaryExtension::addNullCheckFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   NullCheckFolding *opt = new (this->_region) NullCheckFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::addBranchFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::addBranchIfEqFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfEqFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::addBranchIfNeFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfNeFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::addBranchIfGtFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfGtFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::addBranchIfGeFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfGeFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::addBranchIfLeFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfLeFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::addBranchIfLtFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfLtFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::addIfeq(int bc_index, int argPos)
   {
   this->addBranchIfEqFolding(bc_index, new (_region) AbsValue(TR::VPIntConst::create(this->_vp, 0), TR::Int32), argPos);
   this->addBranchIfEqFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, -1), TR::Int32), argPos);
   this->addBranchIfEqFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 1, INT_MAX), TR::Int32), argPos);
   }

void
MethodSummaryExtension::addIfne(int bc_index, int argPos)
   {
   this->addBranchIfNeFolding(bc_index, new (_region) AbsValue(TR::VPIntConst::create(this->_vp, 0), TR::Int32), argPos);
   this->addBranchIfNeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, -1), TR::Int32), argPos);
   this->addBranchIfNeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 1, INT_MAX), TR::Int32), argPos);
   }

void
MethodSummaryExtension::addIflt(int bc_index, int argPos)
  {
  this->addBranchIfLtFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, -1), TR::Int32), argPos);
  this->addBranchIfLtFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 0, INT_MAX), TR::Int32), argPos);
  }

void
MethodSummaryExtension::addIfle(int bc_index, int argPos)
  {
  this->addBranchIfLeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, 0), TR::Int32), argPos);
  this->addBranchIfLeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 1, INT_MAX), TR::Int32), argPos);
  }

void
MethodSummaryExtension::addIfgt(int bc_index, int argPos)
  {
  this->addBranchIfGtFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, 0), TR::Int32), argPos);
  this->addBranchIfGtFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 1, INT_MAX), TR::Int32), argPos);
  }

void
MethodSummaryExtension::addIfge(int bc_index, int argPos)
  {
  this->addBranchIfGeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, -1), TR::Int32), argPos);
  this->addBranchIfGeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 0, INT_MAX), TR::Int32), argPos);
  }


void
MethodSummaryExtension::addInstanceOfFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   InstanceOfFolding *opt = new (this->_region) InstanceOfFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::addCheckCastFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   CheckCastFolding *opt = new (this->_region) CheckCastFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummaryExtension::add(PotentialOptimization *potentialOpt)
   {
   this->_potentialOpts.add(potentialOpt); 
   }

MethodSummary::MethodSummary(TR::Compilation *comp, TR::Region &region, TR::ValuePropagation *vp, IDTNode* hunk):
  _methodSummaryNew(comp->trMemory())
  , _hunk(hunk)
  , _index(0)
  , _region(region)
  , _comp(comp)
  , _vp(vp)
{
}

MethodSummaryRow::MethodSummaryRow(TR::Compilation *comp, TR::Region &region)
  : _caller_idx(-1)
  , _callee_idx(-1)
  , _bytecode_idx(-1)
  , _region(region)
  , _comp(comp)
  , _benefit(1)
{
  this->_pt = MethodSummaryRow::PotentialTransform::UNK;
  this->_row = new (this->_region) AbsVarArray(comp, region);
}

int
MethodSummary::getIndex(void) {
  return this->_index++;
}

TR::VPConstraint*
MethodSummaryRow::at(int n) {
  return this->_row->at(n);
}

void
MethodSummaryRow::at(int n, TR::VPConstraint *constraint) {
  this->_row->at(n, constraint);
}

void 
MethodSummaryRow::transform(MethodSummaryRow::PotentialTransform pt) {
  this->_pt = pt;
}

MethodSummaryRow::PotentialTransform
MethodSummaryRow::transform() {
  return this->_pt;
}

MethodSummaryRow*
MethodSummary::newCHECK_CAST(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::CHECK_CAST, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newIFLT(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::IFLT, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newSTR_LEN(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::STR_LEN, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newIFLE(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::IFLE, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newIFEQ(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::IFEQ, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newIFNE(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::IFNE, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newIFGE(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::IFGE, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newIFGT(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::IFGT, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newINOF(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::INOF, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newIFNU(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::IFNU, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newIFNN(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::IFNN, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newNLCK(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::NLCK, hunk, bytecodeIndex);
}

MethodSummaryRow*
MethodSummary::newSIZE(IDTNode* hunk, int bytecodeIndex) {
  return this->getRowSummary(MethodSummaryRow::PotentialTransform::SIZE, hunk, bytecodeIndex);
}


MethodSummaryRow*
MethodSummary::getRowSummary(MethodSummaryRow::PotentialTransform pt, IDTNode *hunk, int bytecodeIndex) {
  MethodSummaryRow *retval = this->getRowSummary();
  if (!retval) return NULL;
  retval->transform(pt);
  retval->_bytecode_idx = bytecodeIndex;
  retval->_caller_idx = hunk->getCallerIndex();
  retval->_callee_idx = hunk->getCalleeIndex();
  return retval;
}

MethodSummaryRow*
MethodSummary::getRowSummary() {
  int index = this->getIndex();
  MethodSummaryRow* methodSummaryRow = new (this->_region) MethodSummaryRow(this->_comp, this->_region);
  this->_methodSummaryNew.add(methodSummaryRow);
  return methodSummaryRow;
}

void
MethodSummary::printMethodSummary(int numberOfParams) {
  int methodSummarySize = this->_index;
  if (methodSummarySize <= 0) { return; }
  //int numberOfParams = 10;//this->numberOfParametersInCurrentMethod();
  traceMsg(this->_comp, "| Opt            | Benefit      ");
  for (int j = 0; j < numberOfParams ; j++) {
    traceMsg(this->_comp, "| Arg%d", j);
  }
  traceMsg(this->_comp, "|\n");
  for (int j = 0; j < numberOfParams + 2; j++) {
    traceMsg(this->_comp, "|--------");
  }
  traceMsg(this->_comp, "|\n");
  ListIterator<MethodSummaryRow> methodSummaryRowIt(&this->_methodSummaryNew);
  for (MethodSummaryRow *summaryRow = methodSummaryRowIt.getFirst(); summaryRow != NULL; summaryRow = methodSummaryRowIt.getNext()) {
  //for (int i = 0; i < methodSummarySize; i++) {
    //MethodSummaryRow *summaryRow = methodSummary[i];
    MethodSummaryRow::PotentialTransform pt = summaryRow->transform();
    char *toPrint;
    switch(pt) {
      case MethodSummaryRow::PotentialTransform::IFLT: 
      toPrint = "IFLT";
      break;
      case MethodSummaryRow::PotentialTransform::IFLE: 
      toPrint = "IFLE";
      break;
      case MethodSummaryRow::PotentialTransform::IFEQ: 
      toPrint = "IFEQ";
      break;
      case MethodSummaryRow::PotentialTransform::IFNE: 
      toPrint = "IFNE";
      break;
      case MethodSummaryRow::PotentialTransform::IFGE: 
      toPrint = "IFGE";
      break;
      case MethodSummaryRow::PotentialTransform::IFGT: 
      toPrint = "IFGT";
      break;
      case MethodSummaryRow::PotentialTransform::INOF: 
      toPrint = "INOF";
      break;
      case MethodSummaryRow::PotentialTransform::IFNU: 
      toPrint = "IFNU";
      break;
      case MethodSummaryRow::PotentialTransform::IFNN: 
      toPrint = "IFNN";
      break;
      case MethodSummaryRow::PotentialTransform::NLCK: 
      toPrint = "NLCK";
      break;
      default:
      toPrint = "UNK";
      break;
    }
    traceMsg(this->_comp, "|\t%s\t", toPrint );
    traceMsg(this->_comp, "|\t%d\t", summaryRow->benefit());
    for (int j = 0; j < numberOfParams; j++) {
      TR::VPConstraint *constraint = summaryRow->at(j);
      if (!constraint) {
        traceMsg(this->_comp, "|\tFREE\t");
        continue;
      }
      traceMsg(this->_comp, "|\t");
      constraint->print(this->_comp, this->_comp->getOutFile());
      traceMsg(this->_comp, "\t");
    }  
    traceMsg(this->_comp, "|\n");
  }
}

int
MethodSummary::compareInformationAtCallSite(TR_ResolvedMethod *resolvedMethod, AbsVarArray* argumentConstraints) {

  if (!resolvedMethod) { return 0; }

  auto numberOfParameters = resolvedMethod->numberOfParameters();
  const auto numberOfExplicitParameters = resolvedMethod->numberOfExplicitParameters();
  const auto numberOfImplicitParameters = numberOfParameters - numberOfExplicitParameters;
  const auto hasImplicitParameter = numberOfImplicitParameters > 0;

  TR::VPConstraint **withoutContext = new (this->_region) TR::VPConstraint *[numberOfParameters];

  if (hasImplicitParameter) {
     TR_OpaqueClassBlock *implicitParameterClass = resolvedMethod->containingClass();
     TR::VPConstraint *implicitParameterConstraint = this->getClassConstraint(implicitParameterClass);
     withoutContext[0] = implicitParameterConstraint;
  }
  TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*this->_comp);

  for (int i = numberOfImplicitParameters; !parameterIterator->atEnd(); parameterIterator->advanceCursor(), i++) {
    auto parameter = parameterIterator;
    const bool isClass = parameter->isClass();
    if (!isClass) {
        withoutContext[i] = NULL;
        continue;
    }

    TR_OpaqueClassBlock *parameterClass = parameter->getOpaqueClass();
    if (!parameterClass) {
       withoutContext[i] = NULL;
       continue;
    }

    TR::VPConstraint *explicitParameterConstraint = this->getClassConstraint(parameterClass);
    withoutContext[i] = explicitParameterConstraint;
  }

  // now we have a without context and we also have the AbsVarArray... so we need to iterate over both.
  int benefit = 0;
  for (int i = 0; i < numberOfParameters; i++) {
     TR::VPConstraint *constraintWithContext = argumentConstraints->at(i);
     TR::VPConstraint *constraintWithoutContext = withoutContext[i];
     bool noInformation = !constraintWithoutContext && !constraintWithContext;
     if (noInformation) continue;

     bool noUsefulInformation = constraintWithoutContext && !constraintWithContext;
     if (noUsefulInformation) continue;

     bool uncompareableInformation = !constraintWithoutContext && constraintWithContext;
     if (!uncompareableInformation) {
       benefit++;
       continue;
     }

     TR::VPConstraint *intersection  = constraintWithContext->intersect(constraintWithoutContext, this->_vp);
     //bool subset = intersection == constraintWithContext;
     if (!intersection) continue;

     if (this->_comp->trace(OMR::inlining)) {
       //traceMsg(this->_comp, "subset\n");
       //intersection->print(this->_comp, this->_comp->getOutFile());
     }
     benefit++;
  }

  return benefit;
}

TR::VPConstraint*
MethodSummary::getClassConstraint(TR_OpaqueClassBlock *_class) {
  if (!_class) {
     return NULL;
  }

  TR::VPClassType *fixedClass = TR::VPFixedClass::create(this->_vp, _class);
  TR::VPConstraint *classConstraint = TR::VPClass::create(this->_vp, fixedClass, NULL, NULL, NULL, NULL);
  return classConstraint;
}
// char *
// MethodSummary::toString(){
//    return "ok";
// }
char*
MethodSummary::toString() {
  // Ok, so first we need a region of memory that is big enough to hold information
  // about the method summary.
  auto const buffer_length = 32768;
  char* buffer = new (this->_region) char [buffer_length];
  int index = 0;
  memset(buffer, 0, buffer_length);
  static const char* signatureChars = feGetEnv("TR_ProfitabilitySignatureChars");
  if (!signatureChars) {
  const char* name = this->_hunk->getName();
  size_t const name_length = strnlen(name, buffer_length);
  memcpy(buffer, name, name_length);
  index += name_length;
  }

  // isEmpty
  ListIterator<MethodSummaryRow> methodSummaryRowIt(&this->_methodSummaryNew);
  if (methodSummaryRowIt.getFirst() == NULL) {
    int retval = snprintf(buffer + index, buffer_length - index, "\nempty method summary\n");
    index += retval;
    return buffer;
  }

  // new line is break between name and table
  const char* optBenefitString = "\n|\tOpt\t|\tBenefit\t|\tLocation\t";
  const size_t optBenefitStringLength = strnlen(optBenefitString, buffer_length - index);
  memcpy(buffer + index, optBenefitString, optBenefitStringLength);
  index += optBenefitStringLength;

  auto hunk = this->_hunk;
  TR_ASSERT(hunk, "hunk is null");
  int numberOfParams = hunk->numberOfParameters();
  for (int j = 0; j < numberOfParams ; j++) {
    const char* argString = "|\tArg\t";
    const size_t argStringLength = strnlen(argString, buffer_length - index);
    memcpy(buffer + index, argString, argStringLength);
    index += argStringLength;
  }
  
  const char* separatorString = "|\n";
  const size_t separatorStringLength = strnlen(separatorString, buffer_length - index);
  memcpy(buffer + index, separatorString, separatorStringLength);
  index += separatorStringLength;



#ifdef TR_PROFITABILITY_VERIFY
  AbsVarArray *argumentConstraints = hunk->_argumentConstraints; // needed for printing the method summary?
#else
  AbsVarArray *argumentConstraints = NULL;
#endif
  if (!argumentConstraints) return buffer; //TODO: FIXME

  int aggregatedBenefit = 0;
  int i = 0;
  for (MethodSummaryRow *summaryRow = methodSummaryRowIt.getFirst(); summaryRow != NULL; summaryRow = methodSummaryRowIt.getNext()) {
    i++;
    MethodSummaryRow::PotentialTransform pt = summaryRow->transform();
    char *toPrint;
    switch(pt) {
      case MethodSummaryRow::PotentialTransform::IFLT: 
      toPrint = "IFLT";
      break;
      case MethodSummaryRow::PotentialTransform::IFLE: 
      toPrint = "IFLE";
      break;
      case MethodSummaryRow::PotentialTransform::IFEQ: 
      toPrint = "IFEQ";
      break;
      case MethodSummaryRow::PotentialTransform::IFNE: 
      toPrint = "IFNE";
      break;
      case MethodSummaryRow::PotentialTransform::IFGE: 
      toPrint = "IFGE";
      break;
      case MethodSummaryRow::PotentialTransform::IFGT: 
      toPrint = "IFGT";
      break;
      case MethodSummaryRow::PotentialTransform::INOF: 
      toPrint = "INOF";
      break;
      case MethodSummaryRow::PotentialTransform::IFNU: 
      toPrint = "IFNU";
      break;
      case MethodSummaryRow::PotentialTransform::IFNN: 
      toPrint = "IFNN";
      break;
      case MethodSummaryRow::PotentialTransform::NLCK: 
      toPrint = "NLCK";
      break;
      case MethodSummaryRow::PotentialTransform::STR_LEN: 
      toPrint = "STR_LEN";
      break;
      case MethodSummaryRow::PotentialTransform::CHECK_CAST: 
      toPrint = "CHECK_CAST";
      break;
      default:
      toPrint = "UNKN";
      break;
   }
    const char* recordSeparatorString = "|\t";
    const size_t recordSeparatorStringLength = strnlen(recordSeparatorString, buffer_length - index);
    memcpy(buffer + index, recordSeparatorString, recordSeparatorStringLength);
    index += recordSeparatorStringLength;

    const size_t toPrintLength = strnlen(toPrint, buffer_length - index);
    memcpy(buffer + index, toPrint, toPrintLength);
    index += toPrintLength;

    index += snprintf(buffer + index, buffer_length - index, "\t|\t%d\t|\ter=%d,ee=%d,index=%d\t", summaryRow->benefit(), summaryRow->caller_idx(), summaryRow->callee_idx(), summaryRow->bc_idx());


    for (int j = 0; j < numberOfParams; j++) {
      TR::VPConstraint *transformationConstraint = summaryRow->at(j);
      if (!transformationConstraint) {
        index += snprintf(buffer + index, buffer_length - index, "|\tFREE\t");
        continue;
      }

      TR::VPConstraint *contextConstraint = argumentConstraints->at(j);
      if (!contextConstraint) { 
        index += snprintf(buffer + index, buffer_length - index, "|\t");
        //index += transformationConstraint->snprint(buffer + index, buffer_length - index);
        index += snprintf(buffer + index, buffer_length - index, "\t");
        continue;
      }

      TR::VPConstraint *subset = NULL;
/*
      TODO: Andrew said that we don't need subset, but look at the VPConstraint API
      if (pt == MethodSummaryRow::PotentialTransform::IFNN)
      {
        auto classPresence = contextConstraint->getClassPresence();
        subset = classPresence ? classPresence->subset(transformationConstraint, this->_vp) : NULL;
      } else if (pt == MethodSummaryRow::PotentialTransform::IFNU) {
        auto classPresence = contextConstraint->getClassPresence();
        subset = classPresence ? classPresence->subset(transformationConstraint, this->_vp) : NULL;
      } else if (pt ==  MethodSummaryRow::PotentialTransform::NLCK) {
        TR::VPConstraint *classPresence = contextConstraint->getClassPresence(); 
        TR::VPConstraint *nonNull = classPresence ? classPresence : contextConstraint->asNonNullObject();
        subset = subset ? subset : contextConstraint->asConstString();
      } else if (pt ==  MethodSummaryRow::PotentialTransform::INOF) {
        subset = strcmp(contextConstraint->name(), transformationConstraint->name()) == 0 ? contextConstraint : NULL;
      } else if (pt ==  MethodSummaryRow::PotentialTransform::CHECK_CAST) {
        subset = contextConstraint->intersect1(transformationConstraint, this->_vp);
      } else {
        subset = contextConstraint->subset(transformationConstraint, this->_vp);
      }
      if (!subset) { 
        index += snprintf(buffer + index, buffer_length - index, "|\t");
        //index += transformationConstraint->snprint(buffer + index, buffer_length - index);
        index += snprintf(buffer + index, buffer_length, "\t");
        continue;
      }
      index += snprintf(buffer + index, buffer_length - index, "|\t**");
      //index += subset->snprint(buffer + index, buffer_length - index);
      index += snprintf(buffer + index, buffer_length - index, " from ");
      //index += contextConstraint->snprint(buffer + index, buffer_length - index);
      index += snprintf(buffer + index, buffer_length - index, " in ");
      //index += transformationConstraint->snprint(buffer + index, buffer_length - index);
      index += snprintf(buffer + index, buffer_length - index, "**\t");
      aggregatedBenefit += summaryRow->benefit();
*/

    }
    index += snprintf(buffer + index, buffer_length - index, "|\n");
  }
  index += snprintf(buffer + index, buffer_length - index, "BENEFIT:%d\n", aggregatedBenefit);
  return buffer;
}


//TODO: change name
int
MethodSummary::applyVerbose(AbsVarArray *argumentConstraints, IDTNode* hunk) {
  
    int methodSummarySize = this->_index;
    if (methodSummarySize <= 0)  {
     return 1;
    }

    TR::ResolvedMethodSymbol *resolvedMethodSymbol = hunk->getResolvedMethodSymbol();
    TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
    int benefit = this->compareInformationAtCallSite(resolvedMethod, argumentConstraints);
    //traceMsg(this->_comp, "### name : %s\n", hunk->name());
    //traceMsg(this->_comp, "CONTEXT CONSTRAINTS:\n");
    //argumentConstraints->print();
    //traceMsg(this->_comp, "\n");
    //traceMsg(this->_comp, "METHOD SUMMARY:\n");
    benefit += this->applyVerbose(argumentConstraints, hunk->numberOfParameters());
    return benefit + 1; // non-zeros
}

int
MethodSummary::applyVerbose(AbsVarArray *argumentConstraints, int numberOfParams) {

  int methodSummarySize = this->_index;
  int aggregatedBenefit = 0;

  
  traceMsg(this->_comp, "| Opt            | Benefit      ");
  for (int j = 0; j < numberOfParams ; j++) {
    traceMsg(this->_comp, " | Arg%d ", j);
  }
  traceMsg(this->_comp, "|\n");
  for (int j = 0; j < numberOfParams + 2; j++) {
    traceMsg(this->_comp, "|--------");
  }
  traceMsg(this->_comp, "|\n");

  ListIterator<MethodSummaryRow> methodSummaryRowIt(&this->_methodSummaryNew);
  int i = 0;
  for (MethodSummaryRow *summaryRow = methodSummaryRowIt.getFirst(); summaryRow != NULL; summaryRow = methodSummaryRowIt.getNext()) {
    i++;
    MethodSummaryRow::PotentialTransform pt = summaryRow->transform();
    char *toPrint;
    switch(pt) {
      case MethodSummaryRow::PotentialTransform::IFLT: 
      toPrint = "IFLT";
      break;
      case MethodSummaryRow::PotentialTransform::IFLE: 
      toPrint = "IFLE";
      break;
      case MethodSummaryRow::PotentialTransform::IFEQ: 
      toPrint = "IFEQ";
      break;
      case MethodSummaryRow::PotentialTransform::IFNE: 
      toPrint = "IFNE";
      break;
      case MethodSummaryRow::PotentialTransform::IFGE: 
      toPrint = "IFGE";
      break;
      case MethodSummaryRow::PotentialTransform::IFGT: 
      toPrint = "IFGT";
      break;
      case MethodSummaryRow::PotentialTransform::INOF: 
      toPrint = "INOF";
      break;
      case MethodSummaryRow::PotentialTransform::IFNU: 
      toPrint = "IFNU";
      break;
      case MethodSummaryRow::PotentialTransform::IFNN: 
      toPrint = "IFNN";
      break;
      case MethodSummaryRow::PotentialTransform::NLCK: 
      toPrint = "NLCK";
      break;
      default:
      toPrint = "UNK";
      break;
    }

    traceMsg(this->_comp, "|\t%s\t", toPrint );
    traceMsg(this->_comp, "|\t%d\t", summaryRow->benefit());
    bool applied = false;
    for (int j = 0; j < numberOfParams; j++) {

      TR::VPConstraint *transformationConstraint = summaryRow->at(j);
      if (!transformationConstraint) {
        traceMsg(this->_comp, "|\tFREE\t");
        continue;
      }


      TR::VPConstraint *contextConstraint = argumentConstraints->at(j);
      if (!contextConstraint) { 
        traceMsg(this->_comp, "|\t");
        transformationConstraint->print(this->_comp, this->_comp->getOutFile());
        traceMsg(this->_comp, "\t");
        continue;
      }

      //bool subset = intersection;
      //bool subset = intersection == contextConstraint;
      TR::VPConstraint *subset = NULL;
      
      if (pt == MethodSummaryRow::PotentialTransform::IFNN)
      {
        TR::VPConstraint *classPresence = contextConstraint->getClassPresence(); 
        TR::VPConstraint *nonNull = classPresence ? classPresence : contextConstraint->asNonNullObject();
      //   subset = nonNull ? nonNull->subset(transformationConstraint, this->_vp) : NULL;
      } else if (pt == MethodSummaryRow::PotentialTransform::IFNU) {
        TR::VPConstraint *classPresence = contextConstraint->getClassPresence(); 
        TR::VPConstraint *nullObject = classPresence ? classPresence : contextConstraint->asNonNullObject();
      //   subset = nullObject ? nullObject->subset(transformationConstraint, this->_vp) : NULL;
      } else if (pt ==  MethodSummaryRow::PotentialTransform::NLCK) {
        TR::VPConstraint *classPresence = contextConstraint->getClassPresence(); 
        TR::VPConstraint *nonNull = classPresence ? classPresence : contextConstraint->asNonNullObject();
      //   subset = nonNull ? nonNull->subset(transformationConstraint, this->_vp) : NULL;
      //   subset = subset ? subset : contextConstraint->asConstString();
      } else if (pt ==  MethodSummaryRow::PotentialTransform::INOF) {
      //   subset = strcmp(contextConstraint->name(), transformationConstraint->name()) == 0 ? contextConstraint : NULL;
      } else if (pt ==  MethodSummaryRow::PotentialTransform::CHECK_CAST) {
      //   subset = contextConstraint->intersect1(transformationConstraint, this->_vp);
      } else {
      //   subset = contextConstraint->subset(transformationConstraint, this->_vp);
      }
      if (!subset) { 
        traceMsg(this->_comp, "|\t");
        transformationConstraint->print(this->_comp, this->_comp->getOutFile());
        traceMsg(this->_comp, "\t");
        continue;
      }
   
      traceMsg(this->_comp, "|\t**");
      contextConstraint->print(this->_comp, this->_comp->getOutFile());
      traceMsg(this->_comp, " in ");
      transformationConstraint->print(this->_comp, this->_comp->getOutFile());
      // subset->print(this->_comp, this->_comp->getOutFile());
      traceMsg(this->_comp, "**\t");
      aggregatedBenefit += summaryRow->benefit();
      applied |= true;
      
    }
    traceMsg(this->_comp, "|\n");
#ifdef TR_PROFITABILITY_VERIFY
    if (applied && this->_comp->_index < 100) {
        this->_comp->transformEnum[this->_comp->_index] = i;
        this->_comp->callee_idx[this->_comp->_index] = summaryRow->callee_idx();
        this->_comp->bytecode_idx[this->_comp->_index] = summaryRow->bc_idx();
        this->_comp->_index++;
    }
#endif
    applied = false;
  }
  return aggregatedBenefit;
}

// int
// MethodSummary::apply(AbsVarArray *a){ return 1; }

// int
// MethodSummary::apply(AbsVarArray *argumentConstraints) {
//   List<MethodSummaryRow> methodSummary = this->_methodSummaryNew;
//   int methodSummarySize = this->_index;
//   int numberOfParams = 10;
//   int aggregatedBenefit = 0;
//   for (int i = 0; i < methodSummarySize; i++) {
//     MethodSummaryRow summaryRow = methodSummary.;
//     for (int j = 0; j < numberOfParams; j++) {
//       TR::VPConstraint *transformationConstraint = summaryRow.at(j);
//       if (!transformationConstraint) continue;
//       TR::VPConstraint *contextConstraint = argumentConstraints->at(j);
//       if (!contextConstraint) continue;
//       bool intersects = contextConstraint->intersect(transformationConstraint, this->_vp);
//       if (!intersects) continue;
   
//       aggregatedBenefit += summaryRow->benefit();
//     }
//   }
//   return aggregatedBenefit;
// }
