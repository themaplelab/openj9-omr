#pragma once
#ifndef _IDTConstructor_
#define _IDTConstructor_

#include "compiler/optimizer/AbsEnvStatic.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/optimizer/IDTNode.hpp"
#include "compiler/optimizer/BenefitInliner.hpp"
#include "compiler/optimizer/MethodSummary.hpp"

class AbsFrameIDTConstructor;

class IDTConstructor : public AbsEnvStatic 
{
public:
  IDTConstructor(TR::Region &region, IDTNode *node, AbsFrame*);
  IDTConstructor(IDTConstructor&);
  IDTConstructor(AbsEnvStatic&);
  static AbsEnvStatic *enterMethod(TR::Region&region, IDTNode* node, AbsFrame* absFrame, TR::ResolvedMethodSymbol*);
  TR::Region& getCallSitesRegion();
  virtual bool loadFromIDT() { return false; }
protected:
  virtual AbsState& invokevirtual(AbsState&, int, int);
  virtual AbsState& invokespecial(AbsState&, int, int);
  virtual AbsState& invokestatic(AbsState&, int, int);
  virtual AbsState& invokeinterface(AbsState&, int, int);
  //virtual AbsState& invokeinterface2(AbsState&, int, int);
  virtual AbsState& ifeq(AbsState&, int, int);
  virtual AbsState& ifne(AbsState&, int, int);
  virtual AbsState& ifge(AbsState&, int, int);
  virtual AbsState& ifgt(AbsState&, int, int);
  virtual AbsState& iflt(AbsState&, int, int);
  virtual AbsState& ifle(AbsState&, int, int);
  virtual AbsState& ifnonnull(AbsState&, int, int);
  virtual AbsState& ifnull(AbsState&, int, int);
  virtual AbsState& instanceof(AbsState&, int, int);
private:
  void addInstanceof(int, int, AbsValue *);
  void addIfeq(int, int);
  void addIflt(int, int);
  void addIfne(int, int);
  void addIfgt(int, int);
  void addIfge(int, int);
  void addIfle(int, int);
  void addIfNonNull(int, int, AbsValue *absValue);
  void addIfNull(int, int, AbsValue *absValue);
  IDTNodeIndices *getDeque();
  TR_CallSite* findCallSiteTargets(TR::ResolvedMethodSymbol *callerSymbol, int, int, TR::MethodSymbol::Kinds, OMR::Block *block, TR::CFG* cfg = NULL);
  TR::SymbolReference* getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind);
  int getCallerIndex() const;
  TR_CallStack *getCallStack();
  OMR::BenefitInliner* inliner();
   TR_CallSite * getCallSite(TR::MethodSymbol::Kinds kind,
                                    TR_ResolvedMethod *callerResolvedMethod,
                                    TR::TreeTop *callNodeTreeTop,
                                    TR::Node *parent,
                                    TR::Node *callNode,
                                    TR::Method * interfaceMethod,
                                    TR_OpaqueClassBlock *receiverClass,
                                    int32_t vftSlot,
                                    int32_t cpIndex,
                                    TR_ResolvedMethod *initialCalleeMethod,
                                    TR::ResolvedMethodSymbol * initialCalleeSymbol,
                                    bool isIndirectCall,
                                    bool isInterface,
                                    TR_ByteCodeInfo & bcInfo,
                                    TR::Compilation *comp,
                                    int32_t depth=-1,
                                    bool allConsts=false,
                                    TR::SymbolReference *symRef=nullptr);
};

class AbsFrameIDTConstructor : public AbsFrame
{
public:
  AbsFrameIDTConstructor(TR::Region &region, IDTNode *node, int callerIndex, TR_CallStack* callStack, OMR::BenefitInliner* inliner);
  virtual void interpret();
  TR::Region &getCallSitesRegion();
  TR_CallStack* getCallStack();
  IDTNode *getNode();
  MethodSummary *_summary;
  void traceMethodSummary();
protected:
  OMR::BenefitInliner* _inliner;
  TR::Region &_callSitesRegion;
  TR_CallStack *_inliningCallStack;
  int _callerIndex;
  virtual void factFlow(OMR::Block *);
  virtual AbsEnvStatic *mergeAllPredecessors(OMR::Block *);
  IDTNodeIndices *_deque;
public:
  void setDeque(IDTNodeIndices * deque) { this->_deque=deque; };
  IDTNodeIndices *getDeque() { return this->_deque; };
   int getCallerIndex() const;
  OMR::BenefitInliner* inliner();
};

#endif
