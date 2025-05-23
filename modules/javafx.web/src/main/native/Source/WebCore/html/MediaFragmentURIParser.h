/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(VIDEO)

#include <wtf/MediaTime.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

namespace WebCore {

class MediaFragmentURIParser final {
public:

    MediaFragmentURIParser(const URL&);

    MediaTime startTime();
    MediaTime endTime();

private:

    void parseFragments();

    enum TimeFormat { None, Invalid, NormalPlayTime, SMPTETimeCode, WallClockTimeCode };
    void parseTimeFragment();
    bool parseNPTFragment(std::span<const LChar>, MediaTime& startTime, MediaTime& endTime);
    bool parseNPTTime(std::span<const LChar>, unsigned& offset, MediaTime&);

    URL m_url;
    TimeFormat m_timeFormat;
    MediaTime m_startTime;
    MediaTime m_endTime;
    Vector<std::pair<String, String>> m_fragments;
};

} // namespace WebCore

#endif // ENABLE(VIDEO)
