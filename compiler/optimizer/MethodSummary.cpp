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
   }

void
MethodSummaryExtension::addIfne(int bc_index, int argPos)
   {
   this->addBranchIfNeFolding(bc_index, new (_region) AbsValue(TR::VPIntConst::create(this->_vp, 0), TR::Int32), argPos);
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

