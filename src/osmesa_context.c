//========================================================================
// GLFW 3.4 OSMesa - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2016 Google Inc.
// Copyright (c) 2016-2017 Camilla Löwy <elmindreda@glfw.org>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "internal.h"


static void makeCurrentOSMesa(_GLFWwindow* window, _GLFWcontext* context)
{
    if (context)
    {
        int width, height;
        _glfw.platform.getFramebufferSize(window, &width, &height);

        // Check to see if we need to allocate a new buffer
        if ((window->osmesa.buffer == NULL) ||
            (width != window->osmesa.width) ||
            (height != window->osmesa.height))
        {
            _glfw_free(window->osmesa.buffer);

            // Allocate the new buffer (width * height * 8-bit RGBA)
            window->osmesa.buffer = _glfw_calloc(4, (size_t) width * height);
            window->osmesa.width  = width;
            window->osmesa.height = height;
        }

        if (!OSMesaMakeCurrent(context->osmesa.handle,
                               window->osmesa.buffer,
                               GL_UNSIGNED_BYTE,
                               width, height))
        {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "OSMesa: Failed to make context current");
            return;
        }
    }

    _glfwPlatformSetTls(&_glfw.windowSlot, window);
    _glfwPlatformSetTls(&_glfw.contextSlot, context);
}

static GLFWglproc getProcAddressOSMesa(const char* procname)
{
    return (GLFWglproc) OSMesaGetProcAddress(procname);
}

static void destroyContextOSMesa(_GLFWcontext* context)
{
    if (context->osmesa.handle)
    {
        OSMesaDestroyContext(context->osmesa.handle);
        context->osmesa.handle = NULL;
    }
}

static void swapBuffersOSMesa(_GLFWwindow* window)
{
    // No double buffering on OSMesa
}

static void swapIntervalOSMesa(int interval)
{
    // No swap interval on OSMesa
}

static int extensionSupportedOSMesa(const char* extension)
{
    // OSMesa does not have extensions
    return GLFW_FALSE;
}

static GLFWbool createFramebufferOSMesa(_GLFWwindow* window, const _GLFWfbconfig* fbconfig)
{
    return GLFW_TRUE;
}

static void destroyFramebufferOSMesa(_GLFWwindow* window)
{
    if (window->osmesa.buffer)
    {
        _glfw_free(window->osmesa.buffer);
        window->osmesa.width = 0;
        window->osmesa.height = 0;
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       GLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

GLFWbool _glfwInitOSMesa(void)
{
    int i;
    const char* sonames[] =
    {
#if defined(_GLFW_OSMESA_LIBRARY)
        _GLFW_OSMESA_LIBRARY,
#elif defined(_WIN32)
        "libOSMesa.dll",
        "OSMesa.dll",
#elif defined(__APPLE__)
        "libOSMesa.8.dylib",
#elif defined(__CYGWIN__)
        "libOSMesa-8.so",
#elif defined(__OpenBSD__) || defined(__NetBSD__)
        "libOSMesa.so",
#else
        "libOSMesa.so.8",
        "libOSMesa.so.6",
#endif
        NULL
    };

    if (_glfw.osmesa.handle)
        return GLFW_TRUE;

    for (i = 0;  sonames[i];  i++)
    {
        _glfw.osmesa.handle = _glfwPlatformLoadModule(sonames[i]);
        if (_glfw.osmesa.handle)
            break;
    }

    if (!_glfw.osmesa.handle)
    {
        _glfwInputError(GLFW_API_UNAVAILABLE, "OSMesa: Library not found");
        return GLFW_FALSE;
    }

    _glfw.osmesa.CreateContextExt = (PFN_OSMesaCreateContextExt)
        _glfwPlatformGetModuleSymbol(_glfw.osmesa.handle, "OSMesaCreateContextExt");
    _glfw.osmesa.CreateContextAttribs = (PFN_OSMesaCreateContextAttribs)
        _glfwPlatformGetModuleSymbol(_glfw.osmesa.handle, "OSMesaCreateContextAttribs");
    _glfw.osmesa.DestroyContext = (PFN_OSMesaDestroyContext)
        _glfwPlatformGetModuleSymbol(_glfw.osmesa.handle, "OSMesaDestroyContext");
    _glfw.osmesa.MakeCurrent = (PFN_OSMesaMakeCurrent)
        _glfwPlatformGetModuleSymbol(_glfw.osmesa.handle, "OSMesaMakeCurrent");
    _glfw.osmesa.GetDepthBuffer = (PFN_OSMesaGetDepthBuffer)
        _glfwPlatformGetModuleSymbol(_glfw.osmesa.handle, "OSMesaGetDepthBuffer");
    _glfw.osmesa.GetProcAddress = (PFN_OSMesaGetProcAddress)
        _glfwPlatformGetModuleSymbol(_glfw.osmesa.handle, "OSMesaGetProcAddress");

    if (!_glfw.osmesa.CreateContextExt ||
        !_glfw.osmesa.DestroyContext ||
        !_glfw.osmesa.MakeCurrent ||
        !_glfw.osmesa.GetDepthBuffer ||
        !_glfw.osmesa.GetProcAddress)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "OSMesa: Failed to load required entry points");

        _glfwTerminateOSMesa();
        return GLFW_FALSE;
    }

    return GLFW_TRUE;
}

