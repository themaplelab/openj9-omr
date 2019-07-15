#pragma once

#include <vector>
#include <queue>
#include "IDT.hpp"
#include "compile/Compilation.hpp"

class PriorityPreorder {
public:
  PriorityPreorder(IDT*, TR::Compilation*);
  size_t size() const;
  IDT::Node* get(size_t);
private:
  IDT* _root;
  IDT::NodePtrVector _entries;
  IDT::NodePtrPriorityQueue _queue;
};
