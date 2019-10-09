#include "compiler/optimizer/InliningProposal.hpp"
#include "compiler/optimizer/Growable_2d_array.hpp"
#include "compiler/optimizer/BenefitInliner.hpp"
#include "compiler/optimizer/PriorityPreorder.hpp"

InliningProposal*
forwards_BitVectorImpl(const int cost_budget, PriorityPreorder& items, Growable_2d_array_BitVectorImpl* table, TR::Compilation* comp, OMR::BenefitInliner *inliner, IDT *idt)
{
  size_t max = items.size();
  InliningProposal new_proposal(inliner, idt, max);
  InliningProposal *old_proposal = NULL;
  InliningProposal common(inliner, idt, max);
  InliningProposal *prev_best = NULL;
  InliningProposal *temp = NULL;
  InliningProposal curr_set(inliner, idt, max);
  InliningProposal *retval = NULL;
  for(int row = 0; row < max; row++) {
    for(int budget = 1; budget < cost_budget + 1; budget++) {

      IDT::Node* curr_item = items.get(row);
      TR_ASSERT(curr_item->getCallTarget()->_calleeSymbol->getFlowGraph(), "we have a cfg");
      //TODO: encapsulate this in a clear method.
      curr_set.intersectInPlace(*table->_get(-1, -1), *table->_get(-1, -1));
      curr_set.pushBack(curr_item);
      int offset_row = row - 1;
      int offset_col = budget - curr_set.getCost();
      prev_best = table->_get(offset_row, offset_col);

      while( !curr_item->isRoot() && !prev_best->inSet(curr_item->getParent())) {

        curr_set.pushBack(curr_item->getParent());
        curr_item = curr_item->getParent();
        offset_col = budget - curr_set.getCost();
        prev_best = table->_get(offset_row, offset_col);

      }

      offset_col = budget - curr_set.getCost();
      //TODO: rename, maybe to tableElem or tableCell what is this other set that I'm pointing to.
      temp = table->_get(offset_row, offset_col);
      common.intersectInPlace(curr_set, *temp);
      bool prereqInTemp = curr_item->getParent() and temp->inSet(curr_item->getParent());

      while( !common.isEmpty() || !(prereqInTemp or temp->isEmpty())) {

        offset_row--;
        temp = table->_get(offset_row, offset_col);
        common.intersectInPlace(curr_set, *temp);
        prereqInTemp = curr_item->getParent() and temp->inSet(curr_item->getParent());

      }

      offset_col = budget - curr_set.getCost();
      //TODO: rename unionInPlace to just union
      // is the curr_set union table same as table? if so, we can avoid calling put
      auto prevBest = table->_get(offset_row, offset_col);
      bool prevBestOverlaps = prevBest->overlaps(&curr_set);
      old_proposal = table->_get(row - 1, budget);
      if (prevBestOverlaps) {
         bool isNewBetterThanOld = old_proposal->getBenefit() < prevBest->getBenefit();
         bool doesNewFitInBudget = prevBest->getCost() <= budget;
         bool storeNew = isNewBetterThanOld and doesNewFitInBudget;
         if (storeNew) {
            prevBest->print();
            table->_put(row, budget, prevBest);
         } else {
            old_proposal->print();
            table->_put(row, budget, old_proposal);
         }
      } else {
         new_proposal.unionInPlace(*prevBest, curr_set);
         bool isNewBetterThanOld = old_proposal->getBenefit() < new_proposal.getBenefit();
         //TODO: why is this boolean not in the original inliner paper
         bool doesNewFitInBudget = new_proposal.getCost() <= budget;
         bool storeNew = isNewBetterThanOld and doesNewFitInBudget;
         if (storeNew) { 
           new_proposal.print();
           table->put(row, budget, new_proposal);
         }
         else {
           old_proposal->print();
           table->_put(row, budget, old_proposal);
         }
      }
 
      retval = table->_get(row, budget);
      retval->print();

    }
  }
  return retval;
}
