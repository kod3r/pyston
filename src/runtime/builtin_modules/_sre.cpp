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

#include <cmath>

#include "core/types.h"
#include "gc/collector.h"
#include "runtime/inline/boxing.h"
#include "runtime/types.h"
#include "runtime/util.h"

namespace pyston {

BoxedModule* sre_module;

void setupSre() {
    sre_module = createModule("_sre", "__builtin__");

    sre_module->giveAttr("MAGIC", boxInt(20031017));
    sre_module->giveAttr("CODESIZE", boxInt(4));
}
}
