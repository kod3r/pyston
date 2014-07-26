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

#include "runtime/long.h"

#include <cmath>
#include <gmp.h>
#include <sstream>

#include "codegen/compvars.h"
#include "core/common.h"
#include "core/options.h"
#include "core/stats.h"
#include "core/types.h"
#include "gc/collector.h"
#include "runtime/gc_runtime.h"
#include "runtime/inline/boxing.h"
#include "runtime/objmodel.h"
#include "runtime/types.h"
#include "runtime/util.h"

namespace pyston {

BoxedClass* long_cls;
const ObjectFlavor long_flavor(&boxGCHandler, NULL);

extern "C" Box* createLong(const std::string* s) {
    BoxedLong* rtn = new BoxedLong(long_cls);
    int r = mpz_init_set_str(rtn->n, s->c_str(), 10);
    RELEASE_ASSERT(r == 0, "%d: '%s'", r, s->c_str());
    return rtn;
}

extern "C" Box* longNew(Box* _cls, Box* val) {
    if (!isSubclass(_cls->cls, type_cls))
        raiseExcHelper(TypeError, "long.__new__(X): X is not a type object (%s)", getTypeName(_cls)->c_str());

    BoxedClass* cls = static_cast<BoxedClass*>(_cls);
    if (!isSubclass(cls, long_cls))
        raiseExcHelper(TypeError, "long.__new__(%s): %s is not a subtype of long", getNameOfClass(cls)->c_str(),
                       getNameOfClass(cls)->c_str());

    assert(cls->instance_size >= sizeof(BoxedInt));
    void* mem = rt_alloc(cls->instance_size);
    BoxedLong* rtn = ::new (mem) BoxedLong(cls);
    initUserAttrs(rtn, cls);

    if (val->cls == int_cls) {
        mpz_init_set_si(rtn->n, static_cast<BoxedInt*>(val)->n);
    } else if (val->cls == str_cls) {
        const std::string& s = static_cast<BoxedString*>(val)->s;
        int r = mpz_init_set_str(rtn->n, s.c_str(), 10);
        RELEASE_ASSERT(r == 0, "");
    } else {
        fprintf(stderr, "TypeError: int() argument must be a string or a number, not '%s'\n",
                getTypeName(val)->c_str());
        raiseExcHelper(TypeError, "");
    }
    return rtn;
}

Box* longRepr(BoxedLong* v) {
    if (!isSubclass(v->cls, long_cls))
        raiseExcHelper(TypeError, "descriptor '__repr__' requires a 'long' object but received a '%s'",
                       getTypeName(v)->c_str());

    int space_required = mpz_sizeinbase(v->n, 10) + 2; // basic size
    space_required += 1;                               // 'L' suffix
    char* buf = (char*)malloc(space_required);
    mpz_get_str(buf, 10, v->n);
    strcat(buf, "L");

    auto rtn = new BoxedString(buf);
    free(buf);

    return rtn;
}

Box* longStr(BoxedLong* v) {
    if (!isSubclass(v->cls, long_cls))
        raiseExcHelper(TypeError, "descriptor '__str__' requires a 'long' object but received a '%s'",
                       getTypeName(v)->c_str());

    char* buf = mpz_get_str(NULL, 10, v->n);
    auto rtn = new BoxedString(buf);
    free(buf);

    return rtn;
}

Box* longMul(BoxedLong* v1, Box* _v2) {
    if (!isSubclass(v1->cls, long_cls))
        raiseExcHelper(TypeError, "descriptor '__mul__' requires a 'long' object but received a '%s'",
                       getTypeName(v1)->c_str());

    if (!isSubclass(_v2->cls, long_cls))
        return NotImplemented;

    BoxedLong* v2 = static_cast<BoxedLong*>(_v2);

    BoxedLong* r = new BoxedLong(long_cls);
    mpz_init(r->n);
    mpz_mul(r->n, v1->n, v2->n);
    return r;
}

void setupLong() {
    long_cls->giveAttr("__name__", boxStrConstant("long"));

    long_cls->giveAttr("__new__",
                       new BoxedFunction(boxRTFunction((void*)longNew, UNKNOWN, 2, 1, false, false), { boxInt(0) }));

    long_cls->giveAttr("__mul__", new BoxedFunction(boxRTFunction((void*)longMul, UNKNOWN, 2)));

    long_cls->giveAttr("__repr__", new BoxedFunction(boxRTFunction((void*)longRepr, STR, 1)));
    long_cls->giveAttr("__str__", new BoxedFunction(boxRTFunction((void*)longStr, STR, 1)));

    long_cls->freeze();
}
}
