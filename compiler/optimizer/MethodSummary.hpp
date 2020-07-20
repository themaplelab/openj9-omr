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
   
   virtual void trace(TR::ValuePropagation *vp)=0; 
   virtual int predicate(TR::VPConstraint *other, TR::ValuePropagation* vp) { return 0; };

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

   virtual int predicate(TR::VPConstraint *other, TR::ValuePropagation* vp);
   virtual void trace(TR::ValuePropagation *vp); 

   private:
   Kinds _kind;

   };

class NullBranchFolding : public BranchFolding
   {
   public:
   NullBranchFolding(TR::VPConstraint* constraint, int paramPosition, Kinds kind):
      BranchFolding(constraint, paramPosition, kind)
   {};

   virtual int predicate(TR::VPConstraint* other, TR::ValuePropagation* vp);
   };

class NullCheckFolding : public PotentialOptimization
   {
   public:
   NullCheckFolding(TR::VPConstraint* constraint, int paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {};

   virtual int predicate(TR::VPConstraint *other, TR::ValuePropagation* vp);
   virtual void trace(TR::ValuePropagation *vp);

   };

class InstanceOfFolding : public PotentialOptimization
   {
   public:
   InstanceOfFolding(TR::VPConstraint* constraint, int paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {};

   virtual int predicate(TR::VPConstraint* other, TR::ValuePropagation* vp);
   virtual void trace(TR::ValuePropagation* vp);
   };

class CheckCastFolding : public PotentialOptimization
   {
   public:
   CheckCastFolding(TR::VPConstraint* constraint, int paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {};

   virtual int predicate(TR::VPConstraint* other, TR::ValuePropagation* vp);
   virtual void trace(TR::ValuePropagation* vp);
   };

//The followings are to be completed in the future.

// class NullCheckFolding : public PotentialOptimization
//    {
//    public:
//    NullCheckFolding(TR::VPConstraint *constraint, int paramPosition) :
//       PotentialOptimization(constraint, paramPosition)
//    {};

//    virtual int test(TR::VPConstraint *constraint, TR::ValuePropagation* vp);
//    virtual void trace(TR::ValuePropagation *vp); 
//    };

// class InstanceOfFolding : public PotentialOptimization
//    {
//    public:
//    InstanceOfFolding(TR::VPConstraint *constraint, int paramPosition):
//       PotentialOptimization(constraint, paramPosition)
//    {};

//    virtual void trace(TR::ValuePropagation *vp); 
//    };

// class CheckCastFolding : public PotentialOptimization
//    {
//    public:
//    CheckCastFolding(TR::VPConstraint *constraint, int paramPosition):
//       PotentialOptimization(constraint, paramPosition)
//    {};

//    virtual void trace(TR::ValuePropagation *vp); 
//    };

class MethodSummary
   {
   public:
   int predicates(TR::VPConstraint* constraint, int paramPosition);

   MethodSummary(TR::Region& region, TR::ValuePropagation* vp) :
      _region(region),
      _potentialOpts(region),
      _vp(vp)
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
   List<PotentialOptimization> _potentialOpts;
   TR::Region &_region;
   TR::ValuePropagation *_vp;
   };



#endif