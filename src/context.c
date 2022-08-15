//========================================================================
// GLFW 3.4 - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2016 Camilla Löwy <elmindreda@glfw.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================
// Please use C89 style variable declarations in this file because VS 2010
//========================================================================

#include "internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>


//////////////////////////////////////////////////////////////////////////
//////                       GLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Checks whether the desired context attributes are valid
//
// This function checks things like whether the specified client API version
// exists and whether all relevant options have supported and non-conflicting
// values
//
static GLFWbool isValidContextConfig(const _GLFWctxconfig* ctxconfig)
{
    if (ctxconfig->creationAPI != GLFW_NATIVE_CONTEXT_API &&
        ctxconfig->creationAPI != GLFW_EGL_CONTEXT_API &&
        ctxconfig->creationAPI != GLFW_OSMESA_CONTEXT_API)
    {
        _glfwInputError(GLFW_INVALID_ENUM,
                        "Invalid context creation API 0x%08X",
                        ctxconfig->creationAPI);
        return GLFW_FALSE;
    }

    if (ctxconfig->clientAPI != GLFW_NO_API &&
        ctxconfig->clientAPI != GLFW_OPENGL_API &&
        ctxconfig->clientAPI != GLFW_OPENGL_ES_API)
    {
        _glfwInputError(GLFW_INVALID_ENUM,
                        "Invalid client API 0x%08X",
                        ctxconfig->clientAPI);
        return GLFW_FALSE;
    }

    if (ctxconfig->share)
    {
        if (!ctxconfig->share->context)
        {
            _glfwInputError(GLFW_NO_WINDOW_CONTEXT, NULL);
            return GLFW_FALSE;
        }

        if (ctxconfig->creationAPI != ctxconfig->share->context->creationAPI)
        {
            _glfwInputError(GLFW_INVALID_ENUM,
                            "Context creation APIs do not match between contexts");
            return GLFW_FALSE;
        }
    }

    if (ctxconfig->clientAPI == GLFW_OPENGL_API)
    {
        if ((ctxconfig->major < 1 || ctxconfig->minor < 0) ||
            (ctxconfig->major == 1 && ctxconfig->minor > 5) ||
            (ctxconfig->major == 2 && ctxconfig->minor > 1) ||
            (ctxconfig->major == 3 && ctxconfig->minor > 3))
        {
            // OpenGL 1.0 is the smallest valid version
            // OpenGL 1.x series ended with version 1.5
            // OpenGL 2.x series ended with version 2.1
            // OpenGL 3.x series ended with version 3.3
            // For now, let everything else through

            _glfwInputError(GLFW_INVALID_VALUE,
                            "Invalid OpenGL version %i.%i",
                            ctxconfig->major, ctxconfig->minor);
            return GLFW_FALSE;
        }

        if (ctxconfig->profile)
        {
            if (ctxconfig->profile != GLFW_OPENGL_CORE_PROFILE &&
                ctxconfig->profile != GLFW_OPENGL_COMPAT_PROFILE)
            {
                _glfwInputError(GLFW_INVALID_ENUM,
                                "Invalid OpenGL profile 0x%08X",
                                ctxconfig->profile);
                return GLFW_FALSE;
            }

            if (ctxconfig->major <= 2 ||
                (ctxconfig->major == 3 && ctxconfig->minor < 2))
            {
                // Desktop OpenGL context profiles are only defined for version 3.2
                // and above

                _glfwInputError(GLFW_INVALID_VALUE,
                                "Context profiles are only defined for OpenGL version 3.2 and above");
                return GLFW_FALSE;
            }
        }

        if (ctxconfig->forward && ctxconfig->major <= 2)
        {
            // Forward-compatible contexts are only defined for OpenGL version 3.0 and above
            _glfwInputError(GLFW_INVALID_VALUE,
                            "Forward-compatibility is only defined for OpenGL version 3.0 and above");
            return GLFW_FALSE;
        }
    }
    else if (ctxconfig->clientAPI == GLFW_OPENGL_ES_API)
    {
        if (ctxconfig->major < 1 || ctxconfig->minor < 0 ||
            (ctxconfig->major == 1 && ctxconfig->minor > 1) ||
            (ctxconfig->major == 2 && ctxconfig->minor > 0))
        {
            // OpenGL ES 1.0 is the smallest valid version
            // OpenGL ES 1.x series ended with version 1.1
            // OpenGL ES 2.x series ended with version 2.0
            // For now, let everything else through

            _glfwInputError(GLFW_INVALID_VALUE,
                            "Invalid OpenGL ES version %i.%i",
                            ctxconfig->major, ctxconfig->minor);
            return GLFW_FALSE;
        }
    }

    if (ctxconfig->robustness)
    {
        if (ctxconfig->robustness != GLFW_NO_RESET_NOTIFICATION &&
            ctxconfig->robustness != GLFW_LOSE_CONTEXT_ON_RESET)
        {
            _glfwInputError(GLFW_INVALID_ENUM,
                            "Invalid context robustness mode 0x%08X",
                            ctxconfig->robustness);
            return GLFW_FALSE;
        }
    }

    if (ctxconfig->release)
    {
        if (ctxconfig->release != GLFW_RELEASE_BEHAVIOR_NONE &&
            ctxconfig->release != GLFW_RELEASE_BEHAVIOR_FLUSH)
        {
            _glfwInputError(GLFW_INVALID_ENUM,
                            "Invalid context release behavior 0x%08X",
                            ctxconfig->release);
            return GLFW_FALSE;
        }
    }

    return GLFW_TRUE;
}

