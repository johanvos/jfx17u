/*
 * Copyright (c) 2011, 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */


#include "FrameLoaderClientJava.h"
#include "FrameNetworkingContextJava.h"
#include "WebPage.h"

#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JavaScript.h>
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/Chrome.h>
#include <WebCore/DNS.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FormState.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HistoryItem.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PluginWidgetJava.h>
#include <WebCore/PolicyChecker.h>
#include <WebCore/ProgressTracker.h>
#include <WebCore/ScriptController.h>
#include <WebCore/Settings.h>
#include <WebCore/WindowFeatures.h>

#include "com_sun_webkit_LoadListenerClient.h"

namespace WebCore {

namespace FrameLoaderClientJavaInternal {

static JGClass webPageClass;
static JGClass networkContextClass;

static jmethodID setRequestURLMID;
static jmethodID removeRequestURLMID;

static jmethodID fireLoadEventMID;
static jmethodID fireResourceLoadEventMID;
static jmethodID canHandleURLMID;

static jmethodID permitNavigateActionMID;
static jmethodID permitRedirectActionMID;
static jmethodID permitAcceptResourceActionMID;
static jmethodID permitSubmitDataActionMID;
static jmethodID permitEnableScriptsActionMID;
static jmethodID permitNewWindowActionMID;

static jmethodID didClearWindowObjectMID;

static jmethodID frameCreatedMID;
static jmethodID frameDestroyedMID;

static void initRefs(JNIEnv* env)
{
    if (!webPageClass) {
        webPageClass = JLClass(env->FindClass(
            "com/sun/webkit/WebPage"));
        ASSERT(webPageClass);

        setRequestURLMID = env->GetMethodID(webPageClass, "fwkSetRequestURL", "(JILjava/lang/String;)V");
        ASSERT(setRequestURLMID);
        removeRequestURLMID = env->GetMethodID(webPageClass, "fwkRemoveRequestURL", "(JI)V");
        ASSERT(removeRequestURLMID);

        fireLoadEventMID = env->GetMethodID(webPageClass, "fwkFireLoadEvent",
                                            "(JILjava/lang/String;Ljava/lang/String;DI)V");
        ASSERT(fireLoadEventMID);
        fireResourceLoadEventMID = env->GetMethodID(webPageClass, "fwkFireResourceLoadEvent",
                                                    "(JIILjava/lang/String;DI)V");
        ASSERT(fireResourceLoadEventMID);

        permitNavigateActionMID = env->GetMethodID(webPageClass, "fwkPermitNavigateAction",
                                                   "(JLjava/lang/String;)Z");
        ASSERT(permitNavigateActionMID);

        permitRedirectActionMID = env->GetMethodID(webPageClass, "fwkPermitRedirectAction",
                                                   "(JLjava/lang/String;)Z");
        ASSERT(permitRedirectActionMID);

        permitAcceptResourceActionMID = env->GetMethodID(webPageClass, "fwkPermitAcceptResourceAction",
                                                         "(JLjava/lang/String;)Z");
        ASSERT(permitAcceptResourceActionMID);

        permitSubmitDataActionMID = env->GetMethodID(webPageClass, "fwkPermitSubmitDataAction",
                                                     "(JLjava/lang/String;Ljava/lang/String;Z)Z");
        ASSERT(permitSubmitDataActionMID);

        permitEnableScriptsActionMID = env->GetMethodID(webPageClass, "fwkPermitEnableScriptsAction",
                                                        "(JLjava/lang/String;)Z");
        ASSERT(permitEnableScriptsActionMID);

        permitNewWindowActionMID = env->GetMethodID(webPageClass, "fwkPermitNewWindowAction",
                                                    "(JLjava/lang/String;)Z");
        ASSERT(permitNewWindowActionMID);

        didClearWindowObjectMID = env->GetMethodID(webPageClass, "fwkDidClearWindowObject", "(JJ)V");
        ASSERT(didClearWindowObjectMID);

        frameCreatedMID = env->GetMethodID(webPageClass, "fwkFrameCreated", "(J)V");
        ASSERT(frameCreatedMID);

        frameDestroyedMID = env->GetMethodID(webPageClass, "fwkFrameDestroyed", "(J)V");
        ASSERT(frameDestroyedMID);
    }
    if (!networkContextClass) {
        networkContextClass = JLClass(env->FindClass("com/sun/webkit/network/NetworkContext"));
        ASSERT(networkContextClass);

        canHandleURLMID = env->GetStaticMethodID(networkContextClass, "canHandleURL", "(Ljava/lang/String;)Z");
        ASSERT(canHandleURLMID);
    }
}
// This was copied from file "WebKit/Source/WebKit/mac/Misc/WebKitErrors.h".
enum {
    WebKitErrorCannotShowMIMEType =                             100,
    WebKitErrorCannotShowURL =                                  101,
    WebKitErrorFrameLoadInterruptedByPolicyChange =             102,
    WebKitErrorCannotUseRestrictedPort =                        103,
    WebKitErrorCannotFindPlugIn =                               200,
    WebKitErrorCannotLoadPlugIn =                               201,
    WebKitErrorJavaUnavailable =                                202,
    WebKitErrorPluginWillHandleLoad =                           203
};

enum ContentDispositionType {
    ContentDispositionNone,
    ContentDispositionInline,
    ContentDispositionAttachment,
    ContentDispositionOther
};

// Below function was removed after https://bugs.webkit.org/show_bug.cgi?id=163095
// from HTTPParser.h.
ContentDispositionType contentDispositionType(const String& contentDisposition)
{
    if (contentDisposition.isEmpty())
        return ContentDispositionNone;

    Vector<String> parameters = contentDisposition.split(';');

    String dispositionType = parameters[0];
    dispositionType.trim(deprecatedIsSpaceOrNewline);

    if (equalLettersIgnoringASCIICase(dispositionType, "inline"_s))
        return ContentDispositionInline;

    // Some broken sites just send bogus headers like
    //
    //   Content-Disposition: ; filename="file"
    //   Content-Disposition: filename="file"
    //   Content-Disposition: name="file"
    //
    // without a disposition token... screen those out.
    if (!isValidHTTPToken(dispositionType))
        return ContentDispositionNone;

    // We have a content-disposition of "attachment" or unknown.
    // RFC 2183, section 2.8 says that an unknown disposition
    // value should be treated as "attachment"
    return ContentDispositionAttachment;
}
} // namespace

FrameLoaderClientJava::FrameLoaderClientJava(const JLObject &webPage)
    : m_page(nullptr)
    , m_frame(nullptr)
    , m_isPageRedirected(false)
    , m_hasRepresentation(false)
    , m_webPage(webPage)
{
}

FrameLoaderClientJava::~FrameLoaderClientJava()
{
    using namespace FrameLoaderClientJavaInternal;
    WC_GETJAVAENV_CHKRET(env);
    initRefs(env);

    ASSERT(m_webPage);
    ASSERT(m_frame);
    env->CallVoidMethod(m_webPage, frameDestroyedMID, ptr_to_jlong(m_frame));
    WTF::CheckAndClearException(env);
}

void FrameLoaderClientJava::dispatchDidNavigateWithinPage()
{
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_PAGE_REPLACED,
                  dynamicDowncast<LocalFrame>(frame())->document()->url().string(),
                  dynamicDowncast<LocalFrame>(frame())->loader().documentLoader()->responseMIMEType(),
                  1.0 /* progress */);
}

