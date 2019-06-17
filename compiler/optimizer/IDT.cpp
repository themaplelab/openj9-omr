#include "IDT.hpp"

#define SINGLE_CHILD_BIT 1

IDTNode::IDTNode() {}

IDTNode::IDTNode(IDT* idt, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms)
  : _idx(idt->nextIdx())
  , _callsite_bci(callsite_bci)
  , _children(nullptr)
  , _rms(rms)
{}


IDT::IDT(TR_InlinerBase* inliner, TR::Region *mem, TR::ResolvedMethodSymbol* rms)
  : _mem(mem)
  , _inliner(inliner)
  , _max_idx(-1)
  , _root(new (*_mem) IDTNode(this, -1, rms))
{}

void
IDT::printTrace()
{
  if (comp()->trace(OMR::selectInliner)) {
    TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, "#IDT: %d candidate methods to inline into %s",
      size() - 1,
      getName(getRoot())
    );
    if (size() == 0) {
      return;
    }
    getRoot()->printNodeThenChildren(this, -2);
  }
}

void
IDTNode::printNodeThenChildren(IDT* idt, int callerIndex)
  {
  if (this != idt->getRoot()) {
    const char *nodeName = idt->getName(this);
    TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, "#IDT: %d: %d inlinable @%d -> bcsz=%d %s", 
      _idx,
      callerIndex,
      _callsite_bci,
      getBcSz(),
      nodeName
    );
  }
  
  if (_children == NULL) {
    return;
  }

  // Print children
  IDTNode* child = getOnlyChild();

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
IDTNode::getBcSz() {
  return _rms->getResolvedMethod()->maxBytecodeIndex();
}

IDTNode*
IDTNode::getOnlyChild()
  {
  if (((uintptr_t)_children) & SINGLE_CHILD_BIT)
    {
    return (IDTNode *)((uintptr_t)(_children) & ~SINGLE_CHILD_BIT);
    }
  return nullptr;
  }

void
IDTNode::setOnlyChild(IDTNode* child)
  {
  TR_ASSERT_FATAL(!((uintptr_t)child & SINGLE_CHILD_BIT), "Maligned memory address.\n");
  _children = (IDTNode::Children*)((uintptr_t)child | SINGLE_CHILD_BIT);
  }

IDTNode*
IDT::getRoot()
  {
  return _root;
  }

int
IDT::nextIdx()
  {
  return _max_idx++;
  }

IDTNode*
IDTNode::addChildIfNotExists(IDT* idt,
                         int32_t callsite_bci,
                         TR::ResolvedMethodSymbol* rms)
  {
  // 0 Children
  if (_children == nullptr)
    {
    IDTNode* created = new (*idt->_mem) IDTNode(idt, callsite_bci, rms);
    setOnlyChild(created);
    return created;
    }
  // 1 Child
  IDTNode* onlyChild = getOnlyChild();
  if (onlyChild != nullptr)
    {
    _children = new (*idt->_mem) IDTNode::Children(*idt->_mem);
    TR_ASSERT_FATAL(!((uintptr_t)_children & SINGLE_CHILD_BIT), "Maligned memory address.\n");
    _children->push_back(*onlyChild);
    // Fall through to two child case.
    }

  // 2+ children
  IDTNode::Children* children = _children;

  for (auto curr = children->begin(); curr != children->end(); ++curr)
    {
    if (curr->nodeSimilar(callsite_bci, rms))
      {
      return nullptr;
      }
    }

  _children->push_back(IDTNode(idt, callsite_bci, rms));
  return &_children->back();
}

bool
IDTNode::nodeSimilar(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms) const
  {
  auto a = _rms->getResolvedMethod()->maxBytecodeIndex();
  auto b = rms->getResolvedMethod()->maxBytecodeIndex();

  return a == b && _callsite_bci == callsite_bci;
  }

TR::Compilation*
IDT::comp()
  {
  return _inliner->comp();
  }


int
IDT::size()
  {
    return size(getRoot());
  }

int
IDT::size(IDTNode *node)
  {
    if (node->_children == NULL) 
      {
      return 1;
      }
    IDTNode *onlyChild = node->getOnlyChild();
    if (onlyChild != NULL)
      {
      return 1 + size(onlyChild);
      }
    int count = 1;
    for (auto curr = node->_children->begin(); curr != node->_children->end(); ++curr)
      {
      count += size(&*curr);
      }
    return count;
  }

size_t
IDTNode::numChildren()
  {
    if (_children == NULL) {
      return 0;
    }
    if (getOnlyChild() != nullptr) {
      return 1;
    }
    return _children->size();
  }

const char *
IDT::getName(IDTNode *node) {
  return node->_rms->getResolvedMethod()->signature(comp()->trMemory());
}
