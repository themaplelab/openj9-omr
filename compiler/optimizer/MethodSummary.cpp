#include "optimizer/MethodSummary.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"

//TODO: Do we need to consider unsigned integers?
// Be careful, other can be an VPIntConst, VPIntRange or VPMergedConstraint
// _constraint can be a VPIntConst or VPIntRange 
int BranchFolding::predicate(TR::VPConstraint *other, TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Contraint to Compare: ");

   if (!other)
      {
      traceMsg(TR::comp(), "TOP\n");
      return 0;
      }

   other->print(vp);
   traceMsg(TR::comp(), "\n");

   TR::VPIntConst* otherIntConst = other->asIntConst();
   TR::VPIntRange* otherIntRange = other->asIntRange();
   TR::VPMergedConstraints* otherMerged = other->asMergedIntConstraints();

   TR::VPIntRange* constraintIntRange = _constraint->asIntRange();
   if (constraintIntRange) //If the constraint is a range
      {
      int32_t constraintLow = constraintIntRange->getLowInt();
      int32_t constraintHigh = constraintIntRange->getHighInt();

      if (otherIntConst)
         {
         if (constraintLow <= otherIntConst->getInt() && constraintHigh >= otherIntConst->getInt())
            return 1;
         return 0;
         }

      if (otherIntRange)
         {
         if (otherIntRange->getLowInt() >= constraintLow && otherIntRange->getHighInt() <= constraintHigh)
            return 1;
         return 0;
         }

      if (otherMerged)
         {
         if (otherMerged->getLowInt() >= constraintLow && otherMerged->getHighInt() <= constraintHigh)
            return 1;
         return 0;
         }
      
      return 0;

      }

   TR::VPIntConst* constraintIntConst = _constraint->asIntConst();
   if (constraintIntConst) //If the constraint is an int const
      {
      if (otherIntConst)
         {
         if (otherIntConst->getInt() == constraintIntConst->getInt())
            return 1;
         return 0;
         }
      return 0;
      }

   return 0;
   }

int NullBranchFolding::predicate(TR::VPConstraint *other, TR::ValuePropagation* vp)
   {
   if (!other)
      return 0;
   traceMsg(TR::comp(), "Contraint to Compare: ");
   other->print(vp);
   traceMsg(TR::comp(), "\n");

   TR::VPClass* otherClass = other->asClass(); 

   TR::VPNullObject* otherNullObject = other->asNullObject();

   TR::VPNullObject* constraintNullObject = _constraint->asNullObject();
   TR::VPNonNullObject* constraintNonNullObject = _constraint->asNonNullObject();

   if (constraintNullObject && otherNullObject) //both null
      return 1; 

   if (constraintNonNullObject && otherClass && otherClass->getClassPresence()->asNonNullObject() ) //both non null
      return 1;

   return 0;
   }

int NullCheckFolding::predicate(TR::VPConstraint* other, TR::ValuePropagation *vp)
   {
   if (!other)
      return 0;

   traceMsg(TR::comp(), "Contraint to Compare: ");
   other->print(vp);
   traceMsg(TR::comp(), "\n");

   TR::VPClass* otherClass = other->asClass(); 
   TR::VPNullObject* otherNullObject = other->asNullObject();

   TR::VPNullObject* constraintNullObject = _constraint->asNullObject();
   TR::VPNonNullObject* constraintNonNullObject = _constraint->asNonNullObject();

   if (constraintNullObject && otherNullObject) //both null
      return 1; 

   if (constraintNonNullObject && otherClass && otherClass->getClassPresence()->asNonNullObject()) //both non null
      return 1;
      
   return 0;
   }


//Note: other can be VPClass or VPNullObject
// constraint is VPFixedClass or VPNullObject
int InstanceOfFolding::predicate(TR::VPConstraint* other, TR::ValuePropagation *vp)
   {
   if (!other)
      return 0;

   traceMsg(TR::comp(), "Contraint to Compare: ");
   other->print(vp);
   traceMsg(TR::comp(), "\n");

   TR::VPNullObject* constraintNullObject = _constraint->asNullObject();
   TR::VPFixedClass* constraintClassType = _constraint->asFixedClass();

   TR::VPNullObject* otherNullObject = other->asNullObject();
   TR::VPClass* otherClass = other->asClass();

   if (constraintNullObject && otherNullObject) //Both are null
      return 1;
      
   if (constraintClassType && otherClass && otherClass->getClassType() && otherClass->getClassType()->asFixedClass()) //Both are types
      {

      if (constraintClassType->getClass() == otherClass->getClassType()->getClass())
         return 1;

      }

   return 0;
   }

int CheckCastFolding::predicate(TR::VPConstraint* other, TR::ValuePropagation *vp)
   {
   if (!other)
      return 0;

   traceMsg(TR::comp(), "Contraint to Compare: ");
   other->print(vp);
   traceMsg(TR::comp(), "\n");

   TR::VPClass* otherClass = other->asClass();

   TR::VPFixedClass* constraintClassType = _constraint->getClassType()->asFixedClass();

   if (otherClass && otherClass->getClassType()->asFixedClass() && constraintClassType)
      if (otherClass->getClassType()->getClass() == constraintClassType->getClass())
         return 1;
   
   return 0;
   }