// Called from twkInit to initialize the client. This will ensure that
// the page field is initialized before any operation that needs it
void FrameLoaderClientJava::init()
{
    (void)page();
}

Page* FrameLoaderClientJava::page()
{
    if (!m_page) {
        m_page = WebPage::pageFromJObject(m_webPage);
        ASSERT(m_page);
    }
    return m_page;
}

Frame* FrameLoaderClientJava::frame()
{
    return m_frame;
}

void FrameLoaderClientJava::setFrame(Frame* frame)
{
    ASSERT(frame);
    m_frame = frame;
}

void FrameLoaderClientJava::setRequestURL(Frame* f,ResourceLoaderIdentifier  identifier, String url)
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    JLString urlJavaString(url.toJavaString(env));
    env->CallVoidMethod(m_webPage, setRequestURLMID, ptr_to_jlong(f), identifier, (jstring)urlJavaString);
    WTF::CheckAndClearException(env);
}

void FrameLoaderClientJava::removeRequestURL(Frame* f, ResourceLoaderIdentifier identifier)
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    env->CallVoidMethod(m_webPage, removeRequestURLMID, ptr_to_jlong(f), identifier);
    WTF::CheckAndClearException(env);
}

void FrameLoaderClientJava::postLoadEvent(Frame* f, int state,
                                          String url, String contentType,
                                          double progress, int errorCode)
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    JLString urlJavaString(url.toJavaString(env));
    JLString contentTypeJavaString(contentType.toJavaString(env));

    // First, notify SharedBufferManager, so users can get the full source
    // in CONTENT_RECEIVED handler
    if (state == com_sun_webkit_LoadListenerClient_PAGE_STARTED ||
        state == com_sun_webkit_LoadListenerClient_PROGRESS_CHANGED ||
        state == com_sun_webkit_LoadListenerClient_CONTENT_RECEIVED)
    {
        auto* localFrame = dynamicDowncast<LocalFrame>(f);
        DocumentLoader* dl = localFrame->loader().activeDocumentLoader();
        if (dl && dl->mainResourceData()) {
            dl->mainResourceData()->size(); // TODO-java: recheck
        }
    }

    // Second, send a load event
    env->CallVoidMethod(m_webPage, fireLoadEventMID,
                        ptr_to_jlong(f), state, (jstring)urlJavaString,
                        (jstring)contentTypeJavaString, progress, errorCode);
    WTF::CheckAndClearException(env);
}

