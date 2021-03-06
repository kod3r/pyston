// Copyright (c) 2014 Dropbox, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "runtime/tuple.h"

#include <sstream>

#include "codegen/compvars.h"
#include "core/ast.h"
#include "core/common.h"
#include "core/stats.h"
#include "core/types.h"
#include "gc/collector.h"
#include "runtime/objmodel.h"
#include "runtime/types.h"
#include "runtime/util.h"

namespace pyston {

extern "C" Box* createTuple(int64_t nelts, Box** elts) {
    BoxedTuple::GCVector velts(elts, elts + nelts);
    return new BoxedTuple(std::move(velts));
}

Box* _tupleSlice(BoxedTuple* self, i64 start, i64 stop, i64 step) {

    i64 size = self->elts.size();
    assert(step != 0);
    if (step > 0) {
        assert(0 <= start);
        assert(stop <= size);
    } else {
        assert(start < size);
        assert(-1 <= stop);
    }

    // This is adapted from CPython's PySlice_GetIndicesEx.
    i64 slicelength;
    if (step < 0)
        slicelength = (stop - start + 1) / (step)+1;
    else
        slicelength = (stop - start - 1) / (step)+1;

    if (slicelength < 0)
        slicelength = 0;

    // FIXME: No need to initialize with 0.
    BoxedTuple::GCVector velts(slicelength, 0);

    i64 curr, i;
    for (curr = start, i = 0; i < slicelength; curr += step, i++)
        velts[i] = self->elts[curr];

    return new BoxedTuple(std::move(velts));
}

Box* tupleGetitemInt(BoxedTuple* self, BoxedInt* slice) {
    i64 n = slice->n;
    i64 size = self->elts.size();

    if (n < 0)
        n = size - n;
    if (n < 0 || n >= size) {
        fprintf(stderr, "IndexError: tuple index out of range\n");
        raiseExcHelper(IndexError, "");
    }

    Box* rtn = self->elts[n];
    return rtn;
}

Box* tupleGetitemSlice(BoxedTuple* self, BoxedSlice* slice) {
    assert(self->cls == tuple_cls);
    assert(slice->cls == slice_cls);

    i64 start, stop, step;
    parseSlice(slice, self->elts.size(), &start, &stop, &step);
    return _tupleSlice(self, start, stop, step);
}

Box* tupleGetitem(BoxedTuple* self, Box* slice) {
    assert(self->cls == tuple_cls);

    if (slice->cls == int_cls)
        return tupleGetitemInt(self, static_cast<BoxedInt*>(slice));
    else if (slice->cls == slice_cls)
        return tupleGetitemSlice(self, static_cast<BoxedSlice*>(slice));
    else
        raiseExcHelper(TypeError, "tuple indices must be integers, not %s", getTypeName(slice)->c_str());
}

Box* tupleAdd(BoxedTuple* self, Box* rhs) {
    if (rhs->cls != tuple_cls) {
        return NotImplemented;
    }

    BoxedTuple* _rhs = static_cast<BoxedTuple*>(rhs);
    BoxedTuple::GCVector velts;
    velts.insert(velts.end(), self->elts.begin(), self->elts.end());
    velts.insert(velts.end(), _rhs->elts.begin(), _rhs->elts.end());
    return new BoxedTuple(std::move(velts));
}

Box* tupleLen(BoxedTuple* t) {
    assert(t->cls == tuple_cls);
    return boxInt(t->elts.size());
}

Box* tupleRepr(BoxedTuple* t) {
    assert(t->cls == tuple_cls);

    std::ostringstream os("");
    os << "(";

    int n = t->elts.size();
    for (int i = 0; i < n; i++) {
        if (i)
            os << ", ";

        BoxedString* elt_repr = static_cast<BoxedString*>(repr(t->elts[i]));
        os << elt_repr->s;
    }
    if (n == 1)
        os << ",";
    os << ")";

    return boxString(os.str());
}

Box* _tupleCmp(BoxedTuple* lhs, BoxedTuple* rhs, AST_TYPE::AST_TYPE op_type) {
    int lsz = lhs->elts.size();
    int rsz = rhs->elts.size();

    bool is_order
        = (op_type == AST_TYPE::Lt || op_type == AST_TYPE::LtE || op_type == AST_TYPE::Gt || op_type == AST_TYPE::GtE);

    int n = std::min(lsz, rsz);
    for (int i = 0; i < n; i++) {
        Box* is_eq = compareInternal(lhs->elts[i], rhs->elts[i], AST_TYPE::Eq, NULL);
        bool bis_eq = nonzero(is_eq);

        if (bis_eq)
            continue;

        if (op_type == AST_TYPE::Eq) {
            return boxBool(false);
        } else if (op_type == AST_TYPE::NotEq) {
            return boxBool(true);
        } else {
            Box* r = compareInternal(lhs->elts[i], rhs->elts[i], op_type, NULL);
            return r;
        }
    }

    if (op_type == AST_TYPE::Lt)
        return boxBool(lsz < rsz);
    else if (op_type == AST_TYPE::LtE)
        return boxBool(lsz <= rsz);
    else if (op_type == AST_TYPE::Gt)
        return boxBool(lsz > rsz);
    else if (op_type == AST_TYPE::GtE)
        return boxBool(lsz >= rsz);
    else if (op_type == AST_TYPE::Eq)
        return boxBool(lsz == rsz);
    else if (op_type == AST_TYPE::NotEq)
        return boxBool(lsz != rsz);

    RELEASE_ASSERT(0, "%d", op_type);
}

Box* tupleLt(BoxedTuple* self, Box* rhs) {
    if (rhs->cls != tuple_cls) {
        return NotImplemented;
    }
    return _tupleCmp(self, static_cast<BoxedTuple*>(rhs), AST_TYPE::Lt);
}

Box* tupleLe(BoxedTuple* self, Box* rhs) {
    if (rhs->cls != tuple_cls) {
        return NotImplemented;
    }
    return _tupleCmp(self, static_cast<BoxedTuple*>(rhs), AST_TYPE::LtE);
}

Box* tupleGt(BoxedTuple* self, Box* rhs) {
    if (rhs->cls != tuple_cls) {
        return NotImplemented;
    }
    return _tupleCmp(self, static_cast<BoxedTuple*>(rhs), AST_TYPE::Gt);
}

Box* tupleGe(BoxedTuple* self, Box* rhs) {
    if (rhs->cls != tuple_cls) {
        return NotImplemented;
    }
    return _tupleCmp(self, static_cast<BoxedTuple*>(rhs), AST_TYPE::GtE);
}

Box* tupleEq(BoxedTuple* self, Box* rhs) {
    if (rhs->cls != tuple_cls) {
        return NotImplemented;
    }
    return _tupleCmp(self, static_cast<BoxedTuple*>(rhs), AST_TYPE::Eq);
}

Box* tupleNe(BoxedTuple* self, Box* rhs) {
    if (rhs->cls != tuple_cls) {
        return NotImplemented;
    }
    return _tupleCmp(self, static_cast<BoxedTuple*>(rhs), AST_TYPE::NotEq);
}

Box* tupleContains(BoxedTuple* self, Box* elt) {
    int size = self->elts.size();
    for (int i = 0; i < size; i++) {
        Box* e = self->elts[i];
        Box* cmp = compareInternal(e, elt, AST_TYPE::Eq, NULL);
        bool b = nonzero(cmp);
        if (b)
            return True;
    }
    return False;
}

Box* tupleHash(BoxedTuple* self) {
    assert(self->cls == tuple_cls);

    int64_t rtn = 3527539;
    for (Box* e : self->elts) {
        BoxedInt* h = hash(e);
        assert(h->cls == int_cls);
        rtn ^= h->n + 0x9e3779b9 + (rtn << 6) + (rtn >> 2);
    }

    return boxInt(rtn);
}

BoxedClass* tuple_iterator_cls = NULL;
extern "C" void tupleIteratorGCHandler(GCVisitor* v, Box* b) {
    boxGCHandler(v, b);
    BoxedTupleIterator* it = (BoxedTupleIterator*)b;
    v->visit(it->t);
}


void setupTuple() {
    tuple_iterator_cls = new BoxedClass(object_cls, &tupleIteratorGCHandler, 0, sizeof(BoxedTuple), false);

    tuple_cls->giveAttr("__name__", boxStrConstant("tuple"));

    CLFunction* getitem = createRTFunction(2, 0, 0, 0);
    addRTFunction(getitem, (void*)tupleGetitemInt, UNKNOWN,
                  std::vector<ConcreteCompilerType*>{ BOXED_TUPLE, BOXED_INT });
    addRTFunction(getitem, (void*)tupleGetitemSlice, SLICE, std::vector<ConcreteCompilerType*>{ BOXED_TUPLE, SLICE });
    addRTFunction(getitem, (void*)tupleGetitem, UNKNOWN, std::vector<ConcreteCompilerType*>{ BOXED_TUPLE, UNKNOWN });
    tuple_cls->giveAttr("__getitem__", new BoxedFunction(getitem));

    tuple_cls->giveAttr("__contains__", new BoxedFunction(boxRTFunction((void*)tupleContains, BOXED_BOOL, 2)));

    tuple_cls->giveAttr("__iter__",
                        new BoxedFunction(boxRTFunction((void*)tupleIter, typeFromClass(tuple_iterator_cls), 1)));


    tuple_cls->giveAttr("__lt__", new BoxedFunction(boxRTFunction((void*)tupleLt, UNKNOWN, 2)));
    tuple_cls->giveAttr("__le__", new BoxedFunction(boxRTFunction((void*)tupleLe, UNKNOWN, 2)));
    tuple_cls->giveAttr("__gt__", new BoxedFunction(boxRTFunction((void*)tupleGt, UNKNOWN, 2)));
    tuple_cls->giveAttr("__ge__", new BoxedFunction(boxRTFunction((void*)tupleGe, UNKNOWN, 2)));
    tuple_cls->giveAttr("__eq__", new BoxedFunction(boxRTFunction((void*)tupleEq, UNKNOWN, 2)));
    tuple_cls->giveAttr("__ne__", new BoxedFunction(boxRTFunction((void*)tupleNe, UNKNOWN, 2)));

    tuple_cls->giveAttr("__hash__", new BoxedFunction(boxRTFunction((void*)tupleHash, BOXED_INT, 1)));
    tuple_cls->giveAttr("__len__", new BoxedFunction(boxRTFunction((void*)tupleLen, BOXED_INT, 1)));
    tuple_cls->giveAttr("__repr__", new BoxedFunction(boxRTFunction((void*)tupleRepr, STR, 1)));
    tuple_cls->giveAttr("__str__", tuple_cls->getattr("__repr__"));
    tuple_cls->giveAttr("__add__", new BoxedFunction(boxRTFunction((void*)tupleAdd, BOXED_TUPLE, 2)));

    tuple_cls->freeze();

    gc::registerStaticRootObj(tuple_iterator_cls);
    tuple_iterator_cls->giveAttr("__name__", boxStrConstant("tupleiterator"));

    CLFunction* hasnext = boxRTFunction((void*)tupleiterHasnextUnboxed, BOOL, 1);
    addRTFunction(hasnext, (void*)tupleiterHasnext, BOXED_BOOL);
    tuple_iterator_cls->giveAttr("__hasnext__", new BoxedFunction(hasnext));
    tuple_iterator_cls->giveAttr(
        "__iter__", new BoxedFunction(boxRTFunction((void*)tupleIterIter, typeFromClass(tuple_iterator_cls), 1)));
    tuple_iterator_cls->giveAttr("next", new BoxedFunction(boxRTFunction((void*)tupleiterNext, UNKNOWN, 1)));

    tuple_iterator_cls->freeze();
}

void teardownTuple() {
    // TODO do clearattrs?
    // decref(tuple_iterator_cls);
}
}
