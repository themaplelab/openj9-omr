#include <vector>
#include <queue>

#include "compiler/optimizer/PriorityPreorder.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/optimizer/IDTNode.hpp"
#include "compiler/infra/Assert.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/Inliner.hpp"


PriorityPreorder::PriorityPreorder(IDT* root, TR::Compilation* comp) : 
    _entries(comp->region()),
    _root(root),
    _queue(IDTNodePtrOrder(), IDTNodePtrVector(comp->region()))
{
  TR_ASSERT_FATAL(root->getRoot(), "root is null");
  this->_queue.push(root->getRoot());
}


size_t
PriorityPreorder::size() const {
  auto root = this->_root;
  return root->getNumNodes();
}

IDTNode*
PriorityPreorder::get(size_t idx) {
  auto size = this->_entries.size();
  if (size > idx) {
     return this->_entries.at(idx);
  }

  int totalSize = this->size();
  if (idx > totalSize - 1) {
    return NULL;
  }

  auto required_len = idx + 1;
  auto shortfall = required_len - size;
  for (size_t i = 0; i < shortfall; i++) {
    IDTNode* new_entry = _queue.top();
    TR_ASSERT(new_entry, "new entry can't be null");
    _queue.pop();
    this->_entries.push_back(new_entry);
    new_entry->enqueueSubordinates( &this->_queue );
  }
  return this->_entries.at(idx);
}
