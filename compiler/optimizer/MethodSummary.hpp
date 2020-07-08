#ifndef METHOD_SUMMARY_INCL
#define METHOD_SUMMARY_INCL

#include "compiler/optimizer/VPConstraint.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/optimizer/IDTNode.hpp"


class PotentialOptimization
   {
   public:
   PotentialOptimization(int bytecode_idx, AbsValue *constraint, int argPos) :
      _bytecode_idx(bytecode_idx),
      _constraint(constraint),
      _argPos(argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
   virtual int test(AbsValue *constraint, TR::ValuePropagation*);
   int _argPos;
   protected:
   int _bytecode_idx;
   AbsValue *_constraint;
   };

class BranchFolding : public PotentialOptimization
   {
   public:
   BranchFolding(int bytecode_idx, AbsValue *constraint, int argPos) :
      PotentialOptimization(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
   };

class BranchIfEqFolding : public BranchFolding
  {
  public:
   BranchIfEqFolding(int bytecode_idx, AbsValue *constraint, int argPos) :
      BranchFolding(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
  };

class BranchIfNeFolding : public BranchFolding
  {
  public:
   BranchIfNeFolding(int bytecode_idx, AbsValue *constraint, int argPos) :
      BranchFolding(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
  };

class BranchIfGtFolding : public BranchFolding
  {
  public:
   BranchIfGtFolding(int bytecode_idx, AbsValue *constraint, int argPos) :
      BranchFolding(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
  };

class BranchIfGeFolding : public BranchFolding
  {
  public:
   BranchIfGeFolding(int bytecode_idx, AbsValue *constraint, int argPos) :
      BranchFolding(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
  };

class BranchIfLtFolding : public BranchFolding
  {
  public:
   BranchIfLtFolding(int bytecode_idx, AbsValue *constraint, int argPos) :
      BranchFolding(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
  };

class BranchIfLeFolding : public BranchFolding
  {
  public:
   BranchIfLeFolding(int bytecode_idx, AbsValue *constraint, int argPos) :
      BranchFolding(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
  };

class NullCheckFolding : public PotentialOptimization
   {
   public:
   NullCheckFolding(int bytecode_idx, AbsValue *constraint, int argPos) :
      PotentialOptimization(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual int test(AbsValue *constraint, TR::ValuePropagation*);
   virtual void trace(TR::ValuePropagation *vp); 
   };

class InstanceOfFolding : public PotentialOptimization
   {
   public:
   InstanceOfFolding(int bytecode_idx, AbsValue *constraint, int argPos):
      PotentialOptimization(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
   };

class CheckCastFolding : public PotentialOptimization
   {
   public:
   CheckCastFolding(int bytecode_idx, AbsValue *constraint, int argPos):
      PotentialOptimization(bytecode_idx, constraint, argPos)
   {};
   static const char *name;
   virtual void trace(TR::ValuePropagation *vp); 
   };

class MethodSummary
   {
public:
   int predicate(AbsValue*, int);
   MethodSummary(TR::Region &, TR::ValuePropagation *);
   void addIfeq(int, int);
   void addIfne(int, int);
   void addIfgt(int, int);
   void addIfge(int, int);
   void addIfle(int, int);
   void addIflt(int, int);
   void addNullCheckFolding(int, AbsValue*, int);
   void addBranchFolding(int, AbsValue*, int);
   void addBranchIfNeFolding(int, AbsValue*, int);
   void addBranchIfEqFolding(int, AbsValue*, int);
   void addBranchIfGtFolding(int, AbsValue*, int);
   void addBranchIfGeFolding(int, AbsValue*, int);
   void addBranchIfLeFolding(int, AbsValue*, int);
   void addBranchIfLtFolding(int, AbsValue*, int);
   void addInstanceOfFolding(int, AbsValue*, int);
   void addCheckCastFolding(int, AbsValue*, int);
   void trace();
private:
   void add(PotentialOptimization*);
   List<PotentialOptimization> _potentialOpts;
   TR::Region &_region;
   TR::ValuePropagation *_vp;
   };



#endif