// Chooses the framebuffer config that best matches the desired one
//
uint32_t _glfwCompareFBConfigs(const _GLFWfbconfig* desired, const _GLFWfbconfig* actual)
{
    unsigned int missing = 0, colorDiff = 0, extraDiff = 0;

    if (desired->doublebuffer != actual->doublebuffer)
        return UINT32_MAX;

    if (desired->stereo && !actual->stereo)
        return UINT32_MAX;

    // Count number of missing buffers
    {
        if (desired->alphaBits > 0 && actual->alphaBits == 0)
            missing++;

        if (desired->depthBits > 0 && actual->depthBits == 0)
            missing++;

        if (desired->stencilBits > 0 && actual->stencilBits == 0)
            missing++;

        if (desired->auxBuffers > 0 &&
            actual->auxBuffers < desired->auxBuffers)
        {
            missing += desired->auxBuffers - actual->auxBuffers;
        }

        if (desired->samples > 0 && actual->samples == 0)
        {
            // Technically, several multisampling buffers could be
            // involved, but that's a lower level implementation detail and
            // not important to us here, so we count them as one
            missing++;
        }

        if (desired->transparent != actual->transparent)
            missing++;
    }

    // These polynomials make many small channel size differences matter
    // less than one large channel size difference

    // Calculate color channel size difference value
    {
        if (desired->redBits != GLFW_DONT_CARE)
        {
            colorDiff += (desired->redBits - actual->redBits) *
                            (desired->redBits - actual->redBits);
        }

        if (desired->greenBits != GLFW_DONT_CARE)
        {
            colorDiff += (desired->greenBits - actual->greenBits) *
                            (desired->greenBits - actual->greenBits);
        }

        if (desired->blueBits != GLFW_DONT_CARE)
        {
            colorDiff += (desired->blueBits - actual->blueBits) *
                            (desired->blueBits - actual->blueBits);
        }
    }

    // Calculate non-color channel size difference value
    {
        if (desired->alphaBits != GLFW_DONT_CARE)
        {
            extraDiff += (desired->alphaBits - actual->alphaBits) *
                            (desired->alphaBits - actual->alphaBits);
        }

        if (desired->depthBits != GLFW_DONT_CARE)
        {
            extraDiff += (desired->depthBits - actual->depthBits) *
                            (desired->depthBits - actual->depthBits);
        }

        if (desired->stencilBits != GLFW_DONT_CARE)
        {
            extraDiff += (desired->stencilBits - actual->stencilBits) *
                            (desired->stencilBits - actual->stencilBits);
        }

        if (desired->accumRedBits != GLFW_DONT_CARE)
        {
            extraDiff += (desired->accumRedBits - actual->accumRedBits) *
                            (desired->accumRedBits - actual->accumRedBits);
        }

        if (desired->accumGreenBits != GLFW_DONT_CARE)
        {
            extraDiff += (desired->accumGreenBits - actual->accumGreenBits) *
                            (desired->accumGreenBits - actual->accumGreenBits);
        }

        if (desired->accumBlueBits != GLFW_DONT_CARE)
        {
            extraDiff += (desired->accumBlueBits - actual->accumBlueBits) *
                            (desired->accumBlueBits - actual->accumBlueBits);
        }

        if (desired->accumAlphaBits != GLFW_DONT_CARE)
        {
            extraDiff += (desired->accumAlphaBits - actual->accumAlphaBits) *
                            (desired->accumAlphaBits - actual->accumAlphaBits);
        }

        if (desired->samples != GLFW_DONT_CARE)
        {
            extraDiff += (desired->samples - actual->samples) *
                            (desired->samples - actual->samples);
        }

        if (desired->sRGB && !actual->sRGB)
            extraDiff++;
    }

    return ((uint32_t) _glfw_min(missing, 1023) << 20) +
           ((uint32_t) _glfw_min(colorDiff, 1023) << 10) +
           (uint32_t) _glfw_min(extraDiff, 1023);
}

