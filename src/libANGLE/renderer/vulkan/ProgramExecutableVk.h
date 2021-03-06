//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramExecutableVk.h: Collects the information and interfaces common to both ProgramVks and
// ProgramPipelineVks in order to execute/draw with either.

#ifndef LIBANGLE_RENDERER_VULKAN_PROGRAMEXECUTABLEVK_H_
#define LIBANGLE_RENDERER_VULKAN_PROGRAMEXECUTABLEVK_H_

#include "common/bitset_utils.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/InfoLog.h"
#include "libANGLE/renderer/glslang_wrapper_utils.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{

class ShaderInfo final : angle::NonCopyable
{
  public:
    ShaderInfo();
    ~ShaderInfo();

    angle::Result initShaders(ContextVk *contextVk,
                              const gl::ShaderBitSet &linkedShaderStages,
                              const gl::ShaderMap<std::string> &shaderSources);
    void release(ContextVk *contextVk);

    ANGLE_INLINE bool valid() const { return mIsInitialized; }

    const gl::ShaderMap<SpirvBlob> &getSpirvBlobs() const { return mSpirvBlobs; }

    // Save and load implementation for GLES Program Binary support.
    void load(gl::BinaryInputStream *stream);
    void save(gl::BinaryOutputStream *stream);

  private:
    gl::ShaderMap<SpirvBlob> mSpirvBlobs;
    bool mIsInitialized = false;
};

enum class ProgramTransformOption : uint8_t
{
    EnableLineRasterEmulation            = 0,
    RemoveEarlyFragmentTestsOptimization = 1,
    EnumCount                            = 2,
    PermutationCount                     = 4,
};
using ProgramTransformOptionBits = angle::PackedEnumBitSet<ProgramTransformOption, uint8_t>;

class ProgramInfo final : angle::NonCopyable
{
  public:
    ProgramInfo();
    ~ProgramInfo();

    angle::Result initProgram(ContextVk *contextVk,
                              const gl::ShaderType shaderType,
                              const ShaderInfo &shaderInfo,
                              ProgramTransformOptionBits optionBits,
                              ProgramExecutableVk *executableVk);
    void release(ContextVk *contextVk);

    ANGLE_INLINE bool valid(const gl::ShaderType shaderType) const
    {
        return mProgramHelper.valid(shaderType);
    }

    vk::ShaderProgramHelper *getShaderProgram() { return &mProgramHelper; }

  private:
    vk::ShaderProgramHelper mProgramHelper;
    gl::ShaderMap<vk::RefCounted<vk::ShaderAndSerial>> mShaders;
};

// State for the default uniform blocks.
struct DefaultUniformBlock final : private angle::NonCopyable
{
    DefaultUniformBlock();
    ~DefaultUniformBlock();

    // Shadow copies of the shader uniform data.
    angle::MemoryBuffer uniformData;

    // Since the default blocks are laid out in std140, this tells us where to write on a call
    // to a setUniform method. They are arranged in uniform location order.
    std::vector<sh::BlockMemberInfo> uniformLayout;
};

class ProgramExecutableVk
{
  public:
    ProgramExecutableVk();
    virtual ~ProgramExecutableVk();

    void reset(ContextVk *contextVk);

    void save(gl::BinaryOutputStream *stream);
    std::unique_ptr<rx::LinkEvent> load(gl::BinaryInputStream *stream);

    void clearVariableInfoMap();
    ShaderMapInterfaceVariableInfoMap &getShaderInterfaceVariableInfoMap()
    {
        return mVariableInfoMap;
    }

    ProgramVk *getShaderProgram(const gl::State &glState, gl::ShaderType shaderType) const;

    void fillProgramStateMap(const ContextVk *contextVk,
                             gl::ShaderMap<const gl::ProgramState *> *programStatesOut);
    const gl::ProgramExecutable &getGlExecutable();