void FrameLoaderClientJava::postResourceLoadEvent(Frame* f, int state,
                                                  ResourceLoaderIdentifier id, String contentType,
                                                  double progress, int errorCode)
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    JLString contentTypeJavaString(contentType.toJavaString(env));
    // notification for resource event listeners
    env->CallVoidMethod(m_webPage, fireResourceLoadEventMID,
                        ptr_to_jlong(f), state, id,
                        (jstring)contentTypeJavaString, progress, errorCode);
    WTF::CheckAndClearException(env);
}

String FrameLoaderClientJava::userAgent(const URL&) const
{
    if (!m_page)
        return emptyString();
    return m_page->settings().userAgent();
}

void FrameLoaderClientJava::savePlatformDataToCachedFrame(CachedFrame*)
{
    notImplemented();
}

void FrameLoaderClientJava::transitionToCommittedFromCachedFrame(CachedFrame*)
{
    notImplemented();
}

void FrameLoaderClientJava::transitionToCommittedForNewPage(InitializingIframe initializingIframe)
{
    FloatRect pageRect = frame()->page()->chrome().pageRect();
    Color bkColor(Color::white);
    std::optional<Color> backgroundColor;
    FrameView* fv = frame()->virtualView();
    auto* localFrameView = dynamicDowncast<LocalFrameView>(fv);
    if (localFrameView) {
        backgroundColor = localFrameView->baseBackgroundColor();
    }
    auto* localFrame = dynamicDowncast<LocalFrame>(frame());
    localFrame->createView(IntRect(pageRect).size(), backgroundColor, /* fixedLayoutSize */ { }, /* fixedVisibleContentRect */ { });
}

WTF::Ref<WebCore::DocumentLoader> FrameLoaderClientJava::createDocumentLoader(const WebCore::ResourceRequest& request, const SubstituteData& substituteData)
{
    return DocumentLoader::create(request, substituteData);
}

void FrameLoaderClientJava::dispatchWillSubmitForm(FormState&, CompletionHandler<void()>&& function)
{
    // FIXME: This is surely too simple
    if (!frame() || !function) {
        return;
    }
    function();
}

void FrameLoaderClientJava::committedLoad(DocumentLoader* loader, const SharedBuffer& data)
{
    //uta: for m_pluginWidget we need to do something different
    loader->commitData(data);
}

void FrameLoaderClientJava::dispatchDecidePolicyForResponse(const ResourceResponse& response, const ResourceRequest&, const String&, FramePolicyFunction&& policyFunction)
{
    using namespace FrameLoaderClientJavaInternal;
    PolicyAction action;

    int statusCode = response.httpStatusCode();
    if (statusCode == 204 || statusCode == 205) {
        // The server does not want us to replace the page contents.
        action = PolicyAction::Ignore;
    } else if (contentDispositionType(response.httpHeaderField(HTTPHeaderName::ContentDisposition)) == ContentDispositionAttachment) {
        // The server wants us to download instead of replacing the page contents.
        // Downloading is handled by the embedder, but we still get the initial
        // response so that we can ignore it and clean up properly.
        action = PolicyAction::Ignore;
    } else if (!canShowMIMEType(response.mimeType())) {
        // Make sure that we can actually handle this type internally.
        action = PolicyAction::Ignore;
    } else {
        // OK, we will render this page.
        action = PolicyAction::Use;
    }

    // NOTE: PolicyChangeError will be generated when action is not PolicyUse.
    policyFunction(action);
}

void FrameLoaderClientJava::dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier identifier, const ResourceResponse& response)
{
    m_response = response;

    if (identifier == m_mainResourceRequestID) {
        double progress = page()->progress().estimatedProgress();
        postLoadEvent(frame(),
                      com_sun_webkit_LoadListenerClient_CONTENTTYPE_RECEIVED,
                      response.url().string(),
                      response.mimeType(),
                      progress);
    }
}

void FrameLoaderClientJava::dispatchDecidePolicyForNewWindowAction(const NavigationAction&,
                                                                   const ResourceRequest& req,
                                                                   FormState*,
                                                                   const String&,
                                                                   std::optional<HitTestResult>&&,
                                                                   FramePolicyFunction&& policyFunction)
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    ASSERT(frame() && policyFunction);
    if (!frame() || !policyFunction) {
        return;
    }

    JLString urlString(req.url().string().toJavaString(env));
    bool permit = jbool_to_bool(env->CallBooleanMethod(m_webPage, permitNewWindowActionMID,
                                                       ptr_to_jlong(frame()), (jstring)urlString));
    WTF::CheckAndClearException(env);

    // FIXME: I think Qt version marshals this to another thread so when we
    // have multi-threaded download, we might need to do the same
    policyFunction(permit ? PolicyAction::Use : PolicyAction::Ignore);
}