// Retrieves the attributes of the current context
//
static GLFWbool refreshContextAttribs(_GLFWcontext* context,
                                      const _GLFWctxconfig* ctxconfig)
{
    int i;
    const char* version;
    const char* prefixes[] =
    {
        "OpenGL ES-CM ",
        "OpenGL ES-CL ",
        "OpenGL ES ",
        NULL
    };

    context->creationAPI = ctxconfig->creationAPI;
    context->clientAPI = GLFW_OPENGL_API;

    context->GetIntegerv = (PFNGLGETINTEGERVPROC)
        context->getProcAddress("glGetIntegerv");
    context->GetString = (PFNGLGETSTRINGPROC)
        context->getProcAddress("glGetString");
    if (!context->GetIntegerv || !context->GetString)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR, "Entry point retrieval is broken");
        return GLFW_FALSE;
    }

    version = (const char*) context->GetString(GL_VERSION);
    if (!version)
    {
        if (ctxconfig->clientAPI == GLFW_OPENGL_API)
        {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "OpenGL version string retrieval is broken");
        }
        else
        {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "OpenGL ES version string retrieval is broken");
        }

        return GLFW_FALSE;
    }

    for (i = 0;  prefixes[i];  i++)
    {
        const size_t length = strlen(prefixes[i]);

        if (strncmp(version, prefixes[i], length) == 0)
        {
            version += length;
            context->clientAPI = GLFW_OPENGL_ES_API;
            break;
        }
    }

    if (!sscanf(version, "%d.%d.%d",
                &context->major,
                &context->minor,
                &context->revision))
    {
        if (context->clientAPI == GLFW_OPENGL_API)
        {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "No version found in OpenGL version string");
        }
        else
        {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "No version found in OpenGL ES version string");
        }

        return GLFW_FALSE;
    }

    if (context->major < ctxconfig->major ||
        (context->major == ctxconfig->major && context->minor < ctxconfig->minor))
    {
        // The desired OpenGL version is greater than the actual version
        // This only happens if the machine lacks {GLX|WGL}_ARB_create_context
        // /and/ the user has requested an OpenGL version greater than 1.0

        // For API consistency, we emulate the behavior of the
        // {GLX|WGL}_ARB_create_context extension and fail here

        if (context->clientAPI == GLFW_OPENGL_API)
        {
            _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                            "Requested OpenGL version %i.%i, got version %i.%i",
                            ctxconfig->major, ctxconfig->minor,
                            context->major, context->minor);
        }
        else
        {
            _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                            "Requested OpenGL ES version %i.%i, got version %i.%i",
                            ctxconfig->major, ctxconfig->minor,
                            context->major, context->minor);
        }

        return GLFW_FALSE;
    }

    if (context->major >= 3)
    {
        // OpenGL 3.0+ uses a different function for extension string retrieval
        // We cache it here instead of in glfwExtensionSupported mostly to alert
        // users as early as possible that their build may be broken

        context->GetStringi = (PFNGLGETSTRINGIPROC)
            context->getProcAddress("glGetStringi");
        if (!context->GetStringi)
        {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "Entry point retrieval is broken");
            return GLFW_FALSE;
        }
    }

    if (context->clientAPI == GLFW_OPENGL_API)
    {
        // Read back context flags (OpenGL 3.0 and above)
        if (context->major >= 3)
        {
            GLint flags;
            context->GetIntegerv(GL_CONTEXT_FLAGS, &flags);

            if (flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT)
                context->forward = GLFW_TRUE;

            if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
                context->debug = GLFW_TRUE;
            else if (glfwExtensionSupported("GL_ARB_debug_output") &&
                     ctxconfig->debug)
            {
                // HACK: This is a workaround for older drivers (pre KHR_debug)
                //       not setting the debug bit in the context flags for
                //       debug contexts
                context->debug = GLFW_TRUE;
            }

            if (flags & GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR)
                context->noerror = GLFW_TRUE;
        }

        // Read back OpenGL context profile (OpenGL 3.2 and above)
        if (context->major >= 4 || (context->major == 3 && context->minor >= 2))
        {
            GLint mask;
            context->GetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);

            if (mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
                context->profile = GLFW_OPENGL_COMPAT_PROFILE;
            else if (mask & GL_CONTEXT_CORE_PROFILE_BIT)
                context->profile = GLFW_OPENGL_CORE_PROFILE;
            else if (glfwExtensionSupported("GL_ARB_compatibility"))
            {
                // HACK: This is a workaround for the compatibility profile bit
                //       not being set in the context flags if an OpenGL 3.2+
                //       context was created without having requested a specific
                //       version
                context->profile = GLFW_OPENGL_COMPAT_PROFILE;
            }
        }

        // Read back robustness strategy
        if (glfwExtensionSupported("GL_ARB_robustness"))
        {
            // NOTE: We avoid using the context flags for detection, as they are
            //       only present from 3.0 while the extension applies from 1.1

            GLint strategy;
            context->GetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB, &strategy);

            if (strategy == GL_LOSE_CONTEXT_ON_RESET_ARB)
                context->robustness = GLFW_LOSE_CONTEXT_ON_RESET;
            else if (strategy == GL_NO_RESET_NOTIFICATION_ARB)
                context->robustness = GLFW_NO_RESET_NOTIFICATION;
        }
    }
    else
    {
        // Read back robustness strategy
        if (glfwExtensionSupported("GL_EXT_robustness"))
        {
            // NOTE: The values of these constants match those of the OpenGL ARB
            //       one, so we can reuse them here

            GLint strategy;
            context->GetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB, &strategy);

            if (strategy == GL_LOSE_CONTEXT_ON_RESET_ARB)
                context->robustness = GLFW_LOSE_CONTEXT_ON_RESET;
            else if (strategy == GL_NO_RESET_NOTIFICATION_ARB)
                context->robustness = GLFW_NO_RESET_NOTIFICATION;
        }
    }

    if (glfwExtensionSupported("GL_KHR_context_flush_control"))
    {
        GLint behavior;
        context->GetIntegerv(GL_CONTEXT_RELEASE_BEHAVIOR, &behavior);

        if (behavior == GL_NONE)
            context->release = GLFW_RELEASE_BEHAVIOR_NONE;
        else if (behavior == GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH)
            context->release = GLFW_RELEASE_BEHAVIOR_FLUSH;
    }

    return GLFW_TRUE;
}

