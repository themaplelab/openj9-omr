#include <iostream>
#include <vector>
#include <queue>

#include "compiler/optimizer/PriorityPreorder.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/infra/Assert.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "compile/Compilation.hpp"
#include "optimizer/Inliner.hpp"
#endif


PriorityPreorder::PriorityPreorder(IDT* root, TR::Compilation* comp) : 
    _entries(comp->region()),
    _root(root),
    _queue(IDT::NodePtrOrder(), IDT::NodePtrVector(comp->region()))
{
  TR_ASSERT_FATAL(root->getRoot(), "root is null");
  this->_queue.push(root->getRoot());
}


size_t
PriorityPreorder::size() const {
  auto root = this->_root;
  return root->size();
}

IDT::Node*
PriorityPreorder::get(size_t idx) {
  auto size = this->_entries.size();
  // starting at array index = 1
  // we want index = 2
  // we have index = 1
  if (size > idx) {
     return this->_entries.at(idx);
  }

  // we have idx = 2
  int totalSize = this->size();
  if (idx > totalSize - 1) {
    return NULL;
  }

  auto required_len = idx + 1;
  auto shortfall = required_len - size;
  for (size_t i = 0; i < shortfall; i++) {
    IDT::Node* new_entry = _queue.top();
    _queue.pop();
    this->_entries.push_back(new_entry);
    new_entry->enqueue_subordinates( &this->_queue );
  }
  return this->_entries.at(idx);
}