void FrameLoaderClientJava::dispatchDecidePolicyForNavigationAction(const NavigationAction& action,
                                                                    const ResourceRequest& req,
                                                                    const ResourceResponse& redirectResponse,  // Use the correct name
                                                                    FormState* formState,
                                                                    const String& clientRedirectSourceForHistory,  // Use the correct name
                                                                    std::optional<NavigationIdentifier> navigationID,  // Use a name for the optional identifier
                                                                    std::optional<HitTestResult>&& hitTestResult,  // Match argument names
                                                                    bool hasOpener,
                                                                    SandboxFlags sandboxFlags,
                                                                    PolicyDecisionMode decisionMode,
                                                                    FramePolicyFunction&& policyFunction)
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    ASSERT(frame() && policyFunction);
    if (!frame() || !policyFunction) {
        return;
    }

    bool permit = true;

    JLString urlJavaString(req.url().string().toJavaString(env));

    // 1. Submitting/resubmitting data.
    if (action.type() == NavigationType::FormSubmitted ||
        action.type() == NavigationType::FormResubmitted)
    {
        JLString httpMethodString(req.httpMethod().toJavaString(env));
        permit = env->CallBooleanMethod(m_webPage, permitSubmitDataActionMID,
                                        ptr_to_jlong(frame()), (jstring)urlJavaString,
                                        (jstring)httpMethodString,
                                        bool_to_jbool(action.type() == NavigationType::FormSubmitted));
        WTF::CheckAndClearException(env);
    // 2. Redirecting page.
    } else if (m_isPageRedirected) {
        permit = env->CallBooleanMethod(m_webPage, permitRedirectActionMID,
                                        ptr_to_jlong(frame()), (jstring)urlJavaString);
        WTF::CheckAndClearException(env);
        m_isPageRedirected = false;
    // 3. Loading document.
    } else {
        permit = env->CallBooleanMethod(m_webPage, permitNavigateActionMID,
                                        ptr_to_jlong(frame()), (jstring)urlJavaString);
        WTF::CheckAndClearException(env);
    }

    policyFunction(permit ? PolicyAction::Use : PolicyAction::Ignore);
}

RefPtr<Widget> FrameLoaderClientJava::createPlugin(HTMLPlugInElement& element,
                                     const URL& url, const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues, const String& mimeType, bool loadManually)

{
    return adoptRef(new PluginWidgetJava(
        m_webPage,
        &element,
        url.string(),
        mimeType,
        paramNames,
        paramValues));
}

RefPtr<LocalFrame> FrameLoaderClientJava::createFrame(const AtomString& name, HTMLFrameOwnerElement& ownerElement)
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    auto* localFrame = dynamicDowncast<LocalFrame>(m_frame);
    //RefPtr<LocalFrame> childFrame(LocalFrame::createSubframe(*page(), makeUniqueRef<FrameLoaderClientJava>(m_webPage),localFrame->loader().frameID(),ownerElement));
    //createSubframe function expects a ClientCreator&&, which is a callable type
    //Instead of passing makeUniqueRef<FrameLoaderClientJava>(m_webPage) directly, we nned to wrap it in a lambda or
    //some callable object that matches the expected type for ClientCreator
    auto clientCreator = [this](auto& localFrame) -> WTF::UniqueRef<WebCore::LocalFrameLoaderClient> {
        return makeUniqueRef<FrameLoaderClientJava>(m_webPage);  // Use only m_webPage
    };
    RefPtr<LocalFrame> childFrame = LocalFrame::createSubframe(*page(), std::move(clientCreator), localFrame->loader().frameID(), ownerElement);

    static_cast<FrameLoaderClientJava&>(childFrame->loader().client()).setFrame(childFrame.get());

    childFrame->tree().setSpecifiedName(name);
    childFrame->init();

    env->CallVoidMethod(m_webPage, frameCreatedMID, ptr_to_jlong(childFrame.get()));
    WTF::CheckAndClearException(env);

    return childFrame;
}

void FrameLoaderClientJava::redirectDataToPlugin(Widget&)
{
    /*
    ASSERT(!m_pluginWidget);
    m_pluginWidget = static_cast<PluginWidgetJava*>(pluginWidget);
    */
    notImplemented();
}

ObjectContentType FrameLoaderClientJava::objectContentType(const URL& url, const String& mimeType)
{
    //copied from FrameLoaderClientEfl.cpp

    // FIXME: once plugin support is enabled, this method needs to correctly handle the 'shouldPreferPlugInsForImages' flag. See
    // WebCore::FrameLoader::defaultObjectContentType() for an example.

    if (url.isEmpty() && mimeType.isEmpty())
        return ObjectContentType::None;

    // We don't use MIMETypeRegistry::getMIMETypeForPath() because it returns "application/octet-stream" upon failure
    String type = mimeType;
    if (type.isEmpty())
        type = MIMETypeRegistry::mimeTypeForExtension(url.path().substring(url.path().reverseFind('.') + 1).toString());

    if (type.isEmpty())
        return ObjectContentType::Frame;

    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return ObjectContentType::Image;

#if 0 // PluginDatabase is disabled until we have Plugin system done.
    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return ObjectContentNetscapePlugin;
#endif

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return ObjectContentType::Frame;

    if (url.protocol() == "about"_s)
        return ObjectContentType::Frame;

    return ObjectContentType::None;
}

