#pragma once

#include <vector>
#include <queue>
#include "IDT.hpp"
#include "compile/Compilation.hpp"

class PriorityPreorder {
public:
  PriorityPreorder(IDT*, TR::Compilation*);
  size_t size() const;
  IDTNode* get(size_t);
private:
  IDT* _root;
  IDTNodePtrVector _entries;
  IDTNodePtrPriorityQueue _queue;
};