GLFWbool _glfwSetFBConfig(_GLFWwindow* window,
                          const _GLFWctxconfig* ctxconfig,
                          const _GLFWfbconfig* fbconfig)
{
    if (!isValidContextConfig(ctxconfig))
        return GLFW_FALSE;

    window->framebufferAPI = ctxconfig->creationAPI;
    window->doublebuffer = fbconfig->doublebuffer;

    switch (window->framebufferAPI)
    {
        case GLFW_NATIVE_CONTEXT_API:
        {
            if (!_glfw.platform.initContextCreation())
                return GLFW_FALSE;

            return _glfw.platform.setFBConfig(window, ctxconfig, fbconfig);
        }

        case GLFW_EGL_CONTEXT_API:
        {
            if (!_glfwInitEGL())
                return GLFW_FALSE;

            return _glfwSetFBConfigEGL(window, ctxconfig, fbconfig);
        }

        case GLFW_OSMESA_CONTEXT_API:
        {
            if (!_glfwInitOSMesa())
                return GLFW_FALSE;

            return _glfwSetFBConfigOSMesa(window, ctxconfig, fbconfig);
        }
    }

    assert(GLFW_FALSE);
    return GLFW_FALSE;
}

GLFWbool _glfwCreateFramebuffer(_GLFWwindow* window, const _GLFWfbconfig* fbconfig)
{
    assert(window->createFramebuffer != NULL);
    return window->createFramebuffer(window, fbconfig);
}

