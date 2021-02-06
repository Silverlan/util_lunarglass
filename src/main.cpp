/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*
* Copyright (c) 2020 Florian Weischer
*/

#include "util_lunarglass/util_lunarglass.hpp"
#include "GlslangToTop.h"
#include "SpvToTop.h"
#include "GlslManager.h"

#pragma comment(lib,"LLVMJIT.lib")
#pragma comment(lib,"LLVMInterpreter.lib")
#pragma comment(lib,"LLVMX86CodeGen.lib")
#pragma comment(lib,"LLVMX86AsmParser.lib")
#pragma comment(lib,"LLVMX86Disassembler.lib")
#pragma comment(lib,"LLVMRuntimeDyld.lib")
#pragma comment(lib,"LLVMExecutionEngine.lib")
#pragma comment(lib,"LLVMAsmPrinter.lib")
#pragma comment(lib,"LLVMSelectionDAG.lib")
#pragma comment(lib,"LLVMX86Desc.lib")
#pragma comment(lib,"LLVMMCParser.lib")
#pragma comment(lib,"LLVMCodeGen.lib")
#pragma comment(lib,"LLVMX86AsmPrinter.lib")
#pragma comment(lib,"LLVMX86Info.lib")
#pragma comment(lib,"LLVMObjCARCOpts.lib")
#pragma comment(lib,"LLVMScalarOpts.lib")
#pragma comment(lib,"LLVMX86Utils.lib")
#pragma comment(lib,"LLVMInstCombine.lib")
#pragma comment(lib,"LLVMTransformUtils.lib")
#pragma comment(lib,"LLVMipa.lib")
#pragma comment(lib,"LLVMAnalysis.lib")
#pragma comment(lib,"LLVMTarget.lib")
#pragma comment(lib,"LLVMCore.lib")
#pragma comment(lib,"LLVMMC.lib")
#pragma comment(lib,"LLVMObject.lib")
#pragma comment(lib,"LLVMSupport.lib")
#pragma comment(lib,"LLVMipo.lib")
#pragma comment(lib,"core.lib")

#pragma comment(lib,"GenericCodeGen.lib")
#pragma comment(lib,"glslang.lib")
#pragma comment(lib,"MachineIndependent.lib")
#pragma comment(lib,"OSDependent.lib")
#pragma comment(lib,"OGLCompiler.lib")