int MethodSummary::predicates(TR::VPConstraint *constraint, int paramPosition)
   {
   if (!_potentialOpts.getSize() || !constraint )  
      return 0;

   traceMsg(TR::comp(), "#### Predicates param %d ####\n", paramPosition);

   int i = 0;
   ListIterator<PotentialOptimization> iter(&_potentialOpts);
   PotentialOptimization *opt = iter.getFirst();
   while (opt)
      {
      if (opt->getParamPosition() == paramPosition)
         {
         opt->trace(_vp);
         traceMsg(TR::comp(), "CCC: argument position matches\n");
         int succuess =  opt->predicate(constraint, _vp);
         i += succuess;
         if (succuess)
            traceMsg(TR::comp(), "CCC: Predicate TRUE\n");
         else
            traceMsg(TR::comp(), "CCC: Predicate FALSE\n");
         }
      opt = iter.getNext();
      }
   return i;
   }

void MethodSummary::trace()
   {
   if (_potentialOpts.getSize() == 0)
      return;
   
   traceMsg(TR::comp(), "#### All Method Summaries ####\n");
   ListIterator<PotentialOptimization> iter(&_potentialOpts);
   PotentialOptimization *opt = iter.getFirst();
   while (opt) 
      {
      opt->trace(_vp);
      opt = iter.getNext();
      }
   }

void BranchFolding::trace(TR::ValuePropagation *vp)
   {
   char* branchFoldingName;

   switch (_kind)
      {
      case Kinds::IfEq:
         branchFoldingName = "IfEq";
         break;

      case Kinds::IfGe:
         branchFoldingName = "IfGe";
         break;

      case Kinds::IfGt:
         branchFoldingName = "IfGt";
         break;
      
      case Kinds::IfLe:
         branchFoldingName = "IfLe";
         break;

      case Kinds::IfLt:
         branchFoldingName = "IfLt";
         break;

      case Kinds::IfNe:
         branchFoldingName = "IfNe";
         break;

      case Kinds::IfNonNull:
         branchFoldingName = "IfNonNull";
         break;

      case Kinds::IfNull:
         branchFoldingName = "IfNull";
         break;
      
      default:
         branchFoldingName = "Unknown";
         break;
      }

   traceMsg(TR::comp(), "Branch Folding %s for argument %d constraint: ", branchFoldingName, _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void NullCheckFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Null Check Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void InstanceOfFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Instanceof Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void CheckCastFolding::trace(TR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Checkcast Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void MethodSummary::addIfEq(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchFolding(TR::VPIntConst::create(_vp, 0), paramPosition, BranchFolding::Kinds::IfEq);
   BranchFolding* p2 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfEq);
   BranchFolding* p3 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfEq);

   add(p1);
   add(p2);
   add(p3);
   }

void MethodSummary::addIfNe(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchFolding(TR::VPIntConst::create(_vp, 0), paramPosition, BranchFolding::Kinds::IfNe);
   BranchFolding* p2 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfNe);
   BranchFolding* p3 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfNe);

   add(p1);
   add(p2);
   add(p3);
   }

void MethodSummary::addIfLt(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfLt);
   BranchFolding* p2 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, 0, INT_MAX), paramPosition,  BranchFolding::Kinds::IfLt);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfLe(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, 0), paramPosition, BranchFolding::Kinds::IfLe);
   BranchFolding* p2 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfLe);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfGt(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, 0), paramPosition, BranchFolding::Kinds::IfGt);
   BranchFolding* p2 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfGt);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfGe(int paramPosition)
   {
   BranchFolding* p1 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfGe);
   BranchFolding* p2 = new (_region) BranchFolding(TR::VPIntRange::create(_vp, 0, INT_MAX), paramPosition, BranchFolding::Kinds::IfGe);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfNull(int paramPosition)
   {
   NullBranchFolding* p1 = new (_region) NullBranchFolding(TR::VPNullObject::create(_vp), paramPosition, BranchFolding::Kinds::IfNull);
   NullBranchFolding* p2 = new (_region) NullBranchFolding(TR::VPNonNullObject::create(_vp), paramPosition, BranchFolding::Kinds::IfNull);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfNonNull(int paramPosition)
   {
   NullBranchFolding* p1 = new (_region) NullBranchFolding(TR::VPNullObject::create(_vp), paramPosition, BranchFolding::Kinds::IfNonNull);
   NullBranchFolding* p2 = new (_region) NullBranchFolding(TR::VPNonNullObject::create(_vp), paramPosition, BranchFolding::Kinds::IfNonNull);

   add(p1);
   add(p2);
   }

void MethodSummary::addNullCheck(int paramPosition)
   {
   NullCheckFolding* p1 = new (_region) NullCheckFolding(TR::VPNullObject::create(_vp), paramPosition);
   NullCheckFolding* p2 = new (_region) NullCheckFolding(TR::VPNonNullObject::create(_vp), paramPosition);

   add(p1);
   add(p2);
   }

void MethodSummary::addInstanceOf(int paramPosition, TR::VPFixedClass* classType)
   {
   InstanceOfFolding* p1 = new (_region) InstanceOfFolding(classType, paramPosition);
   InstanceOfFolding* p2 = new (_region) InstanceOfFolding(TR::VPNullObject::create(_vp), paramPosition);

   add(p1);
   add(p2);
   }

void MethodSummary::addCheckCast(int paramPostion, TR::VPFixedClass* classType)
   {
   CheckCastFolding* p = new (_region) CheckCastFolding(classType, paramPostion);

   add(p);
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

