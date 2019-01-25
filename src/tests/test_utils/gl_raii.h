//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// gl_raii:
//   Helper methods for containing GL objects like buffers and textures.

#ifndef ANGLE_TESTS_GL_RAII_H_
#define ANGLE_TESTS_GL_RAII_H_

#include <functional>

#include "util/shader_utils.h"

namespace angle
{

// This is a bit of hack to work around a bug in MSVS intellisense, and make it very easy to
// use the correct function pointer type without worrying about the various definitions of
// GL_APICALL.
using GLGen    = decltype(glGenBuffers);
using GLDelete = decltype(glDeleteBuffers);

class GLWrapper : angle::NonCopyable
{
  public:
    GLWrapper(GLGen *genFunc, GLDelete *deleteFunc) : mGenFunc(genFunc), mDeleteFunc(deleteFunc) {}
    ~GLWrapper() { (*mDeleteFunc)(1, &mHandle); }

    // The move-constructor and move-assignment operators are necessary so that the data within a
    // GLWrapper object can be relocated.
    GLWrapper(GLWrapper &&rht) : mHandle(rht.mHandle) { rht.mHandle = 0u; }
    GLWrapper &operator=(GLWrapper &&rht)
    {
        if (this != &rht)
        {
            std::swap(mHandle, rht.mHandle);
        }
        return *this;
    }

    void reset()
    {
        if (mHandle != 0u)
        {
            (*mDeleteFunc)(1, &mHandle);
            mHandle = 0u;
        }
    }

    GLuint get()
    {
        if (!mHandle)
        {
            (*mGenFunc)(1, &mHandle);
        }
        return mHandle;
    }

    operator GLuint() { return get(); }

  private:
    GLGen *mGenFunc;
    GLDelete *mDeleteFunc;
    GLuint mHandle = 0u;
};

class GLVertexArray : public GLWrapper
{
  public:
    GLVertexArray() : GLWrapper(&glGenVertexArrays, &glDeleteVertexArrays) {}
};
class GLBuffer : public GLWrapper
{
  public:
    GLBuffer() : GLWrapper(&glGenBuffers, &glDeleteBuffers) {}
};
class GLTexture : public GLWrapper
{
  public:
    GLTexture() : GLWrapper(&glGenTextures, &glDeleteTextures) {}
};
class GLFramebuffer : public GLWrapper
{
  public:
    GLFramebuffer() : GLWrapper(&glGenFramebuffers, &glDeleteFramebuffers) {}
};
class GLRenderbuffer : public GLWrapper
{
  public:
    GLRenderbuffer() : GLWrapper(&glGenRenderbuffers, &glDeleteRenderbuffers) {}
};
class GLSampler : public GLWrapper
{
  public:
    GLSampler() : GLWrapper(&glGenSamplers, &glDeleteSamplers) {}
};
class GLTransformFeedback : public GLWrapper
{
  public:
    GLTransformFeedback() : GLWrapper(&glGenTransformFeedbacks, &glDeleteTransformFeedbacks) {}
};
class GLProgramPipeline : public GLWrapper
{
  public:
    GLProgramPipeline() : GLWrapper(&glGenProgramPipelines, &glDeleteProgramPipelines) {}
};
class GLQueryEXT : public GLWrapper
{
  public:
    GLQueryEXT() : GLWrapper(&glGenQueriesEXT, &glDeleteQueriesEXT) {}
};

class GLShader : angle::NonCopyable
{
  public:
    GLShader() = delete;
    explicit GLShader(GLenum shaderType) { mHandle = glCreateShader(shaderType); }

    ~GLShader() { glDeleteShader(mHandle); }

    GLuint get() { return mHandle; }

    operator GLuint() { return get(); }

  private:
    GLuint mHandle;
};

// Prefer ANGLE_GL_PROGRAM for local variables.
class GLProgram
{
  public:
    GLProgram() : mHandle(0) {}

    ~GLProgram() { glDeleteProgram(mHandle); }

    void makeEmpty() { mHandle = glCreateProgram(); }

    void makeCompute(const char *computeShader) { mHandle = CompileComputeProgram(computeShader); }

    void makeRaster(const char *vertexShader, const char *fragmentShader)
    {
        mHandle = CompileProgram(vertexShader, fragmentShader);
    }

    void makeRaster(const char *vertexShader,
                    const char *geometryShader,
                    const char *fragmentShader)
    {
        mHandle = CompileProgramWithGS(vertexShader, geometryShader, fragmentShader);
    }

    void makeRasterWithTransformFeedback(const char *vertexShader,
                                         const char *fragmentShader,
                                         const std::vector<std::string> &tfVaryings,
                                         GLenum bufferMode)
    {
        mHandle = CompileProgramWithTransformFeedback(vertexShader, fragmentShader, tfVaryings,
                                                      bufferMode);
    }

    void makeBinaryOES(const std::vector<uint8_t> &binary, GLenum binaryFormat)
    {
        mHandle = LoadBinaryProgramOES(binary, binaryFormat);
    }

    void makeBinaryES3(const std::vector<uint8_t> &binary, GLenum binaryFormat)
    {
        mHandle = LoadBinaryProgramES3(binary, binaryFormat);
    }

    bool valid() const { return mHandle != 0; }

    GLuint get() { return mHandle; }

    operator GLuint() { return get(); }

  private:
    GLuint mHandle;
};

#define ANGLE_GL_EMPTY_PROGRAM(name) \
    GLProgram name;                  \
    name.makeEmpty();                \
    ASSERT_TRUE(name.valid())

#define ANGLE_GL_PROGRAM(name, vertex, fragment) \
    GLProgram name;                              \
    name.makeRaster(vertex, fragment);           \
    ASSERT_TRUE(name.valid())

#define ANGLE_GL_PROGRAM_WITH_GS(name, vertex, geometry, fragment) \
    GLProgram name;                                                \
    name.makeRaster(vertex, geometry, fragment);                   \
    ASSERT_TRUE(name.valid())

#define ANGLE_GL_PROGRAM_TRANSFORM_FEEDBACK(name, vertex, fragment, tfVaryings, bufferMode) \
    GLProgram name;                                                                         \
    name.makeRasterWithTransformFeedback(vertex, fragment, tfVaryings, bufferMode);         \
    ASSERT_TRUE(name.valid())

#define ANGLE_GL_COMPUTE_PROGRAM(name, compute) \
    GLProgram name;                             \
    name.makeCompute(compute);                  \
    ASSERT_TRUE(name.valid())

#define ANGLE_GL_BINARY_OES_PROGRAM(name, binary, binaryFormat) \
    GLProgram name;                                             \
    name.makeBinaryOES(binary, binaryFormat);                   \
    ASSERT_TRUE(name.valid())

#define ANGLE_GL_BINARY_ES3_PROGRAM(name, binary, binaryFormat) \
    GLProgram name;                                             \
    name.makeBinaryES3(binary, binaryFormat);                   \
    ASSERT_TRUE(name.valid())

}  // namespace angle

#endif  // ANGLE_TESTS_GL_RAII_H_
