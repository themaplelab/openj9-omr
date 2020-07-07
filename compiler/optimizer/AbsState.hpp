#ifndef ABS_STATE_INCL
#define ABS_STATE_INCL

#include "env/Region.hpp"
#include "optimizer/AbsVarArrayStatic.hpp"
#include "optimizer/AbsOpStackStatic.hpp"
#include "optimizer/AbsValue.hpp"

class AbsState
    {
    public:
    AbsState(TR::Region &region, unsigned int maxLocalVarArrayDepth, unsigned int maxOpStackDepth);
    AbsState(AbsState* other);

    //For AbsArray
    void set(unsigned int, AbsValue*);
    AbsValue *at(unsigned int);

    //For AbsOpStack
    void push(AbsValue *);
    AbsValue* pop();
    AbsValue* top();
    void merge(AbsState*, TR::ValuePropagation*);
    size_t getStackSize();
    size_t getArraySize();
    void trace(TR::ValuePropagation*);

    private:
    TR::Region& _region;
    AbsVarArrayStatic _array;
    AbsOpStackStatic _stack;
    };


#endif