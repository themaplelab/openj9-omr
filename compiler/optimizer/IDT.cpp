#include "IDT.hpp"

#define SINGLE_CHILD_BIT 1


IDT::Node::Node(const IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, IDT::Node *parent):
  _idx(idx),
  _benefit(-1),
  _callsite_bci(callsite_bci),
  _children(nullptr),
  _rms(rms),
  _parent(parent)
  {}

IDT::Node::Node(const IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, IDT::Node *parent, unsigned int benefit):
  _idx(idx),
  _benefit(benefit),
  _callsite_bci(callsite_bci),
  _children(nullptr),
  _rms(rms),
  _parent(parent)
  {}

IDT::IDT(TR_InlinerBase* inliner, TR::Region *mem, TR::ResolvedMethodSymbol* rms):
  _mem(mem),
  _inliner(inliner),
  _max_idx(-1),
  _root(new (*_mem) IDT::Node(this, nextIdx(), -1, rms, nullptr, 1)),
  _current(_root)
  {}

void
IDT::printTrace() const
  {
  if (comp()->trace(OMR::benefitInliner)) {
    const int candidates = size() - 1;
    TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, "#IDT: %d candidate methods to inline into %s",
      candidates,
      getRoot()->getName(this)
    );
    if (candidates <= 0) {
      return;
    }
    getRoot()->printNodeThenChildren(this, -2);
  }
  }

void
IDT::Node::printNodeThenChildren(const IDT* idt, int callerIndex) const
  {
  if (this != idt->getRoot()) {
    const char *nodeName = getName(idt);
    TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, "#IDT: %d: %d inlinable @%d -> bcsz=%d %s, benefit = %u", 
      _idx,
      callerIndex,
      _callsite_bci,
      getBcSz(),
      nodeName,
      this->getBenefit()
    );
  }
  
  if (_children == nullptr) {
    return;
  }

  // Print children
  IDT::Node* child = getOnlyChild();

  if (child != nullptr)
    {
    child->printNodeThenChildren(idt, _idx);
    } 
  else
    {
    for (auto curr = _children->begin(); curr != _children->end(); ++curr)
      {
      curr->printNodeThenChildren(idt, _idx);
      }
    }
  }

uint32_t
IDT::Node::getBcSz() const 
  {
  return _rms->getResolvedMethod()->maxBytecodeIndex();
  }

IDT::Node*
IDT::Node::getOnlyChild() const
  {
  if (((uintptr_t)_children) & SINGLE_CHILD_BIT)
    {
    return (IDT::Node *)((uintptr_t)(_children) & ~SINGLE_CHILD_BIT);
    }
  return nullptr;
  }

void
IDT::Node::setOnlyChild(IDT::Node* child)
  {
  TR_ASSERT_FATAL(!((uintptr_t)child & SINGLE_CHILD_BIT), "Maligned memory address.\n");
  _children = (IDT::Node::Children*)((uintptr_t)child | SINGLE_CHILD_BIT);
  }

IDT::Node*
IDT::getRoot() const
  {
  return _root;
  }


void
IDT::buildIndices() 
  {
    _indices = new (*_mem) IDT::Indices(size(), nullptr, *_mem);
    getRoot()->buildIndices(*_indices);
  }

void
IDT::Node::buildIndices(IDT::Indices &indices)
  {
    TR_ASSERT(indices[getCalleeIndex()] == nullptr, "callee index not unique");
    indices[getCalleeIndex()] = this;
    for (auto curr = _children->begin(); curr != _children->end(); ++curr)
      {
      curr->buildIndices(indices);
      }
  }

int
IDT::nextIdx()
  {
  return _max_idx++;
  }


int
IDT::Node::getCalleeIndex() const
  {
    return _idx;
  }

unsigned int
IDT::Node::getCost() const
  {
    return this->getBcSz();
  }

unsigned int
IDT::Node::getBenefit() const
  {
    return this->_benefit;
  }

IDT::Node*
IDT::Node::addChildIfNotExists(IDT* idt,
                         int32_t callsite_bci,
                         TR::ResolvedMethodSymbol* rms,
                         int benefit)
  {
  // 0 Children
  if (_children == nullptr)
    {
    IDT::Node* created = new (*idt->_mem) IDT::Node(idt, idt->nextIdx(), callsite_bci, rms, this, benefit);
    setOnlyChild(created);
    return created;
    }
  // 1 Child
  IDT::Node* onlyChild = getOnlyChild();
  if (onlyChild != nullptr)
    {
    _children = new (*idt->_mem) IDT::Node::Children(*idt->_mem);
    TR_ASSERT_FATAL(!((uintptr_t)_children & SINGLE_CHILD_BIT), "Maligned memory address.\n");
    _children->push_back(*onlyChild);
    // Fall through to two child case.
    }

  // 2+ children
  //IDT::Node::Children* children = _children;

/*
  for (auto curr = _children->begin(); curr != _children->end(); ++curr)
    {
    if (curr->nodeSimilar(callsite_bci, rms))
      {
      return nullptr;
      }
    }
*/

  _children->push_back(IDT::Node(idt, idt->nextIdx(), callsite_bci, rms, this, benefit));
  return &_children->back();
  }

bool
IDT::Node::nodeSimilar(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms) const
  {
  auto a = _rms->getResolvedMethod()->maxBytecodeIndex();
  auto b = rms->getResolvedMethod()->maxBytecodeIndex();

  return a == b && _callsite_bci == callsite_bci;
  }

TR::Compilation*
IDT::comp() const
  {
  return _inliner->comp();
  }

int
IDT::size() const
  {
    return getRoot()->size();
  }

unsigned int
IDT::Node::getNumChildren() const
   {
   if (_children == nullptr) return 0;
   if (getOnlyChild() != nullptr) return 1;
   return _children->size();
   }

int
IDT::Node::size() const
  {
    if (_children == nullptr) {
      return 1;
    }
    Node *child = this->getOnlyChild();
    if (getOnlyChild() != nullptr) {
      return 1 + child->size();
    }
    int sum = 1;
    for (auto child = _children->begin(); child != _children->end(); ++child) {
      sum += child->size();
    }
    return sum;
  }

const char *
IDT::Node::getName(const IDT *idt) const 
  {
  return _rms->getResolvedMethod()->signature(idt->comp()->trMemory());
  }

IDT::Node *
IDT::Node::getParent()
  {
    return _parent;
  }

bool IDT::addToCurrentChild(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, float callRatio)
  {
    IDT::Node *node = _current->addChildIfNotExists(this, callsite_bci, rms, (int)(100 * callRatio));
    if (node == nullptr) {
      return false;
    }
    _current = node;
    _indices = nullptr;
    return true;
  }

void IDT::popCurrent()
  {
    _current = _current->getParent();
    TR_ASSERT_FATAL(_current != nullptr, "Invalid IDT pop");
  }

IDT::Node *IDT::getNodeByCalleeIndex(int calleeIndex)
  {
    if (!_indices) return NULL;
    return (*_indices)[calleeIndex];
  }

void
IDT::Node::enqueue_subordinates(IDT::NodePtrPriorityQueue *q) const
   {
      int count = this->getNumChildren();
      if (count == 1) {
         IDT::Node* child = this->getOnlyChild();
         q->push(child);
         return;
      }
      for (int i = 0; i < count; i++)
         {
            IDT::Node *child = &(_children->at(i));
            q->push(child);
         }
   }