    ProgramInfo &getGraphicsDefaultProgramInfo() { return mGraphicsProgramInfos[0]; }
    ProgramInfo &getGraphicsProgramInfo(ProgramTransformOptionBits optionBits)
    {
        return mGraphicsProgramInfos[optionBits.to_ulong()];
    }
    ProgramInfo &getComputeProgramInfo() { return mComputeProgramInfo; }
    vk::BufferSerial getCurrentDefaultUniformBufferSerial() const
    {
        return mCurrentDefaultUniformBufferSerial;
    }

    angle::Result getGraphicsPipeline(ContextVk *contextVk,
                                      gl::PrimitiveMode mode,
                                      const vk::GraphicsPipelineDesc &desc,
                                      const gl::AttributesMask &activeAttribLocations,
                                      const vk::GraphicsPipelineDesc **descPtrOut,
                                      vk::PipelineHelper **pipelineOut);

    angle::Result getComputePipeline(ContextVk *contextVk, vk::PipelineAndSerial **pipelineOut);

    const vk::PipelineLayout &getPipelineLayout() const { return mPipelineLayout.get(); }
    angle::Result createPipelineLayout(const gl::Context *glContext,
                                       gl::ActiveTextureArray<vk::TextureUnit> *activeTextures);

    angle::Result updateTexturesDescriptorSet(ContextVk *contextVk);
    angle::Result updateShaderResourcesDescriptorSet(ContextVk *contextVk,
                                                     vk::ResourceUseList *resourceUseList,
                                                     vk::CommandBufferHelper *commandBufferHelper);
    angle::Result updateTransformFeedbackDescriptorSet(
        const gl::ProgramState &programState,
        gl::ShaderMap<DefaultUniformBlock> &defaultUniformBlocks,
        vk::BufferHelper *defaultUniformBuffer,
        ContextVk *contextVk,
        const vk::UniformsAndXfbDesc &xfbBufferDesc);

    angle::Result updateDescriptorSets(ContextVk *contextVk, vk::CommandBuffer *commandBuffer);

    void updateEarlyFragmentTestsOptimization(ContextVk *contextVk);

    void setProgram(ProgramVk *program)
    {
        ASSERT(!mProgram && !mProgramPipeline);
        mProgram = program;
    }
    void setProgramPipeline(ProgramPipelineVk *pipeline)
    {
        ASSERT(!mProgram && !mProgramPipeline);
        mProgramPipeline = pipeline;
    }

  private:
    friend class ProgramVk;
    friend class ProgramPipelineVk;

    angle::Result allocUniformAndXfbDescriptorSet(ContextVk *contextVk,
                                                  const vk::UniformsAndXfbDesc &xfbBufferDesc,
                                                  bool *newDescriptorSetAllocated);

    angle::Result allocateDescriptorSet(ContextVk *contextVk,
                                        DescriptorSetIndex descriptorSetIndex);
    angle::Result allocateDescriptorSetAndGetInfo(ContextVk *contextVk,
                                                  DescriptorSetIndex descriptorSetIndex,
                                                  bool *newPoolAllocatedOut);
    void addInterfaceBlockDescriptorSetDesc(const std::vector<gl::InterfaceBlock> &blocks,
                                            const gl::ShaderType shaderType,
                                            VkDescriptorType descType,
                                            vk::DescriptorSetLayoutDesc *descOut);
    void addAtomicCounterBufferDescriptorSetDesc(
        const std::vector<gl::AtomicCounterBuffer> &atomicCounterBuffers,
        const gl::ShaderType shaderType,
        vk::DescriptorSetLayoutDesc *descOut);
    void addImageDescriptorSetDesc(const gl::ProgramExecutable &executable,
                                   bool useOldRewriteStructSamplers,
                                   vk::DescriptorSetLayoutDesc *descOut);
    void addTextureDescriptorSetDesc(const gl::ProgramState &programState,
                                     bool useOldRewriteStructSamplers,
                                     const gl::ActiveTextureArray<vk::TextureUnit> *activeTextures,
                                     vk::DescriptorSetLayoutDesc *descOut);

