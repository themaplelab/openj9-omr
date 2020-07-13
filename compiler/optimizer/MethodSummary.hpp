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
   virtual int predicate(TR::VPConstraint *constraint, TR::ValuePropagation* vp);

   TR::VPConstraint* getConstraint()  { return _constraint; } 
   int getParamPosition() { return _paramPosition; }

   protected:
   TR::VPConstraint *_constraint;
   int _paramPosition;
   };

class BranchFolding : public PotentialOptimization
   {
   public:
   BranchFolding(TR::VPConstraint *constraint, int paramPosition) :
      PotentialOptimization(constraint, paramPosition)
   {};
   virtual void trace(TR::ValuePropagation *vp); 
   };

class BranchIfEqFolding : public BranchFolding
   {
   public:
   BranchIfEqFolding(TR::VPConstraint *constraint, int paramPosition) :
      BranchFolding(constraint, paramPosition)
   {};

   virtual void trace(TR::ValuePropagation *vp); 
   };

class BranchIfNeFolding : public BranchFolding
   {
   public:
   BranchIfNeFolding(TR::VPConstraint *constraint, int paramPosition) :
      BranchFolding(constraint, paramPosition)
   {};

   virtual void trace(TR::ValuePropagation *vp); 
   };

class BranchIfGtFolding : public BranchFolding
   {
   public:
   BranchIfGtFolding(TR::VPConstraint *constraint, int paramPosition) :
      BranchFolding(constraint, paramPosition)
   {};

   virtual void trace(TR::ValuePropagation *vp); 
   };

class BranchIfGeFolding : public BranchFolding
  {
  public:
   BranchIfGeFolding(TR::VPConstraint *constraint, int paramPosition) :
      BranchFolding(constraint, paramPosition)
   {};

   virtual void trace(TR::ValuePropagation *vp); 
  };

class BranchIfLtFolding : public BranchFolding
   {
   public:
   BranchIfLtFolding(TR::VPConstraint *constraint, int paramPosition) :
      BranchFolding(constraint, paramPosition)
   {};

   virtual void trace(TR::ValuePropagation *vp); 
   };

class BranchIfLeFolding : public BranchFolding
   {
   public:
   BranchIfLeFolding(TR::VPConstraint *constraint, int paramPosition) :
      BranchFolding(constraint, paramPosition)
   {};

   virtual void trace(TR::ValuePropagation *vp); 
   };

// class BranchIfNullFolding : public BranchFolding
//    {
//    public:
//    BranchIfNullFolding(TR::VPConstraint* constraint, int paramPosition) :
//       BranchFolding(constraint, paramPosition)
//    {};

//    static const char* name;
//    virtual void trace(TR::ValuePropagation *vp); 
//    }

// class BranchIfNonNullFolding : public BranchFolding
//    {
//    public:
//    BranchIfNonNullFolding(TR::VPConstraint* constraint, int paramPosition) :
//       BranchFolding(constraint, paramPosition)
//    {};

//    static const char* name;
//    virtual void trace(TR::ValuePropagation *vp); 
//    }

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
   MethodSummary(TR::Region&, TR::ValuePropagation* vp);
   void addIfEq(int paramPosition);
   void addIfNe(int paramPosition);
   void addIfGt(int paramPosition);
   void addIfGe(int paramPosition);
   void addIfLe(int paramPosition);
   void addIfLt(int paramPosition);


   // To be implemented
   // void addIfNull(int paramPosition);
   // void addIfNonNull(int paramPosition);

   void trace();
   private:
   void add(PotentialOptimization*);
   List<PotentialOptimization> _potentialOpts;
   TR::Region &_region;
   TR::ValuePropagation *_vp;
   };



#endif