AtomString FrameLoaderClientJava::overrideMediaType() const
{
    notImplemented();
    return AtomString();
}

void FrameLoaderClientJava::setMainFrameDocumentReady(bool)
{
    // this is only interesting once we provide an external API for the DOM
}

bool FrameLoaderClientJava::hasWebView() const
{
    notImplemented();
    return true;
}

void FrameLoaderClientJava::assignIdentifierToInitialRequest(ResourceLoaderIdentifier, DocumentLoader*, const ResourceRequest&)
{
    notImplemented();
}

void FrameLoaderClientJava::willReplaceMultipartContent()
{
    notImplemented(); // TODO-java: recheck
}

void FrameLoaderClientJava::didReplaceMultipartContent()
{
    notImplemented(); // TODO-java: recheck
}

void FrameLoaderClientJava::updateCachedDocumentLoader(DocumentLoader&)
{
    notImplemented(); // TODO-java: recheck
}

void FrameLoaderClientJava::dispatchDidStartProvisionalLoad()
{
    m_mainResourceRequestID = { };
}

void FrameLoaderClientJava::dispatchWillSendRequest(DocumentLoader* l, ResourceLoaderIdentifier identifier, ResourceRequest& req, const ResourceResponse& res)
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    Frame* f = l->frame();
    if (!f) {
        f = frame();
    }

    double progress = 0.0;
    progress = page()->progress().estimatedProgress();

    if (!m_mainResourceRequestID) {
        m_mainResourceRequestID = identifier;
        postLoadEvent(f,
                      com_sun_webkit_LoadListenerClient_PAGE_STARTED,
                      req.url().string(),
                      res.mimeType(),
                      progress);
    } else if (m_mainResourceRequestID == identifier) { // serever-side redirection
        m_isPageRedirected = true;
        postLoadEvent(f,
                      com_sun_webkit_LoadListenerClient_PAGE_REDIRECTED,
                      req.url().string(),
                      res.mimeType(),
                      progress);
    } else {
        // Check resource policy.
        JLString urlJavaString(req.url().string().toJavaString(env));
        bool permit = jbool_to_bool(env->CallBooleanMethod(m_webPage, permitAcceptResourceActionMID,
                                                           ptr_to_jlong(f), (jstring)urlJavaString));
        WTF::CheckAndClearException(env);
        if (!permit) {
/*
            req.setURL(NULL); // will cancel loading
*/
            req.setURL(URL());
        } else {
            setRequestURL(f, identifier, req.url().string());
            postResourceLoadEvent(f,
                                  com_sun_webkit_LoadListenerClient_RESOURCE_STARTED,
                                  identifier,
                                  res.mimeType(),
                                  0.0 /* progress */);
        }
    }
}

void FrameLoaderClientJava::dispatchDidFailLoading(DocumentLoader* dl, ResourceLoaderIdentifier identifier, const ResourceError& error)
{
    Frame* f = dl->frame();
    if (!f) {
        f = frame();
    }
    postResourceLoadEvent(f,
                          com_sun_webkit_LoadListenerClient_RESOURCE_FAILED,
                          identifier,
                          dl->responseMIMEType(),
                          0.0 /* progress */,
                          error.errorCode());
    removeRequestURL(f, identifier);
}

void FrameLoaderClientJava::dispatchDidFailProvisionalLoad(const ResourceError& error, WillContinueLoading,WillInternallyHandleFailure)
{
    ASSERT(frame());
    if (!frame()) {
        return;
    }

    auto* localFrame = dynamicDowncast<LocalFrame>(frame());
    DocumentLoader* dl = localFrame->loader().activeDocumentLoader();
    if (!dl) {
        return;
    }

    double progress = page()->progress().estimatedProgress();
    int state = error.isCancellation()
        ? com_sun_webkit_LoadListenerClient_LOAD_STOPPED
        : com_sun_webkit_LoadListenerClient_LOAD_FAILED;
    postLoadEvent(frame(), state,
                  dl->url().string(),
                  dl->responseMIMEType(),
                  progress,
                  error.errorCode());
}

void FrameLoaderClientJava::dispatchDidFailLoad(const ResourceError& error)
{
    dispatchDidFailProvisionalLoad(error, WillContinueLoading::No, WillInternallyHandleFailure::No);
}

// client-side redirection
void FrameLoaderClientJava::dispatchWillPerformClientRedirect(const URL&, double, WallTime, LockBackForwardList)
{
}

void FrameLoaderClientJava::dispatchDidReceiveTitle(const StringWithDirection&)
{
    double progress = page()->progress().estimatedProgress();
    auto* localFrame = dynamicDowncast<LocalFrame>(frame());
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_TITLE_RECEIVED,
                  localFrame->document()->url().string(),
                  localFrame->loader().documentLoader()->responseMIMEType(),
                  progress);
}