std::optional<std::unordered_map<lunarglass::ShaderStage,std::string>> lunarglass::optimize_glsl(const std::unordered_map<ShaderStage,std::string> &shaderStages,std::string &outInfoLog)
{
    static auto glslangInitialized = false;
    if(glslangInitialized == false)
    {
        glslangInitialized = true;
        glslang::InitializeProcess();
    }
	auto program = std::make_unique<glslang::TProgram>();
	std::vector<std::unique_ptr<glslang::TShader>> shaders;
	shaders.reserve(shaderStages.size());
	TBuiltInResource resources;
    resources.maxLights = 32;
    resources.maxClipPlanes = 6;
    resources.maxTextureUnits = 32;
    resources.maxTextureCoords = 32;
    resources.maxVertexAttribs = 64;
    resources.maxVertexUniformComponents = 4096;
    resources.maxVaryingFloats = 64;
    resources.maxVertexTextureImageUnits = 32;
    resources.maxCombinedTextureImageUnits = 80;
    resources.maxTextureImageUnits = 32;
    resources.maxFragmentUniformComponents = 4096;
    resources.maxDrawBuffers = 32;
    resources.maxVertexUniformVectors = 128;
    resources.maxVaryingVectors = 8;
    resources.maxFragmentUniformVectors = 16;
    resources.maxVertexOutputVectors = 16;
    resources.maxFragmentInputVectors = 15;
    resources.minProgramTexelOffset = -8;
    resources.maxProgramTexelOffset = 7;
    resources.maxClipDistances = 8;
    resources.maxComputeWorkGroupCountX = 65535;
    resources.maxComputeWorkGroupCountY = 65535;
    resources.maxComputeWorkGroupCountZ = 65535;
    resources.maxComputeWorkGroupSizeX = 1024;
    resources.maxComputeWorkGroupSizeY = 1024;
    resources.maxComputeWorkGroupSizeZ = 64;
    resources.maxComputeUniformComponents = 1024;
    resources.maxComputeTextureImageUnits = 16;
    resources.maxComputeImageUniforms = 8;
    resources.maxComputeAtomicCounters = 8;
    resources.maxComputeAtomicCounterBuffers = 1;
    resources.maxVaryingComponents = 60;
    resources.maxVertexOutputComponents = 64;
    resources.maxGeometryInputComponents = 64;
    resources.maxGeometryOutputComponents = 128;
    resources.maxFragmentInputComponents = 128;
    resources.maxImageUnits = 8;
    resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    resources.maxImageSamples = 0;
    resources.maxVertexImageUniforms = 0;
    resources.maxTessControlImageUniforms = 0;
    resources.maxTessEvaluationImageUniforms = 0;
    resources.maxGeometryImageUniforms = 0;
    resources.maxFragmentImageUniforms = 8;
    resources.maxCombinedImageUniforms = 8;
    resources.maxGeometryTextureImageUnits = 16;
    resources.maxGeometryOutputVertices = 256;
    resources.maxGeometryTotalOutputComponents = 1024;
    resources.maxGeometryUniformComponents = 1024;
    resources.maxGeometryVaryingComponents = 64;
    resources.maxTessControlInputComponents = 128;
    resources.maxTessControlOutputComponents = 128;
    resources.maxTessControlTextureImageUnits = 16;
    resources.maxTessControlUniformComponents = 1024;
    resources.maxTessControlTotalOutputComponents = 4096;
    resources.maxTessEvaluationInputComponents = 128;
    resources.maxTessEvaluationOutputComponents = 128;
    resources.maxTessEvaluationTextureImageUnits = 16;
    resources.maxTessEvaluationUniformComponents = 1024;
    resources.maxTessPatchComponents = 120;
    resources.maxPatchVertices = 32;
    resources.maxTessGenLevel = 64;
    resources.maxViewports = 16;
    resources.maxVertexAtomicCounters = 0;
    resources.maxTessControlAtomicCounters = 0;
    resources.maxTessEvaluationAtomicCounters = 0;
    resources.maxGeometryAtomicCounters = 0;
    resources.maxFragmentAtomicCounters = 8;
    resources.maxCombinedAtomicCounters = 8;
    resources.maxAtomicCounterBindings = 1;
    resources.maxVertexAtomicCounterBuffers = 0;
    resources.maxTessControlAtomicCounterBuffers = 0;
    resources.maxTessEvaluationAtomicCounterBuffers = 0;
    resources.maxGeometryAtomicCounterBuffers = 0;
    resources.maxFragmentAtomicCounterBuffers = 1;
    resources.maxCombinedAtomicCounterBuffers = 1;
    resources.maxAtomicCounterBufferSize = 16384;
    resources.maxTransformFeedbackBuffers = 4;
    resources.maxTransformFeedbackInterleavedComponents = 64;
    resources.maxCullDistances = 8;
    resources.maxCombinedClipAndCullDistances = 8;
    resources.maxSamples = 4;

    resources.limits.nonInductiveForLoops = 1;
    resources.limits.whileLoops = 1;
    resources.limits.doWhileLoops = 1;
    resources.limits.generalUniformIndexing = 1;
    resources.limits.generalAttributeMatrixVectorIndexing = 1;
    resources.limits.generalVaryingIndexing = 1;
    resources.limits.generalSamplerIndexing = 1;
    resources.limits.generalVariableIndexing = 1;
    resources.limits.generalConstantMatrixVectorIndexing = 1;
	EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
	for(auto &pair : shaderStages)
	{
		EShLanguage stage;
		switch(pair.first)
		{
		case ShaderStage::Compute:
			stage = EShLanguage::EShLangCompute;
			break;
		case ShaderStage::Fragment:
			stage = EShLanguage::EShLangFragment;
			break;
		case ShaderStage::Geometry:
			stage = EShLanguage::EShLangGeometry;
			break;
		case ShaderStage::TessellationControl:
			stage = EShLanguage::EShLangTessControl;
			break;
		case ShaderStage::TessellationEvaluation:
			stage = EShLanguage::EShLangTessEvaluation;
			break;
		case ShaderStage::Vertex:
			stage = EShLanguage::EShLangVertex;
			break;
		}
		static_assert(static_cast<std::underlying_type_t<ShaderStage>>(ShaderStage::Count) == 6u);
		shaders.emplace_back(std::make_unique<glslang::TShader>(stage));
		auto &shader = shaders.back();

        auto code = pair.second;
		char *strings[] = {
			code.data()
		};
        shader->setStrings(strings,1);

        if (! shader->parse(&resources, 100, false, messages)) {
			outInfoLog = shader->getInfoLog();
			return {};
        }

        program->addShader(shader.get());
    }

    //
    // Program-level front-end processing...
    //

    if (! program->link(messages)) {
		outInfoLog = program->getInfoLog();
		return {};
    }

	auto obfuscate = false;
	auto inactive = false;
	const int substitutionLevel = 1;

    std::unordered_map<ShaderStage,std::string> optimizedShaders;
    for (int stage = 0; stage < EShLangCount; ++stage)
	{
	    gla::TransformOptions managerOptions;
	    gla::GlslManager manager(obfuscate,inactive, substitutionLevel);
	    manager.options = managerOptions;
        const glslang::TIntermediate* intermediate = program->getIntermediate((EShLanguage)stage);
        if (! intermediate)
            continue;
		TranslateGlslangToTop(*intermediate, manager);
		// Generate the Bottom IR
		manager.translateTopToBottom();

		// Generate the GLSL output
		manager.translateBottomToTarget();

		if(manager.getGeneratedShader())
		{
            ShaderStage eStage;
            switch(stage)
            {
            case EShLangVertex:
                eStage = ShaderStage::Vertex;
                break;
            case EShLangTessControl:
                eStage = ShaderStage::TessellationControl;
                break;
            case EShLangTessEvaluation:
                eStage = ShaderStage::TessellationEvaluation;
                break;
            case EShLangGeometry:
                eStage = ShaderStage::Geometry;
                break;
            case EShLangFragment:
                eStage = ShaderStage::Fragment;
                break;
            case EShLangCompute:
                eStage = ShaderStage::Compute;
                break;
            default:
            {
                outInfoLog = "Unsupported shader stage: " +std::to_string(stage);
                return {};
            }
            }
            optimizedShaders[eStage] = manager.getGeneratedShader();
		}
	}
	return optimizedShaders;
}
