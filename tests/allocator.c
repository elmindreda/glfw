//========================================================================
// Custom heap allocator test
// Copyright (c) Camilla Löwy <elmindreda@glfw.org>
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

#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static size_t total = 0;
static size_t current = 0;
static size_t maximum = 0;

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void* allocate(size_t size)
{
    total += size;
    current += size;
    if (current > maximum)
        maximum = current;
    printf("allocate %zu bytes (current %zu maximum %zu total %zu)\n", size, current, maximum, total);
    size_t* real = malloc(size + sizeof(size_t));
    assert(real);
    *real = size;
    return real + 1;
}

static void deallocate(void* pointer)
{
    size_t* real = (size_t*) pointer - 1;
    current -= *real;
    printf("deallocate %zu bytes (current %zu maximum %zu total %zu)\n", *real, current, maximum, total);
    free(real);
}

static void* reallocate(void* pointer, size_t size)
{
    size_t* real = (size_t*) pointer - 1;
    total += size;
    current += size - *real;
    if (current > maximum)
        maximum = current;
    printf("reallocate %zu bytes to %zu bytes (current %zu maximum %zu total %zu)\n", *real, size, current, maximum, total);
    real = realloc(real, size + sizeof(size_t));
    assert(real);
    *real = size;
    return real + 1;
}

int main(void)
{
    GLFWwindow* window;

    const GLFWallocator allocator =
    {
        .allocate = allocate,
        .deallocate = deallocate,
        .reallocate = reallocate
    };

    glfwSetErrorCallback(error_callback);
    glfwInitAllocator(&allocator);

    if (!glfwInit())
        exit(EXIT_FAILURE);

    window = glfwCreateWindow(400, 400, "Some kind of window title", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(1);

    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
        glfwWaitEvents();
    }

    glfwTerminate();
    exit(EXIT_SUCCESS);
}