void FrameLoaderClientJava::dispatchDidReceiveIcon()
{
    // not called without IconDatabase support, so sending the notification
    // from dispatchDidLoadMainResource()
    /*
    Frame* f = page()->mainFrame();
    if (!f->loader() || !f->document()) {
        return;
    }

    double progress = page()->progress()->estimatedProgress();
    postLoadEvent(com_sun_webkit_LoadListenerClient_ICON_RECEIVED,
                  0, // request id
                  f->document()->url(),
                  f->loader()->documentLoader()->responseMIMEType(),
                  progress);
    */
}

void FrameLoaderClientJava::dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int)
{
    notImplemented();
}

void FrameLoaderClientJava::dispatchDidFinishDocumentLoad()
{
    if (!frame()->isMainFrame()) {
        // send the notification for the main frame only
        return;
    }

    double progress = page()->progress().estimatedProgress();
    auto* localFrame = dynamicDowncast<LocalFrame>(frame());

    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_DOCUMENT_AVAILABLE,
                  localFrame->document()->url().string(),
                  localFrame->loader().documentLoader()->responseMIMEType(),
                  progress);
}

void FrameLoaderClientJava::dispatchDidLoadMainResource(DocumentLoader* l)
{
    double progress = page()->progress().estimatedProgress();
    // send ICON_RECEIVED here instead of dispatchDidReceiveIcon(),
    // see comments in the method for details
    auto* localFrame = dynamicDowncast<LocalFrame>(frame());
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_ICON_RECEIVED,
                  localFrame->document()->url().string(),
                  l->responseMIMEType(),
                  progress);
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_CONTENT_RECEIVED,
                  l->responseURL().string(),
                  l->responseMIMEType(),
                  progress);
}

void FrameLoaderClientJava::dispatchDidFinishLoading(DocumentLoader* l, ResourceLoaderIdentifier identifier)
{
    postResourceLoadEvent(frame(),
                          com_sun_webkit_LoadListenerClient_RESOURCE_FINISHED,
                          identifier,
                          l->responseMIMEType(),
                          1.0 /* progress */);
    removeRequestURL(frame(), identifier);
}

void FrameLoaderClientJava::dispatchDidFinishLoad()
{
    double progress = page()->progress().estimatedProgress();
    auto* localFrame = dynamicDowncast<LocalFrame>(frame());
    postLoadEvent(frame(),
                  com_sun_webkit_LoadListenerClient_PAGE_FINISHED,
                  localFrame->document()->url().string(),
                  localFrame->loader().documentLoader()->responseMIMEType(),
                  progress);
}

void FrameLoaderClientJava::finishedLoading(DocumentLoader* dl)
{
    // This is necessary to create an empty document. See bug 634004.
    // However, we only want to do this if makeRepresentation has been called, to
    // match the behavior on the Mac.
    if (m_hasRepresentation)
        dl->writer().setEncoding(""_s, DocumentWriter::IsEncodingUserChosen::No);
}

void FrameLoaderClientJava::frameLoadCompleted()
{
    notImplemented();
}

void FrameLoaderClientJava::saveViewStateToItem(HistoryItem&)
{
    notImplemented();
}

void FrameLoaderClientJava::restoreViewState()
{
    notImplemented();
}

LocalFrame* FrameLoaderClientJava::dispatchCreatePage(const NavigationAction& action, NewFrameOpenerPolicy)
{
    Page* webPage = frame()->page();
    if (!webPage)
        return nullptr;
    Frame *f = frame();
    auto* localFrame = dynamicDowncast<LocalFrame>(f);
    RefPtr<Page> newPage = webPage->chrome().createWindow(*localFrame, { }, action);

    // createWindow can return null (e.g., popup blocker denies the window).
    if (!newPage)
        return 0;

    auto* frame = dynamicDowncast<LocalFrame>(newPage->mainFrame());
    return frame;
}

bool FrameLoaderClientJava::shouldGoToHistoryItem(HistoryItem&) const
{
    // FIXME: This is a very simple implementation. More sophisticated
    // implementation would delegate the decision to a PolicyDelegate.
    // See mac implementation for example.
    return true;
}

void FrameLoaderClientJava::didDisplayInsecureContent()
{
    notImplemented();
}

void FrameLoaderClientJava::didRunInsecureContent(SecurityOrigin&)
{
    notImplemented();
}

void FrameLoaderClientJava::makeRepresentation(DocumentLoader*)
{
    m_hasRepresentation = true;
}

void FrameLoaderClientJava::forceLayoutForNonHTML() { notImplemented(); }
void FrameLoaderClientJava::setCopiesOnScroll() { notImplemented(); }
void FrameLoaderClientJava::detachedFromParent2() { notImplemented(); }
void FrameLoaderClientJava::detachedFromParent3() { notImplemented(); }

void FrameLoaderClientJava::dispatchDidPushStateWithinPage()
{
    dispatchDidNavigateWithinPage();
}

