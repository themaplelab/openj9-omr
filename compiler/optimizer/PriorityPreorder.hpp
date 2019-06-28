#pragma once

#include <vector>
#include <queue>

#include "IDT.hpp"


#ifdef J9_PROJECT_SPECIFIC
#include "compile/Compilation.hpp"
#endif

class PriorityPreorder {
public:
#ifdef J9_PROJECT_SPECIFIC
  PriorityPreorder(IDT*, TR::Compilation*);
#else
  PriorityPreorder(IDT*);
#endif
  size_t size() const;
  IDT::Node* get(size_t);
private:
  IDT* _root;
  IDT::NodePtrVector _entries;
  IDT::NodePtrPriorityQueue _queue;
};