void _glfwTerminateOSMesa(void)
{
    if (_glfw.osmesa.handle)
    {
        _glfwPlatformFreeModule(_glfw.osmesa.handle);
        _glfw.osmesa.handle = NULL;
    }
}

#define SET_ATTRIB(a, v) \
{ \
    assert(((size_t) index + 1) < sizeof(attribs) / sizeof(attribs[0])); \
    attribs[index++] = a; \
    attribs[index++] = v; \
}

static GLFWbool createContextOSMesa(_GLFWcontext* context,
                                    const _GLFWwindow* window,
                                    const _GLFWctxconfig* ctxconfig)
{
    OSMesaContext share = NULL;

    if (ctxconfig->clientAPI == GLFW_OPENGL_ES_API)
    {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "OSMesa: OpenGL ES is not available on OSMesa");
        return GLFW_FALSE;
    }

    if (ctxconfig->share)
        share = ctxconfig->share->context->osmesa.handle;

    if (OSMesaCreateContextAttribs)
    {
        int index = 0, attribs[40];

        SET_ATTRIB(OSMESA_FORMAT, OSMESA_RGBA);
        SET_ATTRIB(OSMESA_DEPTH_BITS, window->osmesa.depthBits);
        SET_ATTRIB(OSMESA_STENCIL_BITS, window->osmesa.stencilBits);
        SET_ATTRIB(OSMESA_ACCUM_BITS, window->osmesa.accumBits);

        if (ctxconfig->profile == GLFW_OPENGL_CORE_PROFILE)
        {
            SET_ATTRIB(OSMESA_PROFILE, OSMESA_CORE_PROFILE);
        }
        else if (ctxconfig->profile == GLFW_OPENGL_COMPAT_PROFILE)
        {
            SET_ATTRIB(OSMESA_PROFILE, OSMESA_COMPAT_PROFILE);
        }

        if (ctxconfig->major != 1 || ctxconfig->minor != 0)
        {
            SET_ATTRIB(OSMESA_CONTEXT_MAJOR_VERSION, ctxconfig->major);
            SET_ATTRIB(OSMESA_CONTEXT_MINOR_VERSION, ctxconfig->minor);
        }

        if (ctxconfig->forward)
        {
            _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                            "OSMesa: Forward-compatible contexts not supported");
            return GLFW_FALSE;
        }

        SET_ATTRIB(0, 0);

        context->osmesa.handle = OSMesaCreateContextAttribs(attribs, share);
    }
    else
    {
        if (ctxconfig->profile)
        {
            _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                            "OSMesa: OpenGL profiles unavailable");
            return GLFW_FALSE;
        }

        context->osmesa.handle =
            OSMesaCreateContextExt(OSMESA_RGBA,
                                   window->osmesa.depthBits,
                                   window->osmesa.stencilBits,
                                   window->osmesa.accumBits,
                                   share);
    }

    if (context->osmesa.handle == NULL)
    {
        _glfwInputError(GLFW_VERSION_UNAVAILABLE,
                        "OSMesa: Failed to create context");
        return GLFW_FALSE;
    }

    context->makeCurrent = makeCurrentOSMesa;
    context->swapInterval = swapIntervalOSMesa;
    context->extensionSupported = extensionSupportedOSMesa;
    context->getProcAddress = getProcAddressOSMesa;
    context->destroy = destroyContextOSMesa;

    return GLFW_TRUE;
}