void FrameLoaderClientJava::dispatchDidReplaceStateWithinPage()
{
    dispatchDidNavigateWithinPage();
}

void FrameLoaderClientJava::dispatchDidDispatchOnloadEvents() {notImplemented(); }
void FrameLoaderClientJava::dispatchDidPopStateWithinPage() { notImplemented(); }
void FrameLoaderClientJava::dispatchDidReceiveServerRedirectForProvisionalLoad() { notImplemented(); }
void FrameLoaderClientJava::dispatchDidCancelClientRedirect() { notImplemented(); }
void FrameLoaderClientJava::dispatchDidChangeLocationWithinPage() { notImplemented(); }
void FrameLoaderClientJava::dispatchWillClose() { notImplemented(); }
void FrameLoaderClientJava::dispatchDidCommitLoad(std::optional<HasInsecureContent>, std::optional<WebCore::UsedLegacyTLS>, std::optional<WasPrivateRelayed>)
{
    // TODO: Look at GTK version
    notImplemented();
}

void FrameLoaderClientJava::dispatchShow() { notImplemented(); }
void FrameLoaderClientJava::cancelPolicyCheck() { notImplemented(); }
void FrameLoaderClientJava::revertToProvisionalState(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientJava::willChangeTitle(DocumentLoader*) { notImplemented(); }
void FrameLoaderClientJava::didChangeTitle(DocumentLoader *l) { setTitle(l->title(), l->url()); }

bool FrameLoaderClientJava::canHandleRequest(const ResourceRequest& req) const
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    JLString urlJavaString(req.url().string().toJavaString(env));
    jboolean ret = env->CallStaticBooleanMethod(networkContextClass, canHandleURLMID, (jstring)urlJavaString);
    WTF::CheckAndClearException(env);

    return jbool_to_bool(ret);
}

bool FrameLoaderClientJava::canShowMIMEType(const String& mimeType) const
{
    //copy from QT implementation
    String type = mimeType.convertToLowercaseWithoutLocale();
    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return true;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return true;

    if (MIMETypeRegistry::isSupportedMediaMIMEType(type))
        return true;

#if 0 // PluginDatabase is disabled until we have Plugin system done.
    if (m_frame && m_frame->settings().arePluginsEnabled()
        && PluginDatabase::installedPlugins()->isMIMETypeRegistered(type))
        return true;
#endif

    return false;
}

bool FrameLoaderClientJava::canShowMIMETypeAsHTML(const String&) const
{
    notImplemented();
    return false;
}


bool FrameLoaderClientJava::representationExistsForURLScheme(StringView URLScheme) const
{
    notImplemented();
    return false;
}

String FrameLoaderClientJava::generatedMIMETypeForURLScheme(StringView URLScheme) const
{
    notImplemented();
    return String();
}

void FrameLoaderClientJava::provisionalLoadStarted()
{
    notImplemented();
}

void FrameLoaderClientJava::didFinishLoad()
{
    notImplemented();
}

void FrameLoaderClientJava::prepareForDataSourceReplacement()
{
    notImplemented();
}

void FrameLoaderClientJava::setTitle(const StringWithDirection&, const URL&)
{
    notImplemented();
}

bool FrameLoaderClientJava::dispatchDidLoadResourceFromMemoryCache(
    DocumentLoader*,
    const ResourceRequest&,
    const ResourceResponse&,
    int)
{
    notImplemented();
    return false;
}

ResourceError FrameLoaderClientJava::cancelledError(const ResourceRequest& request) const
{
    return ResourceError("Error"_s, -999, request.url(), "Request cancelled"_s);
}

ResourceError FrameLoaderClientJava::blockedError(const ResourceRequest& request) const
{
    using namespace FrameLoaderClientJavaInternal;
    return ResourceError("Error"_s, WebKitErrorCannotUseRestrictedPort, request.url(),
                         "Request blocked"_s);
}

ResourceError FrameLoaderClientJava::blockedByContentBlockerError(const ResourceRequest& request) const
{
    using namespace FrameLoaderClientJavaInternal;
    RELEASE_ASSERT_NOT_REACHED(); // Content Blockers are not enabled for WK1.
    return ResourceError("Error"_s, WebKitErrorCannotShowURL, request.url(),
                         "Cannot show URL"_s);
}

ResourceError FrameLoaderClientJava::cannotShowURLError(const ResourceRequest& request) const
{
    using namespace FrameLoaderClientJavaInternal;
    return ResourceError("Error"_s, WebKitErrorCannotShowURL, request.url(),
                         "Cannot show URL"_s);
}

ResourceError FrameLoaderClientJava::interruptedForPolicyChangeError(const ResourceRequest& request) const
{
    using namespace FrameLoaderClientJavaInternal;
    return ResourceError("Error"_s, WebKitErrorFrameLoadInterruptedByPolicyChange,
                         request.url(), "Frame load interrupted by policy change"_s);
}

