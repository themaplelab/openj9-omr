#pragma once

#include "compiler/optimizer/VPConstraint.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/optimizer/AbsVarArray.hpp"

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

class MethodSummaryExtension
   {
public:
   int predicate(AbsValue*, int);
   MethodSummaryExtension(TR::Region &, TR::ValuePropagation *);
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



class MethodSummaryRow {
public:
  MethodSummaryRow(TR::Compilation *comp, TR::Region &region);
  TR::VPConstraint *at(int n);
  void at(int n, TR::VPConstraint* constraint);
  void benefit(int n) { this->_benefit = n;}
  int benefit() { return this->_benefit;}
  enum PotentialTransform {UNK = 0, IFLT, IFLE, IFEQ, IFNE, IFGE, IFGT, INOF, IFNU, IFNN, NLCK, SIZE, STR_LEN, CHECK_CAST };
  void transform(MethodSummaryRow::PotentialTransform pt);
  MethodSummaryRow::PotentialTransform transform();
private:
  int _benefit;
  AbsVarArray *_row;
  TR::Region &_region;
  TR::Compilation *_comp;
  MethodSummaryRow::PotentialTransform _pt;
public:
  int _caller_idx;
  int _callee_idx;
  int _bytecode_idx;
  int caller_idx() { return this->_caller_idx; }
  int callee_idx() { return this->_callee_idx; }
  int bc_idx() { return this->_bytecode_idx; }
};

class MethodSummary {
public:
  MethodSummary(TR::Compilation *comp, TR::Region &region, TR::ValuePropagation*, IDT::Node*);
  int getIndex(void);
  //TODO: Rename all of these. These are not new anymore...
  MethodSummaryRow* newCHECK_CAST(IDT::Node*, int);
  MethodSummaryRow* newIFLT(IDT::Node*, int);
  MethodSummaryRow* newIFLE(IDT::Node*, int);
  MethodSummaryRow* newIFEQ(IDT::Node*, int);
  MethodSummaryRow* newIFNE(IDT::Node*, int);
  MethodSummaryRow* newIFGE(IDT::Node*, int);
  MethodSummaryRow* newIFGT(IDT::Node*, int);
  MethodSummaryRow* newINOF(IDT::Node*, int);
  MethodSummaryRow* newIFNU(IDT::Node*, int);
  MethodSummaryRow* newIFNN(IDT::Node*, int);
  MethodSummaryRow* newNLCK(IDT::Node*, int);
  MethodSummaryRow* newSIZE(IDT::Node*, int);
  MethodSummaryRow* newSTR_LEN(IDT::Node*, int);
  MethodSummaryRow* getRowSummary(void);
  MethodSummaryRow* getRowSummary(MethodSummaryRow::PotentialTransform, IDT::Node*, int);
  TR::VPConstraint *getClassConstraint(TR_OpaqueClassBlock *implicitParameterClass);
  int compareInformationAtCallSite(TR_ResolvedMethod *resolvedMethod, AbsVarArray *argumentConstraints);
  int apply(AbsVarArray* argumentConstraints);
  int applyVerbose(AbsVarArray* argumentConstraints, int);
  int applyVerbose(AbsVarArray* argumentConstraints, IDT::Node*);
  char* toString();
  void printMethodSummary(int);
private:
  IDT::Node *_hunk;
  TR::ValuePropagation *_vp;
  TR::Region &_region;
  TR::Compilation *_comp;
  List<MethodSummaryRow> _methodSummaryNew;
  int _index;
};

