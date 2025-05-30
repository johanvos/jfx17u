/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "B3AtomicValue.h"
#include "B3ValueInlines.h"

#if ENABLE(B3_JIT)

namespace JSC { namespace B3 {

AtomicValue::~AtomicValue() = default;

void AtomicValue::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    out.print(comma, "width = ", m_width);

    MemoryValue::dumpMeta(comma, out);
}

AtomicValue::AtomicValue(AtomicValue::AtomicValueRMW, Kind kind, Origin origin, Width width, Value* operand, Value* pointer, MemoryValue::OffsetType offset, HeapRange range, HeapRange fenceRange)
    : MemoryValue(CheckedOpcode, kind, operand->type(), Two, origin, offset, range, fenceRange, operand, pointer)
    , m_width(width)
{
    ASSERT(bestType(GP, accessWidth()) == accessType());

    switch (kind.opcode()) {
    case AtomicXchgAdd:
    case AtomicXchgAnd:
    case AtomicXchgOr:
    case AtomicXchgSub:
    case AtomicXchgXor:
    case AtomicXchg:
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

AtomicValue::AtomicValue(AtomicValue::AtomicValueCAS, Kind kind, Origin origin, Width width, Value* expectedValue, Value* newValue, Value* pointer, MemoryValue::OffsetType offset, HeapRange range, HeapRange fenceRange)
    : MemoryValue(CheckedOpcode, kind, kind.opcode() == AtomicWeakCAS ? Int32 : expectedValue->type(), Three, origin, offset, range, fenceRange, expectedValue, newValue, pointer)
    , m_width(width)
{
    ASSERT(bestType(GP, accessWidth()) == accessType());

    switch (kind.opcode()) {
    case AtomicWeakCAS:
    case AtomicStrongCAS:
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

