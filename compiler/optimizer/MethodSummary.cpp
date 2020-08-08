#include "optimizer/MethodSummary.hpp"
#include "optimizer/VPConstraint.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"


int BranchFolding::predicate(TR::VPConstraint *other, OMR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Contraint to Compare: ");

   if (!other)
      {
      traceMsg(TR::comp(), "TOP\n");
      return 0;
      }

   other->print(vp);
   traceMsg(TR::comp(), "\n");

   if (_constraint->getLowInt() <= other->getLowInt() && _constraint->getHighInt() >= other->getHighInt())
      return 1;
      
   return 0;
   }

int NullBranchFolding::predicate(TR::VPConstraint *other,OMR::ValuePropagation* vp)
   {
   if (!other)
      return 0;
   traceMsg(TR::comp(), "Contraint to Compare: ");
   other->print(vp);
   traceMsg(TR::comp(), "\n");

   if (other->isNullObject() && _constraint->isNullObject()) //both null
      return 1; 

   if (other->isNonNullObject() && _constraint->isNonNullObject() ) //both non-null
      return 1;

   return 0;
   }

int NullCheckFolding::predicate(TR::VPConstraint* other,OMR::ValuePropagation *vp)
   {
   if (!other)
      return 0;

   traceMsg(TR::comp(), "Contraint to Compare: ");
   other->print(vp);
   traceMsg(TR::comp(), "\n");

   if (other->isNullObject() && _constraint->isNullObject()) //both null
      return 1; 

   if (other->isNonNullObject() && _constraint->isNonNullObject() ) //both non-null
      return 1;
      
   return 0;
   }


//Note: other can be VPClass or VPNullObject
// constraint is VPFixedClass or VPNullObject
int InstanceOfFolding::predicate(TR::VPConstraint* other,OMR::ValuePropagation *vp)
   {
   if (!other)
      return 0;

   traceMsg(TR::comp(), "Contraint to Compare: ");
   other->print(vp);
   traceMsg(TR::comp(), "\n");

   //instance of null object
   if (_constraint->isNullObject() && other->isNullObject())
      return 1;
   
   //instance of non-null object. Have the exact same type
   if (other->isNonNullObject() && _constraint->getClass() != NULL && other->getClass() == _constraint->getClass())
      return 1;


   return 0;
   }

int CheckCastFolding::predicate(TR::VPConstraint* other,OMR::ValuePropagation *vp)
   {
   if (!other)
      return 0;

   traceMsg(TR::comp(), "Contraint to Compare: ");
   other->print(vp);
   traceMsg(TR::comp(), "\n");

   //Checkcast null object
   if (_constraint->isNullObject() && other->isNullObject())
      return 1;
   
   //Checkcast non-null object. Have the exact same type
   if (other->isNonNullObject() && _constraint->getClass() != NULL && other->getClass() == _constraint->getClass())
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
         i += succuess * opt->getWeight();
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

void BranchFolding::trace(OMR::ValuePropagation *vp)
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

void NullCheckFolding::trace(OMR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Null Check Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void InstanceOfFolding::trace(OMR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Instanceof Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void CheckCastFolding::trace(OMR::ValuePropagation *vp)
   {
   traceMsg(TR::comp(), "Checkcast Folding for argument %d constraint: ", _paramPosition);
   _constraint->print(vp);
   traceMsg(TR::comp(), "\n");
   }

void MethodSummary::addIfEq(int paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntConst::create(_vp, 0), paramPosition, BranchFolding::Kinds::IfEq);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfEq);
   BranchFolding* p3 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfEq);

   add(p1);
   add(p2);
   add(p3);
   }

void MethodSummary::addIfNe(int paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntConst::create(_vp, 0), paramPosition, BranchFolding::Kinds::IfNe);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfNe);
   BranchFolding* p3 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfNe);

   add(p1);
   add(p2);
   add(p3);
   }

void MethodSummary::addIfLt(int paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfLt);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, 0, INT_MAX), paramPosition,  BranchFolding::Kinds::IfLt);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfLe(int paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, 0), paramPosition, BranchFolding::Kinds::IfLe);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfLe);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfGt(int paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, 0), paramPosition, BranchFolding::Kinds::IfGt);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, 1, INT_MAX), paramPosition, BranchFolding::Kinds::IfGt);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfGe(int paramPosition)
   {
   BranchFolding* p1 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, INT_MIN, -1), paramPosition, BranchFolding::Kinds::IfGe);
   BranchFolding* p2 = new (region()) BranchFolding(TR::VPIntRange::create(_vp, 0, INT_MAX), paramPosition, BranchFolding::Kinds::IfGe);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfNull(int paramPosition)
   {
   NullBranchFolding* p1 = new (region()) NullBranchFolding(TR::VPNullObject::create(_vp), paramPosition, BranchFolding::Kinds::IfNull);
   NullBranchFolding* p2 = new (region()) NullBranchFolding(TR::VPNonNullObject::create(_vp), paramPosition, BranchFolding::Kinds::IfNull);

   add(p1);
   add(p2);
   }

void MethodSummary::addIfNonNull(int paramPosition)
   {
   NullBranchFolding* p1 = new (region()) NullBranchFolding(TR::VPNullObject::create(_vp), paramPosition, BranchFolding::Kinds::IfNonNull);
   NullBranchFolding* p2 = new (region()) NullBranchFolding(TR::VPNonNullObject::create(_vp), paramPosition, BranchFolding::Kinds::IfNonNull);

   add(p1);
   add(p2);
   }

void MethodSummary::addNullCheck(int paramPosition)
   {
   NullCheckFolding* p1 = new (region()) NullCheckFolding(TR::VPNullObject::create(_vp), paramPosition);
   NullCheckFolding* p2 = new (region()) NullCheckFolding(TR::VPNonNullObject::create(_vp), paramPosition);

   add(p1);
   add(p2);
   }

void MethodSummary::addInstanceOf(int paramPosition, TR_OpaqueClassBlock* classBlock)
   {
   if (classBlock == NULL)
      {
      InstanceOfFolding* p1 = new (region()) InstanceOfFolding(TR::VPNullObject::create(_vp), paramPosition);
      add(p1);
      }
   else
      {
      InstanceOfFolding* p1 = new (region()) InstanceOfFolding(TR::VPNullObject::create(_vp), paramPosition);
      InstanceOfFolding* p2 = new (region()) InstanceOfFolding(TR::VPFixedClass::create(_vp, classBlock), paramPosition);
      add(p1);
      add(p2);
      }
   }

void MethodSummary::addCheckCast(int paramPosition, TR_OpaqueClassBlock* classBlock)
   {
   if (classBlock == NULL)
      {
      CheckCastFolding* p1 = new (region()) CheckCastFolding(TR::VPNullObject::create(_vp), paramPosition);
      add(p1);
      }
   else
      {
      CheckCastFolding* p1 = new (region()) CheckCastFolding(TR::VPNullObject::create(_vp), paramPosition);
      CheckCastFolding* p2 = new (region()) CheckCastFolding(TR::VPFixedClass::create(_vp, classBlock), paramPosition);
      add(p1);
      add(p2);
      }
   }

void MethodSummary::add(PotentialOptimization *potentialOpt)
   {
   _potentialOpts.add(potentialOpt); 
   }

