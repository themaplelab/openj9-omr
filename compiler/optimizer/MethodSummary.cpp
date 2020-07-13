#include "optimizer/MethodSummary.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"



// This works for integer range lattice...
int PotentialOptimization::predicate(TR::VPConstraint *constraint, TR::ValuePropagation *vp)
   {
   if (!constraint)
      return 0;

   if (!_constraint)
      return 0;

   TR::VPConstraint *intersect = constraint->intersect(_constraint, vp);

   if (!intersect)
      return 0;

   return intersect->mustBeEqual(constraint, vp) == false ? 0 : 1;
   }

// int
// NullCheckFolding::test(AbsValue *argumentEstimate, TR::ValuePropagation *valueProp)
// {
//   if (!argumentEstimate) return 0;

//   if (!this->_constraint) return 0;

//   TR::VPConstraint *argumentEstimateVP = argumentEstimate->getConstraint(); // aClass...
//   if (!argumentEstimateVP) return 0;

//   TR::VPClass *argEst = argumentEstimateVP->asClass();
//   if (!argEst) return 0;

//   TR::VPClassPresence *argPres = argEst->getClassPresence();
//   if (!argPres) return 0;

//   TR::VPConstraint *c = _constraint->getConstraint(); // NULL or non-NULL...
//   if (!c) return 0;

//   return argPres->mustBeEqual(argumentEstimateVP, valueProp) == false ? 0 : 1;
// }

int MethodSummary::predicates(TR::VPConstraint *constraint, int paramPosition)
   {
   if (!_potentialOpts.getSize())  
      return 0;

   traceMsg(TR::comp(), "BBBB: method summary printing\n");

   int i = 0;
   ListIterator<PotentialOptimization> iter(&_potentialOpts);
   PotentialOptimization *popt = iter.getFirst();
   while (popt)
      {
      popt->trace(this->_vp);
      if (popt->getParamPosition() == paramPosition)
         {
         traceMsg(TR::comp(), "CCC: argument position matches\n");
         int succuess =  popt->predicate(constraint, _vp);
         i += succuess;
         if (succuess)
            traceMsg(TR::comp(), "CCC: Find an optimization\n");
         }
      popt = iter.getNext();
      }
   return i;
   }

void MethodSummary::trace()
   {
   if (_potentialOpts.getSize())
      return;
   
   traceMsg(TR::comp(), "XXXYYY: Method summary...is NOT empty...\n");
   ListIterator<PotentialOptimization> iter(&_potentialOpts);
   PotentialOptimization *popt = iter.getFirst();
   while (popt) 
      {
      popt->trace(_vp);
      popt = iter.getNext();
      }
   }

// void CheckCastFolding::trace(TR::ValuePropagation *vp)
// {
//    traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", CheckCastFolding::name, this->_bytecode_idx, this->_argPos);
//    this->_constraint->print(vp);
//    traceMsg(TR::comp(), "\n");
   
// }

// void
// InstanceOfFolding::trace(TR::ValuePropagation *vp)
// {
//    traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", InstanceOfFolding::name, this->_bytecode_idx, this->_argPos);
//    this->_constraint->print(vp);
//    traceMsg(TR::comp(), "\n");
   
// }

// void
// NullCheckFolding::trace(TR::ValuePropagation *vp)
// {
//    traceMsg(TR::comp(), "%s in bytecode %d for argument %d constraint", NullCheckFolding::name, this->_bytecode_idx, this->_argPos);
//    this->_constraint->print(vp);
//    traceMsg(TR::comp(), "\n");
// }

void BranchFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Branch Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void BranchIfEqFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Branch IfEq Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void BranchIfNeFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Branch IfNe Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void BranchIfGtFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Branch IfGt Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void BranchIfGeFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Branch IfGe Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void BranchIfLtFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Branch IfLt Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void BranchIfLeFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Branch IfLe Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }



MethodSummary::MethodSummary(TR::Region &region, TR::ValuePropagation *vp):
   _region(region),
   _potentialOpts(region),
   _vp(vp)
{
}

// void
// MethodSummary::addNullCheckFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    NullCheckFolding *opt = new (this->_region) NullCheckFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

// void
// MethodSummary::addBranchFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    BranchFolding *opt = new (this->_region) BranchFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

// void
// MethodSummary::addBranchIfEqFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    BranchFolding *opt = new (this->_region) BranchIfEqFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

// void
// MethodSummary::addBranchIfNeFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    BranchFolding *opt = new (this->_region) BranchIfNeFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

// void
// MethodSummary::addBranchIfGtFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    BranchFolding *opt = new (this->_region) BranchIfGtFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

// void
// MethodSummary::addBranchIfGeFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    BranchFolding *opt = new (this->_region) BranchIfGeFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

// void
// MethodSummary::addBranchIfLeFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    BranchFolding *opt = new (this->_region) BranchIfLeFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

// void
// MethodSummary::addBranchIfLtFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    BranchFolding *opt = new (this->_region) BranchIfLtFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

void MethodSummary::addIfEq(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchIfEqFolding(TR::VPIntConst::create(_vp, 0), paramPosition);
   BranchFolding* p2 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition);
   BranchFolding* p3 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition);

   add(p1);
   add(p2);
   add(p3);
   }

void MethodSummary::addIfNe(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchIfEqFolding(TR::VPIntConst::create(_vp, 0), paramPosition);
   BranchFolding* p2 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition);
   BranchFolding* p3 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition);

   add(p1);
   add(p2);
   add(p3);
   }

void MethodSummary::addIfLt(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition);
   BranchFolding* p2 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, 0, INT_MAX), paramPosition);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfLe(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, INT_MIN, 0), paramPosition);
   BranchFolding* p2 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfGt(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, INT_MIN, 0), paramPosition);
   BranchFolding* p2 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfGe(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition);
   BranchFolding* p2 = new (_region) BranchIfEqFolding(TR::VPIntRange::create(_vp, 0, INT_MAX), paramPosition);

   add(p1);
   add(p2);
   }


// void
// MethodSummary::addInstanceOfFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    InstanceOfFolding *opt = new (this->_region) InstanceOfFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

// void
// MethodSummary::addCheckCastFolding(int bc_index, AbsValue *constraint, int argPos)
//    {
//    CheckCastFolding *opt = new (this->_region) CheckCastFolding(bc_index, constraint, argPos);
//    this->add(opt);
//    }

void MethodSummary::add(PotentialOptimization *potentialOpt)
   {
   _potentialOpts.add(potentialOpt); 
   }