void _glfwDestroyFramebuffer(_GLFWwindow* window)
{
    if (window->destroyFramebuffer)
        window->destroyFramebuffer(window);
}

_GLFWcontext* _glfwCreateContext(_GLFWwindow* window, const _GLFWctxconfig* ctxconfig)
{
    _GLFWcontext* context;
    _GLFWwindow* previousWindow;
    _GLFWcontext* previousContext;

    assert(window->createContext != NULL);

    context = _glfw_calloc(1, sizeof(_GLFWcontext));

    if (!window->createContext(context, window, ctxconfig))
    {
        _glfwDestroyContext(context);
        return NULL;
    }

    previousContext = _glfwPlatformGetTls(&_glfw.contextSlot);
    previousWindow = _glfwPlatformGetTls(&_glfw.windowSlot);
    _glfwMakeCurrent(window, context);

    if (!refreshContextAttribs(context, ctxconfig))
    {
        _glfwMakeCurrent(previousWindow, previousContext);
        _glfwDestroyContext(context);
        return NULL;
    }

    // Clearing the front buffer to black to avoid garbage pixels left over from
    // previous uses of our bit of VRAM
    if (window->doublebuffer)
    {
        PFNGLCLEARPROC glClear = (PFNGLCLEARPROC) context->getProcAddress("glClear");

        glClear(GL_COLOR_BUFFER_BIT);
        window->swapBuffers(window);
    }

    _glfwMakeCurrent(previousWindow, previousContext);
    return context;
}

GLFWbool _glfwCreateWindowContext(_GLFWwindow* window, const _GLFWctxconfig* ctxconfig)
{
    assert(window != NULL);
    window->context = _glfwCreateContext(window, ctxconfig);
    return window->context != NULL;
}

void _glfwDestroyContext(_GLFWcontext* context)
{
    if (context == NULL)
        return;

    // The window's context must not be current on another thread when the
    // window is destroyed
    if (context == _glfwPlatformGetTls(&_glfw.contextSlot))
        _glfwMakeCurrent(NULL, NULL);

    if (context->destroy)
        context->destroy(context);

    _glfw_free(context);
}

// Searches an extension string for the specified extension
//
GLFWbool _glfwStringInExtensionString(const char* string, const char* extensions)
{
    const char* start = extensions;

    for (;;)
    {
        const char* where;
        const char* terminator;

        where = strstr(start, string);
        if (!where)
            return GLFW_FALSE;

        terminator = where + strlen(string);
        if (where == start || *(where - 1) == ' ')
        {
            if (*terminator == ' ' || *terminator == '\0')
                break;
        }

        start = terminator;
    }

    return GLFW_TRUE;
}

void _glfwMakeCurrent(_GLFWwindow* window, _GLFWcontext* context)
{
    _GLFWcontext* previous = _glfwPlatformGetTls(&_glfw.contextSlot);
    if (previous)
    {
        if (!context || context->creationAPI != previous->creationAPI)
            previous->makeCurrent(NULL, NULL);
    }

    if (context)
        context->makeCurrent(window, context);
}


