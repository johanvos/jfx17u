/*
 * Copyright (C) 2013-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSDOMPromiseDeferred.h"

#include "EventLoop.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMPromise.h"
#include "LocalDOMWindow.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSONObject.h>
#include <JavaScriptCore/JSPromiseConstructor.h>
#include <JavaScriptCore/Strong.h>
#include <wtf/Scope.h>

namespace WebCore {
using namespace JSC;

JSC::JSValue DeferredPromise::promise() const
{
    if (isEmpty())
        return jsUndefined();

    ASSERT(deferred());
    return deferred();
}

void DeferredPromise::callFunction(JSGlobalObject& lexicalGlobalObject, ResolveMode mode, JSValue resolution)
{
    if (shouldIgnoreRequestToFulfill())
        return;

    JSC::VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto handleExceptionIfNeeded = makeScopeExit([&] {
        if (UNLIKELY(scope.exception()))
            handleUncaughtException(scope, *jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject));
    });

    if (activeDOMObjectsAreSuspended() || !ScriptDisallowedScope::isScriptAllowedInMainThread()) {
        JSC::Strong<JSC::Unknown, ShouldStrongDestructorGrabLock::Yes> strongResolution(lexicalGlobalObject.vm(), resolution);
        ASSERT(!activeDOMObjectsAreSuspended() || scriptExecutionContext()->eventLoop().isSuspended());
        scriptExecutionContext()->eventLoop().queueTask(TaskSource::Networking, [this, protectedThis = Ref { *this }, mode, strongResolution = WTFMove(strongResolution)]() mutable {
            if (shouldIgnoreRequestToFulfill())
                return;

            JSC::JSGlobalObject* lexicalGlobalObject = globalObject();
            JSC::JSLockHolder locker(lexicalGlobalObject);
            callFunction(*globalObject(), mode, strongResolution.get());
        });
        return;
    }

    // FIXME: We could have error since any JS call can throw stack-overflow errors.
    // https://bugs.webkit.org/show_bug.cgi?id=203402
    switch (mode) {
    case ResolveMode::Resolve:
        deferred()->resolve(&lexicalGlobalObject, resolution);
        break;
    case ResolveMode::Reject:
        deferred()->reject(&lexicalGlobalObject, resolution);
        break;
    case ResolveMode::RejectAsHandled:
        deferred()->rejectAsHandled(&lexicalGlobalObject, resolution);
        break;
    }

    if (m_mode == Mode::ClearPromiseOnResolve)
        clear();
}

void DeferredPromise::whenSettled(Function<void()>&& callback)
{
    if (shouldIgnoreRequestToFulfill())
        return;

    if (activeDOMObjectsAreSuspended()) {
        scriptExecutionContext()->eventLoop().queueTask(TaskSource::Networking, [this, protectedThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
            whenSettled(WTFMove(callback));
        });
        return;
    }

    {
        auto* globalObject = this->globalObject();
        auto& vm = globalObject->vm();
        JSC::JSLockHolder locker(vm);
        auto scope = DECLARE_CATCH_SCOPE(vm);
        DOMPromise::whenPromiseIsSettled(globalObject, deferred(), WTFMove(callback));
        DEFERRED_PROMISE_HANDLE_AND_RETURN_IF_EXCEPTION(scope, globalObject);
    }
}

void DeferredPromise::reject(RejectAsHandled rejectAsHandled)
{
    if (shouldIgnoreRequestToFulfill())
        return;

    ASSERT(deferred());
    ASSERT(m_globalObject);
    auto& lexicalGlobalObject = *m_globalObject;
    JSC::JSLockHolder locker(&lexicalGlobalObject);
    reject(lexicalGlobalObject, JSC::jsUndefined(), rejectAsHandled);
}

void DeferredPromise::reject(std::nullptr_t, RejectAsHandled rejectAsHandled)
{
    if (shouldIgnoreRequestToFulfill())
        return;

    ASSERT(deferred());
    ASSERT(m_globalObject);
    auto& lexicalGlobalObject = *m_globalObject;
    JSC::JSLockHolder locker(&lexicalGlobalObject);
    reject(lexicalGlobalObject, JSC::jsNull(), rejectAsHandled);
}

void DeferredPromise::reject(Exception exception, RejectAsHandled rejectAsHandled)
{
    JSC::JSValue exceptionObject;
    reject(exception, rejectAsHandled, exceptionObject);
}

void DeferredPromise::reject(Exception exception, RejectAsHandled rejectAsHandled, JSC::JSValue& exceptionObject)
{
    if (shouldIgnoreRequestToFulfill())
        return;

    Ref protectedThis(*this);
    ASSERT(deferred());
    ASSERT(m_globalObject);
    auto& lexicalGlobalObject = *m_globalObject;
    JSC::VM& vm = lexicalGlobalObject.vm();
    JSC::JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (exception.code() == ExceptionCode::ExistingExceptionError) {
        EXCEPTION_ASSERT(scope.exception());
        auto error = scope.exception()->value();
        bool isTerminating = handleTerminationExceptionIfNeeded(scope, lexicalGlobalObject);
        if (!isTerminating) {
        scope.clearException();
            reject<IDLAny>(error, rejectAsHandled);
        }
        return;
    }

    if (exceptionObject.isEmpty()) {
        exceptionObject = createDOMException(lexicalGlobalObject, WTFMove(exception));
    if (UNLIKELY(scope.exception())) {
        handleUncaughtException(scope, lexicalGlobalObject);
        return;
    }
    }

    reject(lexicalGlobalObject, exceptionObject, rejectAsHandled);
    if (UNLIKELY(scope.exception()))
        handleUncaughtException(scope, lexicalGlobalObject);
}

void DeferredPromise::reject(ExceptionCode ec, const String& message, RejectAsHandled rejectAsHandled)
{
    if (shouldIgnoreRequestToFulfill())
        return;

    Ref protectedThis(*this);
    ASSERT(deferred());
    ASSERT(m_globalObject);
    auto& lexicalGlobalObject = *m_globalObject;
    JSC::VM& vm = lexicalGlobalObject.vm();
    JSC::JSLockHolder locker(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (ec == ExceptionCode::ExistingExceptionError) {
        EXCEPTION_ASSERT(scope.exception());
        auto error = scope.exception()->value();
        bool isTerminating = handleTerminationExceptionIfNeeded(scope, lexicalGlobalObject);
        if (!isTerminating) {
        scope.clearException();
            reject<IDLAny>(error, rejectAsHandled);
        }
        return;
    }

    auto error = createDOMException(&lexicalGlobalObject, ec, message);
    if (UNLIKELY(scope.exception())) {
        handleUncaughtException(scope, lexicalGlobalObject);
        return;
    }

    reject(lexicalGlobalObject, error, rejectAsHandled);
    if (UNLIKELY(scope.exception()))
        handleUncaughtException(scope, lexicalGlobalObject);
}

void rejectPromiseWithExceptionIfAny(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, JSPromise& promise, JSC::CatchScope& catchScope)
{
    UNUSED_PARAM(lexicalGlobalObject);
    if (LIKELY(!catchScope.exception()))
        return;
    if (catchScope.vm().hasPendingTerminationException())
        return;

    JSValue error = catchScope.exception()->value();
    catchScope.clearException();

    DeferredPromise::create(globalObject, promise)->reject<IDLAny>(error);
}

JSC::EncodedJSValue createRejectedPromiseWithTypeError(JSC::JSGlobalObject& lexicalGlobalObject, const String& errorMessage, RejectedPromiseWithTypeErrorCause cause)
{
    auto& globalObject = lexicalGlobalObject;
    auto& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto promiseConstructor = globalObject.promiseConstructor();
    auto rejectFunction = promiseConstructor->get(&lexicalGlobalObject, vm.propertyNames->builtinNames().rejectPrivateName());
    RETURN_IF_EXCEPTION(scope, { });
    auto* rejectionValue = static_cast<ErrorInstance*>(createTypeError(&lexicalGlobalObject, errorMessage));
    if (cause == RejectedPromiseWithTypeErrorCause::NativeGetter)
        rejectionValue->setNativeGetterTypeError();

    auto callData = JSC::getCallData(rejectFunction);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(rejectionValue);
    ASSERT(!arguments.hasOverflowed());

    RELEASE_AND_RETURN(scope, JSValue::encode(call(&lexicalGlobalObject, rejectFunction, callData, promiseConstructor, arguments)));
}

static inline JSC::JSValue parseAsJSON(JSC::JSGlobalObject* lexicalGlobalObject, const String& data)
{
    JSC::JSLockHolder lock(lexicalGlobalObject);
    return JSC::JSONParse(lexicalGlobalObject, data);
}

void fulfillPromiseWithJSON(Ref<DeferredPromise>&& promise, const String& data)
{
    JSC::JSValue value = parseAsJSON(promise->globalObject(), data);
    if (!value)
        promise->reject(ExceptionCode::SyntaxError);
    else
        promise->resolve<IDLAny>(value);
}

void fulfillPromiseWithArrayBuffer(Ref<DeferredPromise>&& promise, ArrayBuffer* arrayBuffer)
{
    if (!arrayBuffer) {
        promise->reject<IDLAny>(createOutOfMemoryError(promise->globalObject()));
        return;
    }
    promise->resolve<IDLInterface<ArrayBuffer>>(*arrayBuffer);
}

void fulfillPromiseWithArrayBufferFromSpan(Ref<DeferredPromise>&& promise, std::span<const uint8_t> data)
{
    fulfillPromiseWithArrayBuffer(WTFMove(promise), ArrayBuffer::tryCreate(data).get());
}

void fulfillPromiseWithUint8Array(Ref<DeferredPromise>&& promise, Uint8Array* bytes)
{
    if (!bytes) {
        promise->reject<IDLAny>(createOutOfMemoryError(promise->globalObject()));
        return;
    }
    promise->resolve<IDLInterface<Uint8Array>>(*bytes);
}

void fulfillPromiseWithUint8ArrayFromSpan(Ref<DeferredPromise>&& promise, std::span<const uint8_t> data)
{
    fulfillPromiseWithUint8Array(WTFMove(promise), Uint8Array::tryCreate(data).get());
}

bool DeferredPromise::handleTerminationExceptionIfNeeded(CatchScope& scope, JSDOMGlobalObject& lexicalGlobalObject)
{
    auto* exception = scope.exception();
    VM& vm = scope.vm();

    auto& scriptExecutionContext = *lexicalGlobalObject.scriptExecutionContext();
    if (auto* globalScope = dynamicDowncast<WorkerGlobalScope>(scriptExecutionContext)) {
        auto* scriptController = globalScope->script();
        bool terminatorCausedException = vm.isTerminationException(exception);
        if (terminatorCausedException || (scriptController && scriptController->isTerminatingExecution())) {
            scriptController->forbidExecution();
            m_needsAbort = true;
            return true;
        }
    }
    return false;
}

void DeferredPromise::handleUncaughtException(CatchScope& scope, JSDOMGlobalObject& lexicalGlobalObject)
{
    auto* exception = scope.exception();
    handleTerminationExceptionIfNeeded(scope, lexicalGlobalObject);
    reportException(&lexicalGlobalObject, exception);
};

} // namespace WebCore
