#pragma once
#ifndef INLINER_PACKING_INCL
#define INLINER_PACKING_INCL

#include "compiler/optimizer/IDT.hpp"
#include "compiler/optimizer/InliningProposal.hpp"
#include "compiler/optimizer/PriorityPreorder.hpp"
#include "compiler/optimizer/Growable_2d_array.hpp"

namespace OMR {class BenefitInliner; }

#ifdef J9_PROJECT_SPECIFIC

#include "compile/Compilation.hpp"
#include "optimizer/Inliner.hpp"

#endif

InliningProposal* 
forwards_BitVectorImpl(int, PriorityPreorder&, Growable_2d_array_BitVectorImpl*, TR::Compilation *, OMR::BenefitInliner*, IDT*);

#endif