ResourceError FrameLoaderClientJava::cannotShowMIMETypeError(const ResourceResponse& response) const
{
    using namespace FrameLoaderClientJavaInternal;
    return ResourceError("Error"_s, WebKitErrorCannotShowMIMEType, response.url(),
                         "Cannot show mimetype"_s);
}

ResourceError FrameLoaderClientJava::fileDoesNotExistError(const ResourceResponse& response) const
{
    return ResourceError("Error"_s, -998 /* ### */, response.url(),
                         "File does not exist"_s);
}

ResourceError FrameLoaderClientJava::httpsUpgradeRedirectLoopError(const ResourceRequest&) const
{
    ASSERT_NOT_REACHED();
    return {};
}

ResourceError FrameLoaderClientJava::pluginWillHandleLoadError(const ResourceResponse& response) const
{
    using namespace FrameLoaderClientJavaInternal;
    return ResourceError("Error"_s, WebKitErrorPluginWillHandleLoad, response.url(), "Loading is handled by the media engine"_s);
}

bool FrameLoaderClientJava::shouldFallBack(const ResourceError& error) const
{
    using namespace FrameLoaderClientJavaInternal;
    //Font fallback supported by Java Fonts internaly
    return !(error.isCancellation() || (error.errorCode() == WebKitErrorFrameLoadInterruptedByPolicyChange));
}

void FrameLoaderClientJava::didRestoreFromBackForwardCache()
{
    // FIXME: openjfx2.26 Raise Bug to track fwd / back cache
}

bool FrameLoaderClientJava::canCachePage() const
{
    return true;
}

void FrameLoaderClientJava::dispatchUnableToImplementPolicy(const ResourceError&)
{
    notImplemented();
}

void FrameLoaderClientJava::setMainDocumentError(
    DocumentLoader*,
    const ResourceError&)
{
//    if (m_pluginWidget) {
//        m_pluginWidget = 0;
//    }
    notImplemented();
}

void FrameLoaderClientJava::startDownload(const ResourceRequest&, const String& suggestedName, FromDownloadAttribute fromDownloadAttribute)

{
    notImplemented();
}

void FrameLoaderClientJava::updateGlobalHistory()
{
    notImplemented();
}

void FrameLoaderClientJava::updateGlobalHistoryRedirectLinks()
{
    notImplemented();
}

void FrameLoaderClientJava::dispatchDidClearWindowObjectInWorld(
    DOMWrapperWorld& world)
{
    using namespace FrameLoaderClientJavaInternal;
    JNIEnv* env = WTF::GetJavaEnv();
    initRefs(env);

    if (&world != &mainThreadNormalWorld()) {
        return;
    }

    Frame *f = frame();
    auto* localFrame = dynamicDowncast<LocalFrame>(f);
    JSGlobalContextRef context = toGlobalRef(localFrame->script().globalObject(
            mainThreadNormalWorld()));
    JSObjectRef windowObject = JSContextGetGlobalObject(context);

    env->CallVoidMethod(m_webPage, didClearWindowObjectMID,
            ptr_to_jlong(context), ptr_to_jlong(windowObject));
    WTF::CheckAndClearException(env);
}

void FrameLoaderClientJava::registerForIconNotification()
{
    //notImplemented();
}

void FrameLoaderClientJava::convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&)
{
    //notImplemented();
}

Ref<FrameNetworkingContext> FrameLoaderClientJava::createNetworkingContext()
{
    Frame *f = frame();
    auto* localFrame = dynamicDowncast<LocalFrame>(f);
    return FrameNetworkingContextJava::create(localFrame);
}


bool FrameLoaderClientJava::shouldUseCredentialStorage(
    DocumentLoader*,
    ResourceLoaderIdentifier)
{
    notImplemented();
    return false;
}

void FrameLoaderClientJava::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge& challenge)
{
    notImplemented();
    // If the ResourceLoadDelegate doesn't exist or fails to handle the call, we tell the ResourceHandle
    // to continue without credential - this is the best approximation of Mac behavior
    challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
}

void FrameLoaderClientJava::prefetchDNS(const String& hostname)
{
    WebCore::prefetchDNS(hostname);
}

void FrameLoaderClientJava::sendH2Ping(const URL& url,
    CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>&&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(makeUnexpected(WebCore::internalError(url)));
}

/*std::optional<PageIdentifier> FrameLoaderClientJava::pageID() const
{
    return std::nullopt;
}*/

void FrameLoaderClientJava::broadcastFrameRemovalToOtherProcesses()
{
    notImplemented();
}

ResourceError FrameLoaderClientJava::httpNavigationWithHTTPSOnlyError(const ResourceRequest&) const
{
    return {};
}
void FrameLoaderClientJava:: broadcastMainFrameURLChangeToOtherProcesses(const URL&)
{
    notImplemented();
}

void FrameLoaderClientJava::dispatchLoadEventToOwnerElementInAnotherProcess()
{
    notImplemented();
}
 void FrameLoaderClientJava::loadStorageAccessQuirksIfNeeded()
 {
    notImplemented();
 }

}