    void resolvePrecisionMismatch(const gl::ProgramMergedVaryings &mergedVaryings);
    void updateDefaultUniformsDescriptorSet(const gl::ShaderType shaderType,
                                            const DefaultUniformBlock &defaultUniformBlock,
                                            vk::BufferHelper *defaultUniformBuffer,
                                            ContextVk *contextVk);
    void updateTransformFeedbackDescriptorSetImpl(const gl::ProgramState &programState,
                                                  ContextVk *contextVk);
    angle::Result updateBuffersDescriptorSet(ContextVk *contextVk,
                                             const gl::ShaderType shaderType,
                                             vk::ResourceUseList *resourceUseList,
                                             vk::CommandBufferHelper *commandBufferHelper,
                                             const std::vector<gl::InterfaceBlock> &blocks,
                                             VkDescriptorType descriptorType);
    angle::Result updateAtomicCounterBuffersDescriptorSet(
        const gl::ProgramState &programState,
        const gl::ShaderType shaderType,
        ContextVk *contextVk,
        vk::ResourceUseList *resourceUseList,
        vk::CommandBufferHelper *commandBufferHelper);
    angle::Result updateImagesDescriptorSet(const gl::ProgramExecutable &executable,
                                            const gl::ShaderType shaderType,
                                            ContextVk *contextVk);
    angle::Result initDynamicDescriptorPools(ContextVk *contextVk,
                                             vk::DescriptorSetLayoutDesc &descriptorSetLayoutDesc,
                                             DescriptorSetIndex descriptorSetIndex,
                                             VkDescriptorSetLayout descriptorSetLayout);

    // Descriptor sets for uniform blocks and textures for this program.
    vk::DescriptorSetLayoutArray<VkDescriptorSet> mDescriptorSets;
    vk::DescriptorSetLayoutArray<VkDescriptorSet> mEmptyDescriptorSets;
    size_t mNumDefaultUniformDescriptors;
    vk::BufferSerial mCurrentDefaultUniformBufferSerial;

    angle::HashMap<vk::UniformsAndXfbDesc, VkDescriptorSet> mUniformsAndXfbDescriptorSetCache;
    angle::HashMap<vk::TextureDescriptorDesc, VkDescriptorSet> mTextureDescriptorsCache;

    // We keep a reference to the pipeline and descriptor set layouts. This ensures they don't get
    // deleted while this program is in use.
    vk::BindingPointer<vk::PipelineLayout> mPipelineLayout;
    vk::DescriptorSetLayoutPointerArray mDescriptorSetLayouts;

    // Keep bindings to the descriptor pools. This ensures the pools stay valid while the Program
    // is in use.
    vk::DescriptorSetLayoutArray<vk::RefCountedDescriptorPoolBinding> mDescriptorPoolBindings;

    // Store descriptor pools here. We store the descriptors in the Program to facilitate descriptor
    // cache management. It can also allow fewer descriptors for shaders which use fewer
    // textures/buffers.
    vk::DescriptorSetLayoutArray<vk::DynamicDescriptorPool> mDynamicDescriptorPools;

    gl::ShaderVector<uint32_t> mDynamicBufferOffsets;

    // TODO: http://anglebug.com/4524: Need a different hash key than a string,
    // since that's slow to calculate.
    ShaderMapInterfaceVariableInfoMap mVariableInfoMap;

    ProgramInfo mGraphicsProgramInfos[static_cast<int>(ProgramTransformOption::PermutationCount)];
    ProgramInfo mComputeProgramInfo;

    ProgramTransformOptionBits mTransformOptionBits;

    ProgramVk *mProgram;
    ProgramPipelineVk *mProgramPipeline;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_PROGRAMEXECUTABLEVK_H_
