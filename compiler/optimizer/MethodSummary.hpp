#ifndef METHOD_SUMMARY_INCL
#define METHOD_SUMMARY_INCL

#include "optimizer/VPConstraint.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"


class PotentialOptimization
   {
   public:
   PotentialOptimization(TR::VPConstraint *constraint, int paramPosition) :
      _constraint(constraint),
      _paramPosition(paramPosition)
   {};
   
   virtual void trace(OMR::ValuePropagation *vp)=0; 
   virtual int predicate(TR::VPConstraint *other,OMR::ValuePropagation* vp) { return 0; };
   virtual int getWeight() { return 1; };

   TR::VPConstraint* getConstraint()  { return _constraint; } 
   int getParamPosition() { return _paramPosition; }

   protected:
   TR::VPConstraint *_constraint;
   int _paramPosition;
   };

class BranchFolding : public PotentialOptimization
   {
   public:
   enum Kinds
   {
   IfEq,
   IfNe,
   IfGt,
   IfGe,
   IfLt, 
   IfLe,
   IfNull,
   IfNonNull,
   };

   BranchFolding(TR::VPConstraint *constraint, int paramPosition, Kinds kind) :
      PotentialOptimization(constraint, paramPosition),
      _kind(kind)
   {};

   virtual int predicate(TR::VPConstraint *other,OMR::ValuePropagation* vp);
   virtual void trace(OMR::ValuePropagation *vp); 
   virtual int getWeight() { return 1; }

   private:
   Kinds _kind;

   };

class NullBranchFolding : public BranchFolding
   {
   public:
   NullBranchFolding(TR::VPConstraint* constraint, int paramPosition, Kinds kind):
      BranchFolding(constraint, paramPosition, kind)
   {};

   virtual int predicate(TR::VPConstraint* other,OMR::ValuePropagation* vp);
   virtual int getWeight() { return 1; }
   };

class NullCheckFolding : public PotentialOptimization
   {
   public:
   NullCheckFolding(TR::VPConstraint* constraint, int paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {};

   virtual int predicate(TR::VPConstraint *other,OMR::ValuePropagation* vp);
   virtual void trace(OMR::ValuePropagation *vp);
   virtual int getWeight() { return 1; }

   };

class InstanceOfFolding : public PotentialOptimization
   {
   public:
   InstanceOfFolding(TR::VPConstraint* constraint, int paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {};

   virtual int predicate(TR::VPConstraint* other,OMR::ValuePropagation* vp);
   virtual int getWeight() { return 1; }
   virtual void trace(OMR::ValuePropagation* vp);
   };

class CheckCastFolding : public PotentialOptimization
   {
   public:
   CheckCastFolding(TR::VPConstraint* constraint, int paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {};

   virtual int predicate(TR::VPConstraint* other,OMR::ValuePropagation* vp);
   virtual int getWeight() { return 1; }
   virtual void trace(OMR::ValuePropagation* vp);
   };


class MethodSummary
   {
   public:
   int predicates(TR::VPConstraint* constraint, int paramPosition);

   MethodSummary(TR::Region& region,OMR::ValuePropagation* vp, TR::Compilation* comp) :
      _region(region),
      _potentialOpts(region),
      _vp(vp),
      _comp(comp)
   {};

   void addIfEq(int paramPosition);
   void addIfNe(int paramPosition);
   void addIfGt(int paramPosition);
   void addIfGe(int paramPosition);
   void addIfLe(int paramPosition);
   void addIfLt(int paramPosition);
   void addIfNull(int paramPosition);
   void addIfNonNull(int paramPosition);

   void addNullCheck(int paramPosition);

   void addInstanceOf(int paramPosition, TR_OpaqueClassBlock* classBlock);

   void addCheckCast(int paraPostion, TR_OpaqueClassBlock* classBlock);
   
   void trace();
   private:
   void add(PotentialOptimization*);

   TR::Compilation* comp() { return _comp; }
   TR::Region& region() { return _region; }

   List<PotentialOptimization> _potentialOpts;
   TR::Region &_region;
   OMR::ValuePropagation *_vp;
   TR::Compilation* _comp;

   };



#endif