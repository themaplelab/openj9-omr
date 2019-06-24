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
  private:
  class Node
    {
    public:
    Node(const IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, Node *parent);
    int size() const;
    Node* addChildIfNotExists(IDT* idt,
                                 int32_t callsite_bci,
                                 TR::ResolvedMethodSymbol* rms);
    const char* getName(const IDT* idt) const;
    void printNodeThenChildren(const IDT* idt, int callerIndex) const;
    Node *getParent();
    private:
    typedef TR::deque<Node, TR::Region&> Children;
    Node *_parent;
    int _idx;
    int _callsite_bci;
    // NULL if 0, (Node* & 1) if 1, otherwise a deque*
    Children* _children;
    TR::ResolvedMethodSymbol* _rms;
    bool nodeSimilar(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms) const;
    uint32_t getBcSz() const;
    // Returns NULL if 0 or > 1 children
    Node* getOnlyChild() const;
    void setOnlyChild(Node* child);
    };
  TR::Region* _mem;
  TR_InlinerBase* _inliner;
  int _max_idx = -1;
  Node *_root;
  Node *_current;
  typedef TR::deque<Node, TR::Region&> BuildStack;
  int nextIdx();

public:
  IDT(TR_InlinerBase* inliner, TR::Region* mem, TR::ResolvedMethodSymbol* rms);
  Node* getRoot() const;
  TR::Compilation* comp() const;
  bool addToCurrentChild(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms);
  void popCurrent();

public:
  int size() const;
  void printTrace() const;
};
