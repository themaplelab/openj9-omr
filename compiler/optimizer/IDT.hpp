#pragma once

#include "Inliner.hpp"
#include "env/Region.hpp"
#include "env/TypedAllocator.hpp"
#include "infra/Cfg.hpp"
#include "infra/deque.hpp"
#include "infra/Stack.hpp"

/**
 * Class IDT 
 * =========
 * 
 * The inlining dependency tree maintains a tree of methods and can be used by an inlining optimization
 * The tree is designed to be built in a DFS-manner and maintains a current child which is updated when 
 * New children are added via addToCurrentChild. popCurrent will rever to the previous child.
 */

class IDT
  {
  public:
  class Node;
  typedef TR::deque<Node *, TR::Region&> Indices;
  class Node
    {
    public:
    Node(const IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, Node *parent);
    Node(const IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, Node *parent, unsigned int benefit);
    int size() const;
    Node* addChildIfNotExists(IDT* idt,
                                 int32_t callsite_bci,
                                 TR::ResolvedMethodSymbol* rms,
                                 int benefit);
    const char* getName(const IDT* idt) const;
    void printNodeThenChildren(const IDT* idt, int callerIndex) const;
    Node *getParent();
    int getCalleeIndex() const;
    unsigned int getCost() const;
    unsigned int getBenefit() const;
    void buildIndices(IDT::Indices &indices);
    private:
    typedef TR::deque<Node, TR::Region&> Children;
    Node *_parent;
    int _idx;
    int _callsite_bci;
    // NULL if 0, (Node* & 1) if 1, otherwise a deque*
    Children* _children;
    unsigned int _benefit;
    TR::ResolvedMethodSymbol* _rms;
    bool nodeSimilar(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms) const;
    uint32_t getBcSz() const;
    // Returns NULL if 0 or > 1 children
    Node* getOnlyChild() const;
    void setOnlyChild(Node* child);
    };
  private:
  TR::Region* _mem;
  TR_InlinerBase* _inliner;
  int _max_idx = -1;
  Node *_root;
  Node *_current;
  Indices *_indices = nullptr;
  void buildIndices();
  typedef TR::deque<Node, TR::Region&> BuildStack;
  int nextIdx();

public:
  IDT(TR_InlinerBase* inliner, TR::Region* mem, TR::ResolvedMethodSymbol* rms);
  Node* getRoot() const;
  TR::Compilation* comp() const;
  bool addToCurrentChild(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, float callRatio);
  void popCurrent();

public:
  int size() const;
  void printTrace() const;
  IDT::Node *getNodeByCalleeIndex(int calleeIndex);
};