#undef SET_ATTRIB

GLFWbool _glfwSetFBConfigOSMesa(_GLFWwindow* window,
                                const _GLFWctxconfig* ctxconfig,
                                const _GLFWfbconfig* fbconfig)
{
    window->osmesa.depthBits = fbconfig->depthBits;
    window->osmesa.stencilBits = fbconfig->stencilBits;
    window->osmesa.accumBits = fbconfig->accumRedBits +
                               fbconfig->accumGreenBits +
                               fbconfig->accumBlueBits +
                               fbconfig->accumAlphaBits;

    window->createFramebuffer = createFramebufferOSMesa;
    window->destroyFramebuffer = destroyFramebufferOSMesa;
    window->createContext = createContextOSMesa;
    window->swapBuffers = swapBuffersOSMesa;

    return GLFW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                        GLFW native API                       //////
//////////////////////////////////////////////////////////////////////////

GLFWAPI int glfwGetOSMesaColorBuffer(GLFWwindow* handle, int* width,
                                     int* height, int* format, void** buffer)
{
    _GLFWwindow* window = (_GLFWwindow*) handle;
    assert(window != NULL);

    _GLFW_REQUIRE_INIT_OR_RETURN(GLFW_FALSE);

    if (window->framebufferAPI != GLFW_OSMESA_CONTEXT_API)
    {
        _glfwInputError(GLFW_NO_WINDOW_CONTEXT, NULL);
        return GLFW_FALSE;
    }

    if (width)
        *width = window->osmesa.width;
    if (height)
        *height = window->osmesa.height;
    if (format)
        *format = GL_UNSIGNED_BYTE;
    if (buffer)
        *buffer = window->osmesa.buffer;

    return GLFW_TRUE;
}

GLFWAPI int glfwGetOSMesaDepthBuffer(GLFWwindow* handle,
                                     int* width, int* height,
                                     int* bytesPerValue,
                                     void** buffer)
{
    void* mesaBuffer;
    GLint mesaWidth, mesaHeight, mesaBytes;
    _GLFWwindow* window = (_GLFWwindow*) handle;
    assert(window != NULL);

    _GLFW_REQUIRE_INIT_OR_RETURN(GLFW_FALSE);

    if (!window->context || window->context->creationAPI != GLFW_OSMESA_CONTEXT_API)
    {
        _glfwInputError(GLFW_NO_WINDOW_CONTEXT, NULL);
        return GLFW_FALSE;
    }

    if (!OSMesaGetDepthBuffer(window->context->osmesa.handle,
                              &mesaWidth, &mesaHeight,
                              &mesaBytes, &mesaBuffer))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "OSMesa: Failed to retrieve depth buffer");
        return GLFW_FALSE;
    }

    if (width)
        *width = mesaWidth;
    if (height)
        *height = mesaHeight;
    if (bytesPerValue)
        *bytesPerValue = mesaBytes;
    if (buffer)
        *buffer = mesaBuffer;

    return GLFW_TRUE;
}

GLFWAPI OSMesaContext glfwGetOSMesaContext(GLFWwindow* handle)
{
    _GLFWwindow* window = (_GLFWwindow*) handle;
    _GLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (!window->context || window->context->creationAPI != GLFW_OSMESA_CONTEXT_API)
    {
        _glfwInputError(GLFW_NO_WINDOW_CONTEXT, NULL);
        return NULL;
    }

    return window->context->osmesa.handle;
}