//////////////////////////////////////////////////////////////////////////
//////                        GLFW public API                       //////
//////////////////////////////////////////////////////////////////////////

GLFWAPI void glfwMakeContextCurrent(GLFWwindow* handle)
{
    _GLFWwindow* window = (_GLFWwindow*) handle;

    _GLFW_REQUIRE_INIT();

    if (window)
    {
        if (!window->context)
        {
            _glfwInputError(GLFW_NO_WINDOW_CONTEXT,
                            "Cannot make current with a window that has no OpenGL or OpenGL ES context");
            return;
        }

        _glfwMakeCurrent(window, window->context);
    }
    else
        _glfwMakeCurrent(NULL, NULL);
}

GLFWAPI GLFWwindow* glfwGetCurrentContext(void)
{
    _GLFW_REQUIRE_INIT_OR_RETURN(NULL);
    return _glfwPlatformGetTls(&_glfw.windowSlot);
}

GLFWAPI void glfwSwapBuffers(GLFWwindow* handle)
{
    _GLFWwindow* window = (_GLFWwindow*) handle;
    assert(window != NULL);

    _GLFW_REQUIRE_INIT();

    if (window->framebufferAPI == GLFW_NO_API)
    {
        _glfwInputError(GLFW_NO_WINDOW_CONTEXT,
                        "Cannot swap buffers of a window that has no OpenGL or OpenGL ES context");
        return;
    }

    window->swapBuffers(window);
}

GLFWAPI void glfwSwapInterval(int interval)
{
    _GLFWcontext* context;

    _GLFW_REQUIRE_INIT();

    context = _glfwPlatformGetTls(&_glfw.contextSlot);
    if (!context)
    {
        _glfwInputError(GLFW_NO_CURRENT_CONTEXT,
                        "Cannot set swap interval without a current OpenGL or OpenGL ES context");
        return;
    }

    context->swapInterval(interval);
}

GLFWAPI int glfwExtensionSupported(const char* extension)
{
    _GLFWcontext* context;
    assert(extension != NULL);

    _GLFW_REQUIRE_INIT_OR_RETURN(GLFW_FALSE);

    context = _glfwPlatformGetTls(&_glfw.contextSlot);
    if (!context)
    {
        _glfwInputError(GLFW_NO_CURRENT_CONTEXT,
                        "Cannot query extension without a current OpenGL or OpenGL ES context");
        return GLFW_FALSE;
    }

    if (*extension == '\0')
    {
        _glfwInputError(GLFW_INVALID_VALUE, "Extension name cannot be an empty string");
        return GLFW_FALSE;
    }

    if (context->major >= 3)
    {
        int i;
        GLint count;

        // Check if extension is in the modern OpenGL extensions string list

        context->GetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (i = 0;  i < count;  i++)
        {
            const char* en = (const char*) context->GetStringi(GL_EXTENSIONS, i);
            if (!en)
            {
                _glfwInputError(GLFW_PLATFORM_ERROR,
                                "Extension string retrieval is broken");
                return GLFW_FALSE;
            }

            if (strcmp(en, extension) == 0)
                return GLFW_TRUE;
        }
    }
    else
    {
        // Check if extension is in the old style OpenGL extensions string

        const char* extensions = (const char*) context->GetString(GL_EXTENSIONS);
        if (!extensions)
        {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "Extension string retrieval is broken");
            return GLFW_FALSE;
        }

        if (_glfwStringInExtensionString(extension, extensions))
            return GLFW_TRUE;
    }

    // Check if extension is in the platform-specific string
    return context->extensionSupported(extension);
}

GLFWAPI GLFWglproc glfwGetProcAddress(const char* procname)
{
    _GLFWcontext* context;
    assert(procname != NULL);

    _GLFW_REQUIRE_INIT_OR_RETURN(NULL);

    context = _glfwPlatformGetTls(&_glfw.contextSlot);
    if (!context)
    {
        _glfwInputError(GLFW_NO_CURRENT_CONTEXT,
                        "Cannot query entry point without a current OpenGL or OpenGL ES context");
        return NULL;
    }

    return context->getProcAddress(procname);
}

