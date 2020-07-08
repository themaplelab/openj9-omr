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
MethodSummary::predicate(AbsValue *argumentEstimate, int parameter)
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
MethodSummary::trace()
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


MethodSummary::MethodSummary(TR::Region &region, TR::ValuePropagation *vp):
   _region(region),
   _potentialOpts(region),
   _vp(vp)
{
}

void
MethodSummary::addNullCheckFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   NullCheckFolding *opt = new (this->_region) NullCheckFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::addBranchFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::addBranchIfEqFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfEqFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::addBranchIfNeFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfNeFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::addBranchIfGtFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfGtFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::addBranchIfGeFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfGeFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::addBranchIfLeFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfLeFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::addBranchIfLtFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   BranchFolding *opt = new (this->_region) BranchIfLtFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::addIfeq(int bc_index, int argPos)
   {
   this->addBranchIfEqFolding(bc_index, new (_region) AbsValue(TR::VPIntConst::create(this->_vp, 0), TR::Int32), argPos);
   this->addBranchIfEqFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, -1), TR::Int32), argPos);
   this->addBranchIfEqFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 1, INT_MAX), TR::Int32), argPos);
   }

void
MethodSummary::addIfne(int bc_index, int argPos)
   {
   this->addBranchIfNeFolding(bc_index, new (_region) AbsValue(TR::VPIntConst::create(this->_vp, 0), TR::Int32), argPos);
   this->addBranchIfNeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, -1), TR::Int32), argPos);
   this->addBranchIfNeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 1, INT_MAX), TR::Int32), argPos);
   }

void
MethodSummary::addIflt(int bc_index, int argPos)
  {
  this->addBranchIfLtFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, -1), TR::Int32), argPos);
  this->addBranchIfLtFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 0, INT_MAX), TR::Int32), argPos);
  }

void
MethodSummary::addIfle(int bc_index, int argPos)
  {
  this->addBranchIfLeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, 0), TR::Int32), argPos);
  this->addBranchIfLeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 1, INT_MAX), TR::Int32), argPos);
  }

void
MethodSummary::addIfgt(int bc_index, int argPos)
  {
  this->addBranchIfGtFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, 0), TR::Int32), argPos);
  this->addBranchIfGtFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 1, INT_MAX), TR::Int32), argPos);
  }

void
MethodSummary::addIfge(int bc_index, int argPos)
  {
  this->addBranchIfGeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, INT_MIN, -1), TR::Int32), argPos);
  this->addBranchIfGeFolding(bc_index, new (_region) AbsValue(TR::VPIntRange::create(this->_vp, 0, INT_MAX), TR::Int32), argPos);
  }


void
MethodSummary::addInstanceOfFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   InstanceOfFolding *opt = new (this->_region) InstanceOfFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::addCheckCastFolding(int bc_index, AbsValue *constraint, int argPos)
   {
   CheckCastFolding *opt = new (this->_region) CheckCastFolding(bc_index, constraint, argPos);
   this->add(opt);
   }

void
MethodSummary::add(PotentialOptimization *potentialOpt)
   {
   this->_potentialOpts.add(potentialOpt); 
   }

