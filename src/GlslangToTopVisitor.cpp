//===- GlslangToTop.cpp - Translate GLSL IR to LunarGLASS Top IR ---------===//
//
// LunarGLASS: An Open Modular Shader Compiler Architecture
// Copyright (C) 2010-2014 LunarG, Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
//     Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
// 
//     Redistributions in binary form must reproduce the above
//     copyright notice, this list of conditions and the following
//     disclaimer in the documentation and/or other materials provided
//     with the distribution.
// 
//     Neither the name of LunarG Inc. nor the names of its
//     contributors may be used to endorse or promote products derived
//     from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//===----------------------------------------------------------------------===//
//
// Author: John Kessenich, LunarG
//
// Visit the nodes in the glslang intermediate tree representation to
// translate it to LunarGLASS TopIR.
//
//===----------------------------------------------------------------------===//

// Glslang includes
#include "glslang/MachineIndependent/localintermediate.h"
#include "glslang/MachineIndependent/SymbolTable.h"

// LunarGLASS includes
#include "LunarGLASSTopIR.h"
#include "LunarGLASSManager.h"
#include "Exceptions.h"
#include "TopBuilder.h"
#include "metadata.h"

// LLVM includes
#pragma warning(push, 1)
#include "llvm/Support/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Scalar.h"
#pragma warning(pop)

#include <string>
#include <map>
#include <list>
#include <vector>
#include <stack>

// Adapter includes
#include "GlslangToTopVisitor.h"
//
// Use this class to carry along data from node to node in the traversal
//
class TGlslangToTopTraverser : public glslang::TIntermTraverser {
public:
    TGlslangToTopTraverser(gla::Manager*, const glslang::TIntermediate*);
    virtual ~TGlslangToTopTraverser();

    bool visitAggregate(glslang::TVisit, glslang::TIntermAggregate*);
    bool visitBinary(glslang::TVisit, glslang::TIntermBinary*);
    void visitConstantUnion(glslang::TIntermConstantUnion*);
    bool visitSelection(glslang::TVisit, glslang::TIntermSelection*);
    bool visitSwitch(glslang::TVisit, glslang::TIntermSwitch*);
    void visitSymbol(glslang::TIntermSymbol* symbol);
    bool visitUnary(glslang::TVisit, glslang::TIntermUnary*);
    bool visitLoop(glslang::TVisit, glslang::TIntermLoop*);
    bool visitBranch(glslang::TVisit visit, glslang::TIntermBranch*);

protected:
    gla::Builder::EStorageQualifier mapStorageClass(const glslang::TQualifier&) const;
    llvm::Value* createLLVMVariable(const glslang::TIntermSymbol*);
    llvm::Type* convertGlslangToGlaType(const glslang::TType& type);

    bool isShaderEntrypoint(const glslang::TIntermAggregate* node);
    void makeFunctions(const glslang::TIntermSequence&);
    void handleFunctionEntry(const glslang::TIntermAggregate* node);
    void translateArguments(glslang::TIntermOperator* node, std::vector<llvm::Value*>& arguments);
    bool argNeedsLValue(const glslang::TIntermOperator* node, int arg);
    llvm::Value* handleTextureCall(glslang::TIntermOperator*);
    llvm::Value* handleTexImageQuery(const glslang::TIntermOperator*, const glslang::TCrackedTextureOp&, const std::vector<llvm::Value*>& arguments, gla::ESamplerType);
    llvm::Value* handleImageAccess(const glslang::TIntermOperator*, const std::vector<llvm::Value*>& arguments, gla::ESamplerType, bool isUnsigned);
    llvm::Value* handleTextureAccess(const glslang::TIntermOperator*, const glslang::TCrackedTextureOp&, const std::vector<llvm::Value*>& arguments, gla::ESamplerType, int flags);
    llvm::Value* handleUserFunctionCall(const glslang::TIntermAggregate*);

    void storeResultMemberToOperand(llvm::Value* structure, int member, TIntermNode& node);
    void storeResultMemberToReturnValue(llvm::Value* structure, int member);
    llvm::Value* createBinaryOperation(glslang::TOperator op, gla::EMdPrecision, llvm::Value* left, llvm::Value* right, bool isUnsigned, bool reduceComparison = true);
    llvm::Value* createUnaryOperation(glslang::TOperator op, gla::EMdPrecision, llvm::Value* operand);
    llvm::Value* createConversion(glslang::TOperator op, gla::EMdPrecision, llvm::Type*, llvm::Value* operand);
    llvm::Value* createUnaryIntrinsic(glslang::TOperator op, gla::EMdPrecision, llvm::Value* operand);
    llvm::Value* createIntrinsic(glslang::TOperator op, gla::EMdPrecision, std::vector<llvm::Value*>& operands, bool isUnsigned);
    llvm::Value* createIntrinsic(glslang::TOperator op);
    void createPipelineRead(glslang::TIntermSymbol*, llvm::Value* storage, int slot, llvm::MDNode*);
    void createPipelineSubread(const glslang::TType&, llvm::Value* storage, std::vector<llvm::Value*>& gepChain, int& slot, llvm::MDNode* md,
                               std::string& name, gla::EInterpolationMethod, gla::EInterpolationLocation);
    int assignSlot(glslang::TIntermSymbol* node, bool input, int& numSlots);
    llvm::Value* getSymbolStorage(const glslang::TIntermSymbol* node, bool& firstTime);
    llvm::Constant* createLLVMConstant(const glslang::TType& type, const glslang::TConstUnionArray&, int& nextConst);
    llvm::Value* MakePermanentTypeProxy(llvm::Type*, llvm::StringRef name);
    llvm::MDNode* declareUniformMetadata(glslang::TIntermSymbol* node, llvm::Value*);
    llvm::MDNode* declareMdIo(llvm::StringRef symbolName, const glslang::TType&, llvm::Type* proxyType, llvm::StringRef proxyName, int slot,
                              gla::EMdTypeLayout inheritMatrix, const char* kind = nullptr);
    void declareChildMdIo(const glslang::TType& type, llvm::Type* proxyType, llvm::SmallVector<llvm::MDNode*, 10>& members, gla::EMdTypeLayout inheritMatrix);
    llvm::MDNode* makeMdSampler(const glslang::TType&, llvm::Type*, llvm::StringRef name);
    llvm::MDNode* declareMdType(const glslang::TType&, gla::EMdTypeLayout inheritMatrix);
    void setOutputMetadata(glslang::TIntermSymbol* node, llvm::Value*, int slot, int numSlots);
    llvm::MDNode* makeInputMetadata(glslang::TIntermSymbol* node, llvm::Value*, int slot);

    const glslang::TTypeList* getStructIfIsStruct(const glslang::TType& type) const { return type.isStruct() ? type.getStruct() : nullptr; }

    gla::Manager& manager;
    llvm::LLVMContext &context;
    llvm::BasicBlock* globalInitializerInsertPoint; // the last block of the global initializers, which start at beginning of the entry point
    llvm::BasicBlock* mainBody;    // the beginning of code that originally was expressed at the beginning of main, after globalInitializerInsertPoint
    llvm::BasicBlock* lastBodyBlock; // the last block forming the user code in the entry-point function
    llvm::IRBuilder<> llvmBuilder;
    llvm::Module* module;
    gla::Metadata metadata;
    bool useUniformOffsets;

    gla::Builder* glaBuilder;
    int nextSlot;                // non-user set interpolations slots, virtual space, so inputs and outputs can both share it
    bool inMain;
    bool linkageOnly;
    const glslang::TIntermediate* glslangIntermediate; // N.B.: this is only available when using the new C++ glslang interface path

    std::map<int, llvm::Value*> symbolValues;
    std::map<std::string, llvm::Function*> functionMap;
    std::map<std::string, int> slotMap;
    std::map<int, llvm::MDNode*> inputMdMap;
    std::map<std::string, llvm::MDNode*> uniformMdMap;
    std::map<const glslang::TTypeList*, llvm::StructType*> structMap;
    std::map<const glslang::TTypeList*, std::vector<int> > memberRemapper;  // for mapping glslang block indices to llvm indices (e.g., due to hidden members)
    std::stack<bool> breakForLoop;  // false means break for switch
    std::stack<glslang::TIntermTyped*> loopTerminal;  // code from the last part of a for loop: for(...; ...; terminal), needed for e.g., continue statements
    const char* leftName;
};

namespace {

// Helper functions for translating glslang to metadata, so that information
// not representable in LLVM does not get lost.

gla::EMdInputOutput GetMdInputOutput(const glslang::TType& type)
{
    gla::EMdInputOutput mdQualifier = gla::EMioNone;

    if (type.getBasicType() == glslang::EbtBlock) {
        switch (type.getQualifier().storage) {
        default:
            break;
        case glslang::EvqVaryingIn:
            mdQualifier = gla::EMioPipeInBlock;
            break;
        case glslang::EvqVaryingOut:
            mdQualifier = gla::EMioPipeOutBlock;
            break;
        case glslang::EvqBuffer:
            if (type.getStruct()->back().type->isArray() && type.getStruct()->back().type->getOuterArraySize() == glslang::UnsizedArraySize && type.getStruct()->back().type->getQualifier().storage == glslang::EvqBuffer)
                mdQualifier = gla::EMioBufferBlockMemberArrayed;
            else
                mdQualifier = gla::EMioBufferBlockMember;
            break;
        case glslang::EvqUniform:
            mdQualifier = gla::EMioUniformBlockMember;
            break;
        }

        return mdQualifier;
    }

    // non-blocks...

    switch (type.getQualifier().storage) {
    default:                                                             break;

    // inputs
    case glslang::EvqVaryingIn:  mdQualifier = gla::EMioPipeIn;          break;
    case glslang::EvqVertexId:   mdQualifier = gla::EMioVertexId;        break;
    case glslang::EvqInstanceId: mdQualifier = gla::EMioInstanceId;      break;
    case glslang::EvqFace:       mdQualifier = gla::EMioFragmentFace;    break;
    case glslang::EvqPointCoord: mdQualifier = gla::EMioPointCoord;      break;
    case glslang::EvqFragCoord:  mdQualifier = gla::EMioFragmentCoord;   break;

    // outputs
    case glslang::EvqVaryingOut: mdQualifier = gla::EMioPipeOut;         break;
    case glslang::EvqPosition:   mdQualifier = gla::EMioVertexPosition;  break;
    case glslang::EvqPointSize:  mdQualifier = gla::EMioPointSize;       break;
    case glslang::EvqClipVertex: mdQualifier = gla::EMioClipVertex;      break;
    case glslang::EvqFragColor:  mdQualifier = gla::EMioPipeOut;         break;
    case glslang::EvqFragDepth:  mdQualifier = gla::EMioFragmentDepth;   break;

    // uniforms
    case glslang::EvqUniform:    mdQualifier = gla::EMioDefaultUniform;  break;
    }

    return mdQualifier;
}

gla::EMdTypeLayout GetMdTypeLayout(const glslang::TType& type, gla::EMdTypeLayout& inheritMatrix)
{
    gla::EMdTypeLayout mdType;

    if (type.isMatrix()) {
        switch (type.getQualifier().layoutMatrix) {
        case glslang::ElmRowMajor:
            mdType = gla::EMtlRowMajorMatrix;
            break;
        case glslang::ElmColumnMajor:
            mdType = gla::EMtlColMajorMatrix;
            break;
        default:
            if (inheritMatrix != gla::EMtlNone)
                mdType = inheritMatrix;
            else
                mdType = gla::EMtlColMajorMatrix;
            break;
        }
    } else {
        switch (type.getQualifier().layoutMatrix) {
        case glslang::ElmRowMajor:
            inheritMatrix = gla::EMtlRowMajorMatrix;
            break;
        case glslang::ElmColumnMajor:
            inheritMatrix = gla::EMtlColMajorMatrix;
            break;
        default:
            break;
        }

        switch (type.getBasicType()) {
        default:                     mdType = gla::EMtlNone;       break;
        case glslang::EbtSampler:    mdType = gla::EMtlSampler;    break;
        case glslang::EbtStruct:     mdType = gla::EMtlAggregate;  break;
        case glslang::EbtUint:       mdType = gla::EMtlUnsigned;   break;
        case glslang::EbtAtomicUint: mdType = gla::EMtlAtomicUint; break;
        case glslang::EbtBlock:
            switch (type.getQualifier().storage) {
            case glslang::EvqUniform:
            case glslang::EvqBuffer:
                switch (type.getQualifier().layoutPacking) {
                case glslang::ElpShared:  return gla::EMtlShared;
                case glslang::ElpStd140:  return gla::EMtlStd140;
                case glslang::ElpStd430:  return gla::EMtlStd430;
                case glslang::ElpPacked:  return gla::EMtlPacked;
                default:
                    gla::UnsupportedFunctionality("uniform block layout", gla::EATContinue);
                    return gla::EMtlShared;
                }
                break;
            case glslang::EvqVaryingIn:
            case glslang::EvqVaryingOut:
                if (type.getQualifier().layoutPacking != glslang::ElpNone)
                    gla::UnsupportedFunctionality("in/out block layout", gla::EATContinue);
                return gla::EMtlNone;
            default:
                gla::UnsupportedFunctionality("block storage qualification", gla::EATContinue);
                return gla::EMtlNone;
            }
        }
    }

    return mdType;
}

gla::EMdSampler GetMdSampler(const glslang::TType& type)
{
    if (! type.getSampler().image)
        return gla::EMsTexture;

    // The rest is for images

    switch (type.getQualifier().layoutFormat) {
    case glslang::ElfNone:             return gla::EMsImage;
    case glslang::ElfRgba32f:          return gla::EMsRgba32f;
    case glslang::ElfRgba16f:          return gla::EMsRgba16f;
    case glslang::ElfR32f:             return gla::EMsR32f;
    case glslang::ElfRgba8:            return gla::EMsRgba8;
    case glslang::ElfRgba8Snorm:       return gla::EMsRgba8Snorm;
    case glslang::ElfRg32f:            return gla::EMsRg32f;
    case glslang::ElfRg16f:            return gla::EMsRg16f;
    case glslang::ElfR11fG11fB10f:     return gla::EMsR11fG11fB10f;
    case glslang::ElfR16f:             return gla::EMsR16f;
    case glslang::ElfRgba16:           return gla::EMsRgba16;
    case glslang::ElfRgb10A2:          return gla::EMsRgb10A2;
    case glslang::ElfRg16:             return gla::EMsRg16;
    case glslang::ElfRg8:              return gla::EMsRg8;
    case glslang::ElfR16:              return gla::EMsR16;
    case glslang::ElfR8:               return gla::EMsR8;
    case glslang::ElfRgba16Snorm:      return gla::EMsRgba16Snorm;
    case glslang::ElfRg16Snorm:        return gla::EMsRg16Snorm;
    case glslang::ElfRg8Snorm:         return gla::EMsRg8Snorm;
    case glslang::ElfR16Snorm:         return gla::EMsR16Snorm;
    case glslang::ElfR8Snorm:          return gla::EMsR8Snorm;
    case glslang::ElfRgba32i:          return gla::EMsRgba32i;
    case glslang::ElfRgba16i:          return gla::EMsRgba16i;
    case glslang::ElfRgba8i:           return gla::EMsRgba8i;
    case glslang::ElfR32i:             return gla::EMsR32i;
    case glslang::ElfRg32i:            return gla::EMsRg32i;
    case glslang::ElfRg16i:            return gla::EMsRg16i;
    case glslang::ElfRg8i:             return gla::EMsRg8i;
    case glslang::ElfR16i:             return gla::EMsR16i;
    case glslang::ElfR8i:              return gla::EMsR8i;
    case glslang::ElfRgba32ui:         return gla::EMsRgba32ui;
    case glslang::ElfRgba16ui:         return gla::EMsRgba16ui;
    case glslang::ElfRgba8ui:          return gla::EMsRgba8ui;
    case glslang::ElfR32ui:            return gla::EMsR32ui;
    case glslang::ElfRg32ui:           return gla::EMsRg32ui;
    case glslang::ElfRg16ui:           return gla::EMsRg16ui;
    case glslang::ElfRg8ui:            return gla::EMsRg8ui;
    case glslang::ElfR16ui:            return gla::EMsR16ui;
    case glslang::ElfR8ui:             return gla::EMsR8ui;
    default:
        gla::UnsupportedFunctionality("unknown image format", gla::EATContinue);
        return gla::EMsImage;
    }
}

gla::EMdSamplerDim GetMdSamplerDim(const glslang::TType& type)
{
    switch (type.getSampler().dim) {
    case glslang::Esd1D:     return gla::EMsd1D;
    case glslang::Esd2D:     return type.getSampler().ms ? gla::EMsd2DMS : gla::EMsd2D;
    case glslang::Esd3D:     return gla::EMsd3D;
    case glslang::EsdCube:   return gla::EMsdCube;
    case glslang::EsdRect:   return gla::EMsdRect;
    case glslang::EsdBuffer: return gla::EMsdBuffer;
    default:
        gla::UnsupportedFunctionality("unknown sampler dimension", gla::EATContinue);
        return gla::EMsd2D;
    }
}

gla::EMdSamplerBaseType GetMdSamplerBaseType(glslang::TBasicType type)
{
    switch (type) {
    case glslang::EbtFloat:    return gla::EMsbFloat;
    case glslang::EbtInt:      return gla::EMsbInt;
    case glslang::EbtUint:     return gla::EMsbUint;
    default:
        gla::UnsupportedFunctionality("base type of sampler return type", gla::EATContinue);
        return gla::EMsbFloat;
    }
}

int GetMdSlotLocation(const glslang::TType& type)
{
    if (type.getQualifier().layoutLocation == glslang::TQualifier::layoutLocationEnd)
        return gla::MaxUserLayoutLocation;
    else
        return type.getQualifier().layoutLocation;
}

int GetMdLocation(const glslang::TType& type)
{
    if (type.getQualifier().layoutLocation != glslang::TQualifier::layoutLocationEnd)
        return type.getQualifier().layoutLocation;
    else
        return gla::MaxUserLayoutLocation;
}

int GetMdBinding(const glslang::TType& type)
{
    if (type.getQualifier().layoutBinding != glslang::TQualifier::layoutBindingEnd)
        return type.getQualifier().layoutBinding;
    else
        return -1;
}

unsigned int GetMdQualifiers(const glslang::TType& type)
{
    unsigned int qualifiers = 0;

    if (type.getQualifier().volatil)
        qualifiers |= 1 << gla::EmqVolatile;
    if (type.getQualifier().readonly)
        qualifiers |= 1 << gla::EmqNonwritable;
    if (type.getQualifier().writeonly)
        qualifiers |= 1 << gla::EmqNonreadable;
    if (type.getQualifier().restrict)
        qualifiers |= 1 << gla::EmqRestrict;
    if (type.getQualifier().coherent)
        qualifiers |= 1 << gla::EmqCoherent;

    return qualifiers;
}

int GetMdOffset(const glslang::TType& type, bool uniformOffsets)
{
    // use the default if this is just offset of uniform member where it
    // can only be the default
    if (type.getBasicType() != glslang::EbtAtomicUint && ! uniformOffsets)
        return -1;

    if (type.getQualifier().hasOffset())
        return type.getQualifier().layoutOffset;
    else
       return -1;
}

gla::EMdPrecision GetMdPrecision(const glslang::TType& type)
{
    switch (type.getQualifier().precision) {
    case glslang::EpqNone:    return gla::EMpNone;
    case glslang::EpqLow:     return gla::EMpLow;
    case glslang::EpqMedium:  return gla::EMpMedium;
    case glslang::EpqHigh:    return gla::EMpHigh;
    default:                  return gla::EMpNone;
    }
}

gla::EMdBuiltIn GetMdBuiltIn(const glslang::TType& type)
{
    switch (type.getQualifier().builtIn) {
    case glslang::EbvNone:                 return gla::EmbNone;
    case glslang::EbvNumWorkGroups:        return gla::EmbNumWorkGroups;
    case glslang::EbvWorkGroupSize:        return gla::EmbWorkGroupSize;
    case glslang::EbvWorkGroupId:          return gla::EmbWorkGroupId;
    case glslang::EbvLocalInvocationId:    return gla::EmbLocalInvocationId;
    case glslang::EbvGlobalInvocationId:   return gla::EmbGlobalInvocationId;
    case glslang::EbvLocalInvocationIndex: return gla::EmbLocalInvocationIndex;
    case glslang::EbvVertexId:             return gla::EmbVertexId;
    case glslang::EbvInstanceId:           return gla::EmbInstanceId;
    case glslang::EbvVertexIndex:          return gla::EmbVertexIndex;
    case glslang::EbvInstanceIndex:        return gla::EmbInstanceIndex;
    case glslang::EbvPosition:             return gla::EmbPosition;
    case glslang::EbvPointSize:            return gla::EmbPointSize;
    case glslang::EbvClipVertex:           return gla::EmbClipVertex;
    case glslang::EbvClipDistance:         return gla::EmbClipDistance;
    case glslang::EbvCullDistance:         return gla::EmbCullDistance;
    case glslang::EbvNormal:               return gla::EmbNormal;
    case glslang::EbvVertex:               return gla::EmbVertex;
    case glslang::EbvMultiTexCoord0:       return gla::EmbMultiTexCoord0;
    case glslang::EbvMultiTexCoord1:       return gla::EmbMultiTexCoord1;
    case glslang::EbvMultiTexCoord2:       return gla::EmbMultiTexCoord2;
    case glslang::EbvMultiTexCoord3:       return gla::EmbMultiTexCoord3;
    case glslang::EbvMultiTexCoord4:       return gla::EmbMultiTexCoord4;
    case glslang::EbvMultiTexCoord5:       return gla::EmbMultiTexCoord5;
    case glslang::EbvMultiTexCoord6:       return gla::EmbMultiTexCoord6;
    case glslang::EbvMultiTexCoord7:       return gla::EmbMultiTexCoord7;
    case glslang::EbvFrontColor:           return gla::EmbFrontColor;
    case glslang::EbvBackColor:            return gla::EmbBackColor;
    case glslang::EbvFrontSecondaryColor:  return gla::EmbFrontSecondaryColor;
    case glslang::EbvBackSecondaryColor:   return gla::EmbBackSecondaryColor;
    case glslang::EbvTexCoord:             return gla::EmbTexCoord;
    case glslang::EbvFogFragCoord:         return gla::EmbFogFragCoord;
    case glslang::EbvInvocationId:         return gla::EmbInvocationId;
    case glslang::EbvPrimitiveId:          return gla::EmbPrimitiveId;
    case glslang::EbvLayer:                return gla::EmbLayer;
    case glslang::EbvViewportIndex:        return gla::EmbViewportIndex;
    case glslang::EbvPatchVertices:        return gla::EmbPatchVertices;
    case glslang::EbvTessLevelOuter:       return gla::EmbTessLevelOuter;
    case glslang::EbvTessLevelInner:       return gla::EmbTessLevelInner;
    case glslang::EbvTessCoord:            return gla::EmbTessCoord;
    case glslang::EbvColor:                return gla::EmbColor;
    case glslang::EbvSecondaryColor:       return gla::EmbSecondaryColor;
    case glslang::EbvFace:                 return gla::EmbFace;
    case glslang::EbvFragCoord:            return gla::EmbFragCoord;
    case glslang::EbvPointCoord:           return gla::EmbPointCoord;
    case glslang::EbvFragColor:            return gla::EmbFragColor;
    case glslang::EbvFragData:             return gla::EmbFragData;
    case glslang::EbvFragDepth:            return gla::EmbFragDepth;
    case glslang::EbvSampleId:             return gla::EmbSampleId;
    case glslang::EbvSamplePosition:       return gla::EmbSamplePosition;
    case glslang::EbvSampleMask:           return gla::EmbSampleMask;
    case glslang::EbvHelperInvocation:     return gla::EmbHelperInvocation;
    case glslang::EbvBoundingBox:          return gla::EmbBoundingBox;
    default:
        gla::UnsupportedFunctionality("built in variable", gla::EATContinue);
        return gla::EmbNone;
    }
}

gla::EMdBlendEquationShift GetMdBlendShift(glslang::TBlendEquationShift b)
{
    switch (b) {
    case glslang::EBlendMultiply:      return gla::EmeMultiply;
    case glslang::EBlendScreen:        return gla::EmeScreen;
    case glslang::EBlendOverlay:       return gla::EmeOverlay;
    case glslang::EBlendDarken:        return gla::EmeDarken;
    case glslang::EBlendLighten:       return gla::EmeLighten;
    case glslang::EBlendColordodge:    return gla::EmeColordodge;
    case glslang::EBlendColorburn:     return gla::EmeColorburn;
    case glslang::EBlendHardlight:     return gla::EmeHardlight;
    case glslang::EBlendSoftlight:     return gla::EmeSoftlight;
    case glslang::EBlendDifference:    return gla::EmeDifference;
    case glslang::EBlendExclusion:     return gla::EmeExclusion;
    case glslang::EBlendHslHue:        return gla::EmeHslHue;
    case glslang::EBlendHslSaturation: return gla::EmeHslSaturation;
    case glslang::EBlendHslColor:      return gla::EmeHslColor;
    case glslang::EBlendHslLuminosity: return gla::EmeHslLuminosity;
    case glslang::EBlendAllEquations:  return gla::EmeAllEquations;
    default:
        gla::UnsupportedFunctionality("built in variable", gla::EATContinue);
        return gla::EmeAllEquations;
    }
}

const char* filterMdName(const glslang::TString& name)
{
    if (glslang::IsAnonymous(name))
        return "";
    else
        return name.c_str();
}

void GetInterpolationLocationMethod(const glslang::TType& type, gla::EInterpolationMethod& method, gla::EInterpolationLocation& location)
{
    method = gla::EIMNone;
    if (type.getQualifier().nopersp)
        method = gla::EIMNoperspective;
    else if (type.getQualifier().smooth)
        method = gla::EIMSmooth;
    else if (type.getQualifier().patch)
        method = gla::EIMPatch;

    location = gla::EILFragment;
    if (type.getQualifier().sample)
        location = gla::EILSample;
    else if (type.getQualifier().centroid)
        location = gla::EILCentroid;
}

};  // end anonymous namespace


// A fully functioning front end will know all array sizes,
// this is just a back up size.
const int UnknownArraySize = 8;

TGlslangToTopTraverser::TGlslangToTopTraverser(gla::Manager* manager, const glslang::TIntermediate* glslangIntermediate)
    : TIntermTraverser(true, false, true),
      manager(*manager), context(manager->getModule()->getContext()), llvmBuilder(context),
      module(manager->getModule()), metadata(context, module),
      nextSlot(gla::MaxUserLayoutLocation), inMain(false), linkageOnly(false),
      glslangIntermediate(glslangIntermediate), leftName(0)
{
    // do this after the builder knows the module
    glaBuilder = new gla::Builder(llvmBuilder, manager, metadata);
    glaBuilder->clearAccessChain();
    glaBuilder->setAccessChainDirectionRightToLeft(false);

    globalInitializerInsertPoint = glaBuilder->makeMain();
    mainBody = llvm::BasicBlock::Create(context, "mainBody");
    globalInitializerInsertPoint->getParent()->getBasicBlockList().push_back(mainBody);
    llvmBuilder.SetInsertPoint(globalInitializerInsertPoint);
    lastBodyBlock = globalInitializerInsertPoint;
    useUniformOffsets = glslangIntermediate->getProfile() != EEsProfile && glslangIntermediate->getVersion() >= 420;

    // Add the top-level modes for this shader.

    if (glslangIntermediate->getXfbMode())
        metadata.makeMdNamedInt(gla::XfbModeMdName, glslangIntermediate->getXfbMode());

    switch (glslangIntermediate->getStage()) {
    case EShLangVertex:
        break;

    case EShLangTessControl:
        metadata.makeMdNamedInt(gla::NumVerticesMdName, glslangIntermediate->getVertices());
        break;

    case EShLangTessEvaluation:
        metadata.makeMdNamedInt(gla::InputPrimitiveMdName, glslangIntermediate->getInputPrimitive());
        metadata.makeMdNamedInt(gla::VertexSpacingMdName, glslangIntermediate->getVertexSpacing());
        metadata.makeMdNamedInt(gla::VertexOrderMdName, glslangIntermediate->getVertexOrder());
        metadata.makeMdNamedInt(gla::PointModeMdName, glslangIntermediate->getPointMode());
        break;

    case EShLangGeometry:
        metadata.makeMdNamedInt(gla::InvocationsMdName, glslangIntermediate->getInvocations());
        metadata.makeMdNamedInt(gla::NumVerticesMdName, glslangIntermediate->getVertices());
        metadata.makeMdNamedInt(gla::InputPrimitiveMdName, glslangIntermediate->getInputPrimitive());
        metadata.makeMdNamedInt(gla::OutputPrimitiveMdName, glslangIntermediate->getOutputPrimitive());
        break;

    case EShLangFragment:
        if (glslangIntermediate->getPixelCenterInteger())
            metadata.makeMdNamedInt(gla::PixelCenterIntegerMdName, glslangIntermediate->getPixelCenterInteger());
        if (glslangIntermediate->getOriginUpperLeft())
            metadata.makeMdNamedInt(gla::OriginUpperLeftMdName, glslangIntermediate->getOriginUpperLeft());
        if (glslangIntermediate->getBlendEquations()) {
            int glslangBlendMask = glslangIntermediate->getBlendEquations();
            int glaBlendMask = 0;
            for (glslang::TBlendEquationShift be = (glslang::TBlendEquationShift)0; be < glslang::EBlendCount; be = (glslang::TBlendEquationShift)(be + 1)) {
                if (glslangBlendMask & be)
                    glaBlendMask |= 1 << GetMdBlendShift(be);
            }

            metadata.makeMdNamedInt(gla::BlendEquationMdName, glslangIntermediate->getBlendEquations());
        }
        break;

    case EShLangCompute:
        metadata.makeMdNamedInt(gla::LocalSizeMdName, glslangIntermediate->getLocalSize(0),
                                                      glslangIntermediate->getLocalSize(1),
                                                      glslangIntermediate->getLocalSize(2));
        break;

    default:
        break;
    }

}

TGlslangToTopTraverser::~TGlslangToTopTraverser()
{
    // Fix up the entry point; it has dangling initializer code at the entry,
    // and unfinished exit.

    // Branch from the end of the initializers to the beginning of the user body.
    // N.B. TODO: this doesn't handle initializers with flow control (i.e. ?:).
    llvmBuilder.SetInsertPoint(globalInitializerInsertPoint);
    llvmBuilder.CreateBr(mainBody);

    // Finish up the exit.
    llvmBuilder.SetInsertPoint(lastBodyBlock);
    glaBuilder->leaveFunction(true);

    delete glaBuilder;
}

//
// The rest of the file are the traversal functions.  The last one
// is the one that starts the traversal.
//
// Return true from interior nodes to have the external traversal
// continue on to children.  Return false if children were
// already processed.
//


//
// Symbols can turn into 
//  - pipeline reads, right now, as intrinic reads into shadow storage
//  - pipeline writes, sometime in the future, as intrinsic writes of shadow storage
//  - complex lvalue base setups:  foo.bar[3]....  , where we see foo and start up an access chain
//  - something simple that degenerates into the last bullet
//
// Uniforms, inputs, and outputs also declare metadata for future linker consumption.
//
// Sort out what the deal is...
//
void TGlslangToTopTraverser::visitSymbol(glslang::TIntermSymbol* symbol)
{
    bool input = symbol->getType().getQualifier().isPipeInput();
    bool output = symbol->getType().getQualifier().isPipeOutput();

    // Normal symbols and uniforms need a variable allocated to them,
    // we will shadow inputs by reading them in whole into a global variables, 
    // and outputs are shadowed for read/write optimizations before being written out,
    // so everything gets a variable allocated; see if we've cached it.
    bool firstTime;
    llvm::Value* storage = getSymbolStorage(symbol, firstTime);
    if (firstTime) {
        if (output) {
            // set up output metadata once for all future pipeline intrinsic writes
            int numSlots;
            int slot = assignSlot(symbol, input, numSlots);
            setOutputMetadata(symbol, storage, slot, numSlots);
        } else if (symbol->getType().getQualifier().storage == glslang::EvqShared) {
            // workgroup shared metadata
            metadata.addShared(storage);
        }
    }

    // set up uniform metadata
    llvm::MDNode* mdNode = 0;
    if (symbol->getType().getQualifier().isUniformOrBuffer())
        mdNode = declareUniformMetadata(symbol, storage);

    if (! linkageOnly) {
        // Prepare to generate code for the access

        // L-value chains will be computed purely left to right, so now is "clear" time
        // (since we are on the symbol; the base of the expression, which is left-most)
        glaBuilder->clearAccessChain();

        // Track the current value
        glaBuilder->setAccessChainLValue(storage);

        // Set up metadata for uniform/sampler inputs
        if (mdNode)
            glaBuilder->setAccessChainMetadata(gla::UniformMdName, mdNode);

        // If it's an output, we also want to know which subset is live.
        if (output)
            glaBuilder->accessChainTrackActive();
    }

    if (input) {
        int numSlots;
        int slot = assignSlot(symbol, input, numSlots);
        mdNode = makeInputMetadata(symbol, storage, slot);

        if (! linkageOnly) {
            // do the actual read
            createPipelineRead(symbol, storage, slot, mdNode);
        }
    }
}

bool TGlslangToTopTraverser::visitBinary(glslang::TVisit /* visit */, glslang::TIntermBinary* node)
{
    // First, handle special cases
    switch (node->getOp()) {
    case glslang::EOpAssign:
    case glslang::EOpAddAssign:
    case glslang::EOpSubAssign:
    case glslang::EOpMulAssign:
    case glslang::EOpVectorTimesMatrixAssign:
    case glslang::EOpVectorTimesScalarAssign:
    case glslang::EOpMatrixTimesScalarAssign:
    case glslang::EOpMatrixTimesMatrixAssign:
    case glslang::EOpDivAssign:
    case glslang::EOpModAssign:
    case glslang::EOpAndAssign:
    case glslang::EOpInclusiveOrAssign:
    case glslang::EOpExclusiveOrAssign:
    case glslang::EOpLeftShiftAssign:
    case glslang::EOpRightShiftAssign:
        // A bin-op assign "a += b" means the same thing as "a = a + b"
        // where a is evaluated before b. For a simple assignment, GLSL
        // says to evaluate the left before the right.  So, always, left
        // node then right node.
        {
            // get the left l-value, save it away
            glaBuilder->clearAccessChain();
            node->getLeft()->traverse(this);
            gla::Builder::AccessChain lValue = glaBuilder->getAccessChain();
            TIntermNode* leftBase = node->getLeft();
            while (! leftBase->getAsSymbolNode()) {
                if (leftBase->getAsBinaryNode())
                    leftBase = leftBase->getAsBinaryNode()->getLeft();
                else
                    break;
            }
            leftName = leftBase->getAsSymbolNode() ? leftBase->getAsSymbolNode()->getName().c_str() : 0;

            // evaluate the right
            glaBuilder->clearAccessChain();
            node->getRight()->traverse(this);
            llvm::Value* rValue = glaBuilder->accessChainLoad(GetMdPrecision(node->getRight()->getType()));

            if (node->getOp() != glslang::EOpAssign) {
                // the left is also an r-value
                glaBuilder->setAccessChain(lValue);
                llvm::Value* leftRValue = glaBuilder->accessChainLoad(GetMdPrecision(node->getLeft()->getType()));

                // do the operation
                rValue = createBinaryOperation(node->getOp(), GetMdPrecision(node->getType()), leftRValue, rValue, node->getType().getBasicType() == glslang::EbtUint);

                // these all need their counterparts in createBinaryOperation()
                assert(rValue);
            }

            // store the result
            glaBuilder->setAccessChain(lValue);
            glaBuilder->accessChainStore(rValue);

            // assignments are expressions having an rValue after they are evaluated...
            glaBuilder->clearAccessChain();
            glaBuilder->setAccessChainRValue(rValue);
        }
        leftName = 0;
        return false;
    case glslang::EOpIndexDirect:
    case glslang::EOpIndexDirectStruct:
        {
            // This adapter is building access chains left to right.
            // Set up the access chain to the left.
            node->getLeft()->traverse(this);

            int index = 0;
            if (node->getRight()->getAsConstantUnion() == 0)
                gla::UnsupportedFunctionality("direct index without a constant node", gla::EATContinue);
            else 
                index = node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();

            if (node->getLeft()->getBasicType() == glslang::EbtBlock && node->getOp() == glslang::EOpIndexDirectStruct) {
                // This may be, e.g., an anonymous block-member selection, which generally need
                // index remapping due to hidden members in anonymous blocks.
                std::vector<int>& remapper = memberRemapper[node->getLeft()->getType().getStruct()];
                if (remapper.size() == 0)
                    gla::UnsupportedFunctionality("block without member remapping", gla::EATContinue);
                else
                    index = remapper[index];
            }

            if (! node->getLeft()->getType().isArray() &&
                node->getLeft()->getType().isVector() &&
                node->getOp() == glslang::EOpIndexDirect) {
                // This is essentially a hard-coded vector swizzle of size 1,
                // so short circuit the GEP stuff with a swizzle.
                std::vector<int> swizzle;
                swizzle.push_back(node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst());
                glaBuilder->accessChainPushSwizzleRight(swizzle, convertGlslangToGlaType(node->getType()), node->getLeft()->getVectorSize());
            } else {
                // normal case for indexing array or structure or block
                glaBuilder->accessChainPushLeft(gla::MakeIntConstant(context, index));
            }

            // If this dereference results in a runtime-sized array, it's a pointer
            // we don't want in the middle of an access chain, but rather the base
            // of a new one.
            if (node->getType().isArray() && node->getType().getOuterArraySize() == glslang::UnsizedArraySize && node->getType().getQualifier().storage == glslang::EvqBuffer)
                glaBuilder->accessChainEvolveToRuntimeArrayBase();
        }
        return false;
    case glslang::EOpIndexIndirect:
        {
            // Structure or array or vector indirection.
            // Will use native LLVM gep for struct and array indirection;
            // matrices are arrays of vectors, so will also work for a matrix.
            // Will use the access chain's 'component' for variable index into a vector.

            // This adapter is building access chains left to right.
            // Set up the access chain to the left.
            node->getLeft()->traverse(this);

            // save it so that computing the right side doesn't trash it
            gla::Builder::AccessChain partial = glaBuilder->getAccessChain();

            // compute the next index in the chain
            glaBuilder->clearAccessChain();
            node->getRight()->traverse(this);
            llvm::Value* index = glaBuilder->accessChainLoad(GetMdPrecision(node->getRight()->getType()));

            // restore the saved access chain
            glaBuilder->setAccessChain(partial);

            if (! node->getLeft()->getType().isArray() && node->getLeft()->getType().isVector())
                glaBuilder->accessChainPushComponent(index);
            else
                glaBuilder->accessChainPushLeft(index);
        }
        return false;
    case glslang::EOpVectorSwizzle:
        {
            node->getLeft()->traverse(this);
            glslang::TIntermSequence& swizzleSequence = node->getRight()->getAsAggregate()->getSequence();
            std::vector<int> swizzle;
            for (int i = 0; i < (int)swizzleSequence.size(); ++i)
                swizzle.push_back(swizzleSequence[i]->getAsConstantUnion()->getConstArray()[0].getIConst());
            glaBuilder->accessChainPushSwizzleRight(swizzle, convertGlslangToGlaType(node->getType()), node->getLeft()->getVectorSize());
        }
        return false;
    default:
        break;
    }

    // Assume generic binary op...

    // Get the operands
    glaBuilder->clearAccessChain();
    node->getLeft()->traverse(this);
    llvm::Value* left = glaBuilder->accessChainLoad(GetMdPrecision(node->getLeft()->getType()));

    glaBuilder->clearAccessChain();
    node->getRight()->traverse(this);
    llvm::Value* right = glaBuilder->accessChainLoad(GetMdPrecision(node->getRight()->getType()));

    llvm::Value* result;
    gla::EMdPrecision precision = GetMdPrecision(node->getType());

    switch (node->getOp()) {
    case glslang::EOpVectorTimesMatrix:
    case glslang::EOpMatrixTimesVector:
    case glslang::EOpMatrixTimesScalar:
    case glslang::EOpMatrixTimesMatrix:
        result = glaBuilder->createMatrixMultiply(precision, left, right);
        break;
    default:
        result = createBinaryOperation(node->getOp(), precision, left, right, node->getLeft()->getType().getBasicType() == glslang::EbtUint);
        break;
    }

    if (! result) {
        gla::UnsupportedFunctionality("glslang binary operation", gla::EATContinue);
    } else {
        glaBuilder->clearAccessChain();
        glaBuilder->setAccessChainRValue(result);

        return false;
    }

    return true;
}

bool TGlslangToTopTraverser::visitUnary(glslang::TVisit /* visit */, glslang::TIntermUnary* node)
{
    // try texturing first
    llvm::Value* result = handleTextureCall(node);
    if (result != nullptr) {
        glaBuilder->clearAccessChain();
        glaBuilder->setAccessChainRValue(result);
        return false;
    }

    // evaluate the operand
    glaBuilder->clearAccessChain();
    node->getOperand()->traverse(this);

    // Array length needs an l-value
    if (node->getOp() == glslang::EOpArrayLength) {
        result = glaBuilder->createIntrinsicCall(gla::EMpNone, llvm::Intrinsic::gla_arraylength, glaBuilder->accessChainGetLValue());
        glaBuilder->clearAccessChain();
        glaBuilder->setAccessChainRValue(result);

        return false; // done with this node
    }

    // Now we know an r-value is needed
    llvm::Value* operand = glaBuilder->accessChainLoad(GetMdPrecision(node->getOperand()->getType()));

    gla::EMdPrecision precision = GetMdPrecision(node->getType());

    // it could be a conversion
    result = createConversion(node->getOp(), precision, convertGlslangToGlaType(node->getType()), operand);

    // if not, then possibly an operation
    if (! result)
        result = createUnaryOperation(node->getOp(), precision, operand);

    // if not, then possibly a LunarGLASS intrinsic
    if (! result)
        result = createUnaryIntrinsic(node->getOp(), precision, operand);

    if (result) {
        glaBuilder->clearAccessChain();
        glaBuilder->setAccessChainRValue(result);

        return false; // done with this node
    }

    // it must be a special case, check...
    switch (node->getOp()) {
    case glslang::EOpPostIncrement:
    case glslang::EOpPostDecrement:
    case glslang::EOpPreIncrement:
    case glslang::EOpPreDecrement:
        {
            // we need the integer value "1" or the floating point "1.0" to add/subtract
            llvm::Value* one = gla::GetBasicTypeID(operand) == llvm::Type::FloatTyID ?
                                     gla::MakeFloatConstant(context, 1.0) :
                                     gla::MakeIntConstant(context, 1);
            glslang::TOperator op;
            if (node->getOp() == glslang::EOpPreIncrement ||
                node->getOp() == glslang::EOpPostIncrement)
                op = glslang::EOpAdd;
            else
                op = glslang::EOpSub;

            llvm::Value* result = createBinaryOperation(op, GetMdPrecision(node->getType()), operand, one, node->getType().getBasicType() == glslang::EbtUint);

            // The result of operation is always stored, but conditionally the
            // consumed result.  The consumed result is always an r-value.
            glaBuilder->accessChainStore(result);
            glaBuilder->clearAccessChain();
            if (node->getOp() == glslang::EOpPreIncrement ||
                node->getOp() == glslang::EOpPreDecrement)
                glaBuilder->setAccessChainRValue(result);
            else
                glaBuilder->setAccessChainRValue(operand);
        }

        return false;
    default:
        gla::UnsupportedFunctionality("glslang unary", gla::EATContinue);
        break;
    }

    return true;
}

bool TGlslangToTopTraverser::visitAggregate(glslang::TVisit visit, glslang::TIntermAggregate* node)
{
    // try texturing first
    llvm::Value* result = handleTextureCall(node);
    if (result != nullptr) {
        glaBuilder->clearAccessChain();
        glaBuilder->setAccessChainRValue(result);
        return false;
    }

    glslang::TOperator binOp = glslang::EOpNull;
    bool reduceComparison = true;
    bool isMatrix = false;

    assert(node->getOp());

    gla::EMdPrecision precision = GetMdPrecision(node->getType());

    switch (node->getOp()) {
    case glslang::EOpSequence:
        {
            // If this is the parent node of all the functions, we want to see them
            // early, so all call points have actual LLVM functions to reference.  
            // In all cases, still let the traverser visit the children for us.
            if (visit == glslang::EvPreVisit)
                makeFunctions(node->getAsAggregate()->getSequence());
        }

        return true;
    case glslang::EOpLinkerObjects:
        {
            if (visit == glslang::EvPreVisit)
                linkageOnly = true;
            else
                linkageOnly = false;
        }

        return true;
    case glslang::EOpComma:
        {
            // processing from left to right naturally leaves the right-most
            // lying around in the access chain
            glslang::TIntermSequence& glslangOperands = node->getSequence();
            for (int i = 0; i < (int)glslangOperands.size(); ++i)
                glslangOperands[i]->traverse(this);
        }

        return false;
    case glslang::EOpFunction:
        if (visit == glslang::EvPreVisit) {
            // Current insert point is for initializers; save it so we
            // can come back to it for any global code appearing after this function.
            globalInitializerInsertPoint = llvmBuilder.GetInsertBlock();
            if (isShaderEntrypoint(node)) {
                inMain = true;
                llvmBuilder.SetInsertPoint(mainBody);
                metadata.addMdEntrypoint("main");
            } else {
                handleFunctionEntry(node);
            }
        } else {
            // tidying up main will occur in the destructor
            if (inMain) {
                inMain = false;
                lastBodyBlock = llvmBuilder.GetInsertBlock();
            } else
                glaBuilder->leaveFunction(false);

            // Initializers after main go near the beginning of main().
            llvmBuilder.SetInsertPoint(globalInitializerInsertPoint);
        }

        return true;
    case glslang::EOpParameters:
        // Parameters will have been consumed by EOpFunction processing, but not
        // the body, so we still visited the function node's children, making this
        // child redundant.
        return false;
    case glslang::EOpFunctionCall:
        {
            if (node->isUserDefined())
                result = handleUserFunctionCall(node);

            if (! result) {
                gla::UnsupportedFunctionality("glslang function call");
                glslang::TConstUnionArray emptyConsts;
                int nextConst = 0;
                result = createLLVMConstant(node->getType(), emptyConsts, nextConst);
            }
            glaBuilder->clearAccessChain();
            glaBuilder->setAccessChainRValue(result);

            return false;
        }

        return true;
    case glslang::EOpConstructMat2x2:
    case glslang::EOpConstructMat2x3:
    case glslang::EOpConstructMat2x4:
    case glslang::EOpConstructMat3x2:
    case glslang::EOpConstructMat3x3:
    case glslang::EOpConstructMat3x4:
    case glslang::EOpConstructMat4x2:
    case glslang::EOpConstructMat4x3:
    case glslang::EOpConstructMat4x4:
    case glslang::EOpConstructDMat2x2:
    case glslang::EOpConstructDMat2x3:
    case glslang::EOpConstructDMat2x4:
    case glslang::EOpConstructDMat3x2:
    case glslang::EOpConstructDMat3x3:
    case glslang::EOpConstructDMat3x4:
    case glslang::EOpConstructDMat4x2:
    case glslang::EOpConstructDMat4x3:
    case glslang::EOpConstructDMat4x4:
        isMatrix = true;
        // fall through
    case glslang::EOpConstructFloat:
    case glslang::EOpConstructVec2:
    case glslang::EOpConstructVec3:
    case glslang::EOpConstructVec4:
    case glslang::EOpConstructDouble:
    case glslang::EOpConstructDVec2:
    case glslang::EOpConstructDVec3:
    case glslang::EOpConstructDVec4:
    case glslang::EOpConstructBool:
    case glslang::EOpConstructBVec2:
    case glslang::EOpConstructBVec3:
    case glslang::EOpConstructBVec4:
    case glslang::EOpConstructInt:
    case glslang::EOpConstructIVec2:
    case glslang::EOpConstructIVec3:
    case glslang::EOpConstructIVec4:
    case glslang::EOpConstructUint:
    case glslang::EOpConstructUVec2:
    case glslang::EOpConstructUVec3:
    case glslang::EOpConstructUVec4:
    case glslang::EOpConstructStruct:
        {
            std::vector<llvm::Value*> arguments;
            translateArguments(node, arguments);
            llvm::Value* constructed = glaBuilder->createVariable(gla::Builder::ESQLocal, 0,
                                                                        convertGlslangToGlaType(node->getType()),
                                                                        0, 0, leftName ? leftName : "constructed");
            if (node->getOp() == glslang::EOpConstructStruct || node->getType().isArray()) {
                // TODO: clean up: is there a more direct way to set a whole LLVM structure?
                //                if not, move this inside Top Builder; too many indirections

                std::vector<llvm::Value*> gepChain;
                gepChain.push_back(gla::MakeIntConstant(context, 0));
                for (int field = 0; field < (int)arguments.size(); ++field) {
                    gepChain.push_back(gla::MakeIntConstant(context, field));
                    llvmBuilder.CreateStore(arguments[field], glaBuilder->createGEP(constructed, gepChain));
                    gepChain.pop_back();
                }
                glaBuilder->clearAccessChain();
                glaBuilder->setAccessChainLValue(constructed);
            } else {
                constructed = glaBuilder->createLoad(constructed);
                if (isMatrix)
                    constructed = glaBuilder->createMatrixConstructor(precision, arguments, constructed);
                else
                    constructed = glaBuilder->createConstructor(precision, arguments, constructed);
                glaBuilder->clearAccessChain();
                glaBuilder->setAccessChainRValue(constructed);
            }

            return false;
        }

    // These six are component-wise compares with component-wise results.
    // Forward on to createBinaryOperation(), requesting a vector result.
    case glslang::EOpLessThan:
    case glslang::EOpGreaterThan:
    case glslang::EOpLessThanEqual:
    case glslang::EOpGreaterThanEqual:
    case glslang::EOpVectorEqual:
    case glslang::EOpVectorNotEqual:
        {
            // Map the operation to a binary
            binOp = node->getOp();
            reduceComparison = false;
            switch (node->getOp()) {
            case glslang::EOpVectorEqual:     binOp = glslang::EOpEqual;      break;
            case glslang::EOpVectorNotEqual:  binOp = glslang::EOpNotEqual;   break;
            default:                          binOp = node->getOp();          break;
            }
        }
        break;

    //case glslang::EOpRecip:
    //    return glaBuilder->createRecip(operand);

    case glslang::EOpMul:
        // compontent-wise matrix multiply      
        binOp = glslang::EOpMul;
        break;
    case glslang::EOpOuterProduct:
        // two vectors multiplied to make a matrix
        binOp = glslang::EOpOuterProduct;
        break;
    case glslang::EOpDot:
        {
            // for scalar dot product, use multiply        
            glslang::TIntermSequence& glslangOperands = node->getSequence();
            if (! glslangOperands[0]->getAsTyped()->isVector())
                binOp = glslang::EOpMul;
            break;
        }
    case glslang::EOpMod:
        // when an aggregate, this is the floating-point mod built-in function,
        // which can be emitted by the one in createBinaryOperation()
        binOp = glslang::EOpMod;
        break;
    case glslang::EOpModf:
    case glslang::EOpFrexp:
        {
            // modf()'s and frexp()'s second operand is only an l-value to set the 2nd return value to

            // use a unary intrinsic form to make the call and get back the returned struct
            glslang::TIntermSequence& glslangOperands = node->getSequence();

            // get 'in' operand
            glaBuilder->clearAccessChain();
            glslangOperands[0]->traverse(this);
            llvm::Value* operand0 = glaBuilder->accessChainLoad(GetMdPrecision(glslangOperands[0]->getAsTyped()->getType()));

            // call
            llvm::Value* structure = createUnaryIntrinsic(node->getOp(), precision, operand0);

            // store integer part into second operand
            storeResultMemberToOperand(structure, 1, *glslangOperands[1]);

            // leave the first part as the function-call's value
            storeResultMemberToReturnValue(structure, 0);
        }
        return false;

    case glslang::EOpAddCarry:
    case glslang::EOpSubBorrow:
        {
            // addCarry()'s and subBorrow()'s third operand is only an l-value to set the 2nd return value to

            // use an intrinsic with reduced operand count to make the call and get back a returned struct
            std::vector<llvm::Value*> operands;
            glslang::TIntermSequence& glslangOperands = node->getSequence();

            // first in
            glaBuilder->clearAccessChain();
            glslangOperands[0]->traverse(this);
            operands.push_back(glaBuilder->accessChainLoad(GetMdPrecision(glslangOperands[0]->getAsTyped()->getType())));

            // second in
            glaBuilder->clearAccessChain();
            glslangOperands[1]->traverse(this);
            operands.push_back(glaBuilder->accessChainLoad(GetMdPrecision(glslangOperands[0]->getAsTyped()->getType())));

            // call
            llvm::Value* structure = createIntrinsic(node->getOp(), precision, operands, glslangOperands[0]->getAsTyped()->getBasicType() == glslang::EbtUint);

            // store second struct member into third operand (out)
            storeResultMemberToOperand(structure, 1, *glslangOperands[2]);

            // leave the first member as the function-call's value
            storeResultMemberToReturnValue(structure, 0);
        }
        return false;

    case glslang::EOpIMulExtended:
    case glslang::EOpUMulExtended:
        {
            // imulExtended()'s and umulExtended()'s third and fourth operands are only l-values, for the two return values

            // use an intrinsic with reduced operand count to make the call and get back a returned struct
            std::vector<llvm::Value*> operands;
            glslang::TIntermSequence& glslangOperands = node->getSequence();

            // first in
            glaBuilder->clearAccessChain();
            glslangOperands[0]->traverse(this);
            operands.push_back(glaBuilder->accessChainLoad(GetMdPrecision(glslangOperands[0]->getAsTyped()->getType())));

            // second in
            glaBuilder->clearAccessChain();
            glslangOperands[1]->traverse(this);
            operands.push_back(glaBuilder->accessChainLoad(GetMdPrecision(glslangOperands[0]->getAsTyped()->getType())));

            // call
            llvm::Value* structure = createIntrinsic(node->getOp(), precision, operands, glslangOperands[0]->getAsTyped()->getBasicType() == glslang::EbtUint);

            // store first struct member into third operand (out)
            storeResultMemberToOperand(structure, 0, *glslangOperands[2]);

            // store second struct member into fourth operand (out)
            storeResultMemberToOperand(structure, 1, *glslangOperands[3]);
        }
        return false;

    case glslang::EOpArrayLength:
        {
            // This might be dead code:  array lengths of known arrays are constant propagated by the front end
            glslang::TIntermTyped* typedNode = node->getSequence()[0]->getAsTyped();
            assert(typedNode);
            llvm::Value* length = gla::MakeIntConstant(context, typedNode->getType().getOuterArraySize());

            glaBuilder->clearAccessChain();
            glaBuilder->setAccessChainRValue(length);
        }
        return false;

    case glslang::EOpFtransform:
        {
            // TODO: back-end functionality: if this needs to support decomposition, need to simulate
            // access to the external gl_Vertex and gl_ModelViewProjectionMatrix.
            // For now, pass in dummy arguments, which are thrown away anyway
            // if ftransform is consumed by the backend without decomposition.
            llvm::Value* vertex = glaBuilder->createVariable(gla::Builder::ESQGlobal, 0, llvm::VectorType::get(gla::GetFloatType(context), 4),
                                                                         0, 0, "gl_Vertex_sim");
            llvm::Value* matrix = glaBuilder->createVariable(gla::Builder::ESQGlobal, 0, llvm::VectorType::get(gla::GetFloatType(context), 4),
                                                                         0, 0, "gl_ModelViewProjectionMatrix_sim");

            result = glaBuilder->createIntrinsicCall(GetMdPrecision(node->getType()), llvm::Intrinsic::gla_fFixedTransform, glaBuilder->createLoad(vertex), glaBuilder->createLoad(matrix));
            glaBuilder->clearAccessChain();
            glaBuilder->setAccessChainRValue(result);
        }
        return false;

    case glslang::EOpEmitVertex:
    case glslang::EOpEndPrimitive:
    case glslang::EOpBarrier:
    case glslang::EOpMemoryBarrier:
    case glslang::EOpMemoryBarrierAtomicCounter:
    case glslang::EOpMemoryBarrierBuffer:
    case glslang::EOpMemoryBarrierImage:
    case glslang::EOpMemoryBarrierShared:
    case glslang::EOpGroupMemoryBarrier:
        // These all have 0 operands and will naturally finish up in the createIntrinsic code below for 0 operands
        break;

    case glslang::EOpEmitStreamVertex:
    case glslang::EOpEndStreamPrimitive:
        // These all have 1 operand and will naturally finish up in the createIntrinsic code below for 1 operand
        break;

    default:
        break;
    }

    //
    // See if it maps to a regular operation or intrinsic.
    //

    if (binOp != glslang::EOpNull) {
        glaBuilder->clearAccessChain();
        node->getSequence()[0]->traverse(this);
        llvm::Value* left = glaBuilder->accessChainLoad(GetMdPrecision(node->getSequence()[0]->getAsTyped()->getType()));

        glaBuilder->clearAccessChain();
        node->getSequence()[1]->traverse(this);
        llvm::Value* right = glaBuilder->accessChainLoad(GetMdPrecision(node->getSequence()[1]->getAsTyped()->getType()));

        if (binOp == glslang::EOpOuterProduct)
            result = glaBuilder->createMatrixMultiply(precision, left, right);
        else if (gla::IsAggregate(left) && binOp == glslang::EOpMul)
            result = glaBuilder->createMatrixOp(precision, llvm::Instruction::FMul, left, right);
        else
            result = createBinaryOperation(binOp, precision, left, right, node->getSequence()[0]->getAsTyped()->getType().getBasicType() == glslang::EbtUint, reduceComparison);

        // code above should only make binOp that exists in createBinaryOperation
        assert(result);

        glaBuilder->clearAccessChain();
        glaBuilder->setAccessChainRValue(result);

        return false;
    }

    glslang::TIntermSequence& glslangOperands = node->getSequence();
    std::vector<llvm::Value*> operands;
    for (int i = 0; i < (int)glslangOperands.size(); ++i) {
        glaBuilder->clearAccessChain();
        glslangOperands[i]->traverse(this);
        llvm::Value* arg;
        if (argNeedsLValue(node, i))
            arg = glaBuilder->accessChainGetLValue();
        else
            arg = glaBuilder->accessChainLoad(GetMdPrecision(glslangOperands[i]->getAsTyped()->getType()));
        operands.push_back(arg);
    }
    switch (glslangOperands.size()) {
    case 0:
        result = createIntrinsic(node->getOp());
        break;
    case 1:
        result = createUnaryIntrinsic(node->getOp(), precision, operands.front());
        break;
    default:
        // Check first for intrinsics that can be done natively
        if (node->getOp() == glslang::EOpMix && 
              gla::GetBasicTypeID(operands.front()) == llvm::Type::IntegerTyID)
            result = llvmBuilder.CreateSelect(operands[2], operands[1], operands[0]);
        else
            result = createIntrinsic(node->getOp(), precision, operands, glslangOperands.front()->getAsTyped()->getBasicType() == glslang::EbtUint);
        break;
    }

    if (! result)
        gla::UnsupportedFunctionality("glslang aggregate", gla::EATContinue);
    else {
        glaBuilder->clearAccessChain();
        glaBuilder->setAccessChainRValue(result);

        return false;
    }

    return true;
}

bool TGlslangToTopTraverser::visitSelection(glslang::TVisit /* visit */, glslang::TIntermSelection* node)
{
    // This path handles both if-then-else and ?:
    // The if-then-else has a node type of void, while
    // ?: has a non-void node type
    llvm::Value* result = 0;
    if (node->getBasicType() != glslang::EbtVoid) {
        // don't handle this as just on-the-fly temporaries, because there will be two names
        // and better to leave SSA to LLVM passes
        result = glaBuilder->createVariable(gla::Builder::ESQLocal, 0, convertGlslangToGlaType(node->getType()), 0, 0, leftName ? leftName : "ternary");
    }

    // emit the condition before doing anything with selection
    node->getCondition()->traverse(this);

    // make an "if" based on the value created by the condition
    gla::Builder::If ifBuilder(glaBuilder->accessChainLoad(gla::EMpNone), glaBuilder);

    if (node->getTrueBlock()) {
        // emit the "then" statement
        node->getTrueBlock()->traverse(this);
        if (result)
            glaBuilder->createStore(glaBuilder->accessChainLoad(GetMdPrecision(node->getTrueBlock()->getAsTyped()->getType())), result);
    }

    if (node->getFalseBlock()) {
        ifBuilder.makeBeginElse();
        // emit the "else" statement
        node->getFalseBlock()->traverse(this);
        if (result)
            glaBuilder->createStore(glaBuilder->accessChainLoad(GetMdPrecision(node->getFalseBlock()->getAsTyped()->getType())), result);
    }

    ifBuilder.makeEndIf();

    if (result) {
        // GLSL only has r-values as the result of a :?, but
        // if we have an l-value, that can be more efficient if it will
        // become the base of a complex r-value expression, because the
        // next layer copies r-values into memory to use the GEP mechanism
        glaBuilder->clearAccessChain();
        glaBuilder->setAccessChainLValue(result);
    }

    return false;
}

bool TGlslangToTopTraverser::visitSwitch(glslang::TVisit /* visit */, glslang::TIntermSwitch* node)
{
    // emit and get the condition before doing anything with switch
    node->getCondition()->traverse(this);
    llvm::Value* condition = glaBuilder->accessChainLoad(GetMdPrecision(node->getCondition()->getAsTyped()->getType()));

    // browse the children to sort out code segments
    int defaultSegment = -1;
    std::vector<TIntermNode*> codeSegments;
    glslang::TIntermSequence& sequence = node->getBody()->getSequence();
    std::vector<llvm::ConstantInt*> caseValues;
    std::vector<int> valueToSegment(sequence.size());  // note: probably not all are used, it is an overestimate
    for (glslang::TIntermSequence::iterator c = sequence.begin(); c != sequence.end(); ++c) {
        TIntermNode* child = *c;
        if (child->getAsBranchNode() && child->getAsBranchNode()->getFlowOp() == glslang::EOpDefault)
            defaultSegment = codeSegments.size();
        else if (child->getAsBranchNode() && child->getAsBranchNode()->getFlowOp() == glslang::EOpCase) {
            valueToSegment[caseValues.size()] = codeSegments.size();
            caseValues.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 
                                                        child->getAsBranchNode()->getExpression()->getAsConstantUnion()->getConstArray()[0].getIConst(), 
                                                        false));
        } else
            codeSegments.push_back(child);
    }


    // handle the case where the last code segment is missing, due to no code 
    // statements between the last case and the end of the switch statement
    if ((int)codeSegments.size() == valueToSegment[caseValues.size() - 1])
        codeSegments.push_back(0);

    // make the switch statement
    std::vector<llvm::BasicBlock*> segmentBB;
    glaBuilder->makeSwitch(condition, codeSegments.size(), caseValues, valueToSegment, defaultSegment, segmentBB);

    // emit all the code in the segments
    breakForLoop.push(false);
    for (unsigned int s = 0; s < codeSegments.size(); ++s) {
        glaBuilder->nextSwitchSegment(segmentBB, s);
        if (codeSegments[s])
            codeSegments[s]->traverse(this);
        else
            glaBuilder->addSwitchBreak();
    }
    breakForLoop.pop();

    glaBuilder->endSwitch(segmentBB);

    return false;
}

void TGlslangToTopTraverser::visitConstantUnion(glslang::TIntermConstantUnion* node)
{
    int nextConst = 0;
    llvm::Constant* constant = createLLVMConstant(node->getType(), node->getConstArray(), nextConst);
    glaBuilder->clearAccessChain();
    if (node->getType().isArray() || node->getType().isStruct() || node->getType().isMatrix()) {
        // for aggregrates, make a global constant to base access chains off of
        llvm::Value* lvalue = glaBuilder->createVariable(gla::Builder::ESQConst, 0, constant->getType(), constant, 0, leftName ? leftName : "lconst");
        glaBuilder->setAccessChainLValue(lvalue);
    } else {
        // for non-aggregates, just use directly;
        glaBuilder->setAccessChainRValue(constant);
    }
}

bool TGlslangToTopTraverser::visitLoop(glslang::TVisit /* visit */, glslang::TIntermLoop* node)
{
    // body emission needs to know what the for-loop terminal is when it sees a "continue"
    loopTerminal.push(node->getTerminal());

    glaBuilder->makeNewLoop();

    bool bodyOut = false;
    if (! node->testFirst()) {
        glaBuilder->completeLoopHeaderWithoutTest();
        if (node->getBody()) {
            breakForLoop.push(true);
            node->getBody()->traverse(this);
            breakForLoop.pop();
        }
        bodyOut = true;
        glaBuilder->makeBranchToLoopEndTest();
    }

    if (node->getTest()) {
        // the AST only contained the test, not the branch, we have to add it
        node->getTest()->traverse(this);
        llvm::Value* condition = glaBuilder->accessChainLoad(GetMdPrecision(node->getTest()->getType()));
        glaBuilder->makeLoopTest(condition);
    }

    if (! bodyOut && node->getBody()) {
        breakForLoop.push(true);
        node->getBody()->traverse(this);
        breakForLoop.pop();
    }

    if (loopTerminal.top())
        loopTerminal.top()->traverse(this);

    glaBuilder->closeLoop();

    loopTerminal.pop();

    return false;
}

bool TGlslangToTopTraverser::visitBranch(glslang::TVisit /* visit */, glslang::TIntermBranch* node)
{
    if (node->getExpression())
        node->getExpression()->traverse(this);

    switch (node->getFlowOp()) {
    case glslang::EOpKill:
        glaBuilder->makeDiscard(inMain);
        break;
    case glslang::EOpBreak:
        if (breakForLoop.top())
            glaBuilder->makeLoopExit();
        else
            glaBuilder->addSwitchBreak();
        break;
    case glslang::EOpContinue:
        if (loopTerminal.top())
            loopTerminal.top()->traverse(this);
        glaBuilder->makeLoopBackEdge();
        break;
    case glslang::EOpReturn:
        if (inMain)
            glaBuilder->makeMainReturn();
        else if (node->getExpression())
            glaBuilder->makeReturn(false, glaBuilder->accessChainLoad(GetMdPrecision(node->getExpression()->getType())));
        else
            glaBuilder->makeReturn();

        glaBuilder->clearAccessChain();
        break;

    default:
        gla::UnsupportedFunctionality("branch type");
        break;
    }

    return false;
}

gla::Builder::EStorageQualifier TGlslangToTopTraverser::mapStorageClass(const glslang::TQualifier& qualifier) const
{
    switch (qualifier.storage) {
    case glslang::EvqTemporary:
    case glslang::EvqConstReadOnly:
    case glslang::EvqConst:
        return gla::Builder::ESQLocal;
    case glslang::EvqGlobal:
        return gla::Builder::ESQGlobal;
    case glslang::EvqShared:
        return gla::Builder::ESQShared;
    case glslang::EvqVaryingIn:
    case glslang::EvqFragCoord:
    case glslang::EvqPointCoord:
    case glslang::EvqFace:
    case glslang::EvqVertexId:
    case glslang::EvqInstanceId:
        // Pipeline reads: If we are here, it must be to create a shadow which
        // will shadow the actual pipeline reads, which must still be done elsewhere.
        // The top builder will make a global shadow for ESQInput.
        return gla::Builder::ESQInput;
    case glslang::EvqVaryingOut:
    case glslang::EvqPosition:
    case glslang::EvqPointSize:
    case glslang::EvqClipVertex:
    case glslang::EvqFragColor:
    case glslang::EvqFragDepth:
        return gla::Builder::ESQOutput;
    case glslang::EvqUniform:
        return gla::Builder::ESQUniform;
    case glslang::EvqBuffer:
        return gla::Builder::ESQBuffer;
    case glslang::EvqIn:
    case glslang::EvqOut:
    case glslang::EvqInOut:
        // parameter qualifiers should not come through here
    default:
        gla::UnsupportedFunctionality("glslang qualifier", gla::EATContinue);
        return gla::Builder::ESQLocal;
    }
}

llvm::Value* TGlslangToTopTraverser::createLLVMVariable(const glslang::TIntermSymbol* node)
{
    gla::Builder::EStorageQualifier storageQualifier = mapStorageClass(node->getQualifier());
    if (node->getBasicType() == glslang::EbtSampler)
        storageQualifier = gla::Builder::ESQResource;

    std::string name(node->getName().c_str());

    llvm::Type *llvmType = convertGlslangToGlaType(node->getType());

    return glaBuilder->createVariable(storageQualifier, 0, llvmType, 0, 0, name);
}

llvm::Type* TGlslangToTopTraverser::convertGlslangToGlaType(const glslang::TType& type)
{
    llvm::Type *glaType;

    switch (type.getBasicType()) {
    case glslang::EbtVoid:
        glaType = gla::GetVoidType(context);
        if (type.isArray())
            gla::UnsupportedFunctionality("array of void");
        break;
    case glslang::EbtFloat:
        glaType = gla::GetFloatType(context);
        break;
    case glslang::EbtDouble:
        gla::UnsupportedFunctionality("basic type: double", gla::EATContinue);
        break;
    case glslang::EbtBool:
        glaType = gla::GetBoolType(context);
        break;
    case glslang::EbtInt:
    case glslang::EbtAtomicUint:
    case glslang::EbtSampler:
        glaType = gla::GetIntType(context);
        break;
    case glslang::EbtUint:
        glaType = gla::GetUintType(context);
        break;
    case glslang::EbtStruct:
    case glslang::EbtBlock:
        {
            const glslang::TTypeList* glslangStruct = type.getStruct();
            std::vector<llvm::Type*> structFields;
            llvm::StructType* structType = structMap[glslangStruct];
            if (structType) {
                // If we've seen this struct type, return it
                glaType = structType;
            } else {
                // Create a vector of struct types for LLVM to consume
                int memberDelta = 0;  // how much the member's index changes from glslang to gla, normally 0, except sometimes for blocks
                if (type.getBasicType() == glslang::EbtBlock)
                    memberRemapper[glslangStruct].resize(glslangStruct->size());
                for (int i = 0; i < (int)glslangStruct->size(); i++) {
                    glslang::TType& glslangType = *(*glslangStruct)[i].type;
                    if (glslangType.hiddenMember())
                        ++memberDelta;
                    else {
                        if (type.getBasicType() == glslang::EbtBlock)
                            memberRemapper[glslangStruct][i] = i - memberDelta;
                        structFields.push_back(convertGlslangToGlaType(glslangType));
                    }
                }
                structType = llvm::StructType::create(context, structFields, type.getTypeName().c_str());
                structMap[glslangStruct] = structType;
                glaType = structType;
            }
        }
        break;
    default:
        gla::UnsupportedFunctionality("basic type");
        break;
    }

    if (type.isMatrix())        
        glaType = glaBuilder->getMatrixType(glaType, type.getMatrixCols(), type.getMatrixRows());
    else {
        // If this variable has a vector element count greater than 1, create an LLVM vector
        if (type.getVectorSize() > 1)
            glaType = llvm::VectorType::get(glaType, type.getVectorSize());
    }

    if (type.isArray()) {
        if (type.isArray() && type.getOuterArraySize() == glslang::UnsizedArraySize && type.getQualifier().storage != glslang::EvqBuffer) {
            gla::UnsupportedFunctionality("implicitly-sized array", gla::EATContinue);
            glaType = llvm::ArrayType::get(glaType, UnknownArraySize);
        } if (type.isArray() && type.getOuterArraySize() == glslang::UnsizedArraySize && type.getQualifier().storage == glslang::EvqBuffer) {
            //
            // Runtime array design.
            //
            // If this is the last member of a buffer block, it is the beginning of an array
            // of unknown size.  That would work well as a pointer to an element of the array:
            //
            //    glaType = glaBuilder->getPointerType(glaType, mapStorageClass(type.getQualifier()), 0);
            //
            // This could then be recognized by translation code a pointer to base array calculations off of,
            // e.g., in the middle of an access chain, encapsulating it.
            // 
            // However, the actual memory will be laid out with elements of the array; there
            // won't be a member that is a pointer to the elements.  If the LLVM type reflects
            // this, the pointer will come from computing the GEP of the first element, not from
            // loading the member.  This can't be encapsulated; generating code will have to emit accesses
            // in two steps; 1) to get the GEP of the first element, and 2) to compute the indexed
            // array element.
            //
            // With the latter approach, the LLVM type loses the information about whether the last
            // member is a single element or the beginning of an array of elements.  If this information
            // is needed downstream, it will come from metadata (EMioBufferBlockMemberArrayed).
            //
            // With the latter approach, glaType is already the type of the element, so there is nothing to do here.
            //
        } else {
            for (int d = type.getArraySizes()->getNumDims() - 1; d >= 0; --d)
                glaType = llvm::ArrayType::get(glaType, type.getArraySizes()->getDimSize(d));
        }
    }

    return glaType;
}

bool TGlslangToTopTraverser::isShaderEntrypoint(const glslang::TIntermAggregate* node)
{
    return node->getName() == "main(";
}

void TGlslangToTopTraverser::makeFunctions(const glslang::TIntermSequence& glslFunctions)
{
    for (int f = 0; f < (int)glslFunctions.size(); ++f) {
        glslang::TIntermAggregate* glslFunction = glslFunctions[f]->getAsAggregate();

        // TODO: compile-time performance: find a way to skip this loop if we aren't
        // a child of the root node of the compilation unit, which should be the only
        // one holding a list of functions.
        if (! glslFunction || glslFunction->getOp() != glslang::EOpFunction || isShaderEntrypoint(glslFunction))
            continue;

        std::vector<llvm::Type*> paramTypes;
        glslang::TIntermSequence& parameters = glslFunction->getSequence()[0]->getAsAggregate()->getSequence();

        // At call time, space should be allocated for all the arguments,
        // and pointers to that space passed to the function as the formal parameters.
        for (int i = 0; i < (int)parameters.size(); ++i) {
            llvm::Type* type = convertGlslangToGlaType(parameters[i]->getAsTyped()->getType());
            paramTypes.push_back(llvm::PointerType::get(type, gla::GlobalAddressSpace));
        }

        llvm::BasicBlock* functionBlock;
        llvm::Function *function = glaBuilder->makeFunctionEntry(convertGlslangToGlaType(glslFunction->getType()), glslFunction->getName().c_str(),
                                                                 paramTypes, &functionBlock);
        function->addFnAttr(llvm::Attribute::AlwaysInline);

        // Visit parameter list again to create mappings to local variables and set attributes.
        llvm::Function::arg_iterator arg = function->arg_begin();
        for (int i = 0; i < (int)parameters.size(); ++i, ++arg)
            symbolValues[parameters[i]->getAsSymbolNode()->getId()] = &(*arg);

        // Track function to emit/call later
        functionMap[glslFunction->getName().c_str()] = function;
    }
}

void TGlslangToTopTraverser::handleFunctionEntry(const glslang::TIntermAggregate* node)
{
    // LLVM functions should already be in the functionMap from the prepass 
    // that called makeFunctions.
    llvm::Function* function = functionMap[node->getName().c_str()];
    llvm::BasicBlock& functionBlock = function->getEntryBlock();
    llvmBuilder.SetInsertPoint(&functionBlock);
}

// If a calling node has to pass an l-value to a built-in function, return true.
// TODO: generalize.  Today, this is only the first argument to atomic operations,
// so that's all that is checked.
bool TGlslangToTopTraverser::argNeedsLValue(const glslang::TIntermOperator* node, int arg)
{
    if (arg > 0)
        return false;

    switch (node->getOp()) {
    case glslang::EOpAtomicAdd:
    case glslang::EOpAtomicMin:
    case glslang::EOpAtomicMax:
    case glslang::EOpAtomicAnd:
    case glslang::EOpAtomicOr:
    case glslang::EOpAtomicXor:
    case glslang::EOpAtomicExchange:
    case glslang::EOpAtomicCompSwap:
        return true;
    default:
        return false;
    }
}

void TGlslangToTopTraverser::translateArguments(glslang::TIntermOperator* node, std::vector<llvm::Value*>& arguments)
{
    if (node->getAsAggregate()) {
        const glslang::TIntermSequence& glslangArguments = node->getAsAggregate()->getSequence();
        for (int i = 0; i < (int)glslangArguments.size(); ++i) {
            glaBuilder->clearAccessChain();
            glslangArguments[i]->traverse(this);
            arguments.push_back(glaBuilder->accessChainLoad(GetMdPrecision(glslangArguments[i]->getAsTyped()->getType())));
        }
    } else {
        glslang::TIntermUnary& glslangArgument = *node->getAsUnaryNode();
        glaBuilder->clearAccessChain();
        glslangArgument.getOperand()->traverse(this);
        arguments.push_back(glaBuilder->accessChainLoad(GetMdPrecision(glslangArgument.getAsTyped()->getType())));
    }
}

llvm::Value* TGlslangToTopTraverser::handleTextureCall(glslang::TIntermOperator* node)
{
    if (! node->isImage() && ! node->isTexture())
        return nullptr;

    std::vector<llvm::Value*> arguments;
    translateArguments(node, arguments);

    const glslang::TSampler sampler = node->getAsAggregate() ? node->getAsAggregate()->getSequence()[0]->getAsTyped()->getType().getSampler()
                                                             : node->getAsUnaryNode()->getOperand()->getAsTyped()->getType().getSampler();

    gla::ESamplerType samplerType;
    switch (sampler.dim) {
    case glslang::Esd1D:       samplerType = gla::ESampler1D;      break;
    case glslang::Esd2D:       samplerType = gla::ESampler2D;      break;
    case glslang::Esd3D:       samplerType = gla::ESampler3D;      break;
    case glslang::EsdCube:     samplerType = gla::ESamplerCube;    break;
    case glslang::EsdRect:     samplerType = gla::ESampler2DRect;  break;
    case glslang::EsdBuffer:   samplerType = gla::ESamplerBuffer;  break;
    default:
        gla::UnsupportedFunctionality("sampler type");
        break;
    }
    if (sampler.ms)
        samplerType = gla::ESampler2DMS;

    glslang::TCrackedTextureOp cracked;
    node->crackTexture(sampler, cracked);

    // Steer off queries
    if (cracked.query || node->getOp() == glslang::EOpImageQuerySize || node->getOp() == glslang::EOpImageQuerySamples)
        return handleTexImageQuery(node, cracked, arguments, samplerType);

    // Steer off image accesses
    if (sampler.image)
        return handleImageAccess(node, arguments, samplerType, sampler.type == glslang::EbtUint);

    // Handle texture accesses...

    int texFlags = 0;

    if (sampler.arrayed)
        texFlags |= gla::ETFArrayed;

    if (sampler.shadow)
        texFlags |= gla::ETFShadow;

    return handleTextureAccess(node, cracked, arguments, samplerType, texFlags);
}

llvm::Value* TGlslangToTopTraverser::handleTexImageQuery(const glslang::TIntermOperator* node, const glslang::TCrackedTextureOp& cracked, const std::vector<llvm::Value*>& arguments, gla::ESamplerType samplerType)
{
    gla::EMdPrecision precision = GetMdPrecision(node->getType());

    switch (node->getOp()) {
    case glslang::EOpTextureQuerySize:
    case glslang::EOpImageQuerySize:
    {
        llvm::Value* lastArg = nullptr;
        llvm::Intrinsic::ID intrinsicID;

        if (node->getOp() == glslang::EOpImageQuerySize) {
            intrinsicID = llvm::Intrinsic::gla_queryImageSize;
        } else if (samplerType == gla::ESampler2DMS || samplerType == gla::ESamplerBuffer || samplerType == gla::ESampler2DRect) {
            lastArg = 0;
            intrinsicID = llvm::Intrinsic::gla_queryTextureSizeNoLod;
        } else {
            assert(arguments.size() > 1);
            lastArg = arguments[1];
            intrinsicID = llvm::Intrinsic::gla_queryTextureSize;
        }

        return glaBuilder->createTextureQueryCall(precision,
                                                  intrinsicID,
                                                  convertGlslangToGlaType(node->getType()),
                                                  MakeIntConstant(context, samplerType),
                                                  arguments[0], lastArg, leftName);
    }
    case glslang::EOpTextureQueryLod:
    {
        gla::UnsupportedFunctionality("textureQueryLod");
        return glaBuilder->createTextureQueryCall(precision,
                                                    llvm::Intrinsic::gla_fQueryTextureLod,
                                                    convertGlslangToGlaType(node->getType()), 
                                                    MakeIntConstant(context, samplerType), 
                                                    arguments[0], arguments[1], leftName);
    }
    case glslang::EOpTextureQueryLevels:
        gla::UnsupportedFunctionality("textureQueryLevels");
        return nullptr;
    default:
        gla::UnsupportedFunctionality("texture/image query");
        return nullptr;
    }
}

llvm::Value* TGlslangToTopTraverser::handleImageAccess(const glslang::TIntermOperator* node, const std::vector<llvm::Value*>& arguments, gla::ESamplerType samplerType, bool isUnsigned)
{
    // set the arguments
    gla::Builder::TextureParameters params = {};
    params.ETPSampler = arguments[0];
    params.ETPCoords = arguments[1];

    gla::EImageOp imageOp = gla::EImageNoop;
    switch (node->getOp()) {
    case glslang::EOpImageLoad:           imageOp = gla::EImageLoad;           break;
    case glslang::EOpImageStore:          imageOp = gla::EImageStore;          break;
    case glslang::EOpImageAtomicAdd:      imageOp = gla::EImageAtomicAdd;      break;
    case glslang::EOpImageAtomicMin:      imageOp = isUnsigned ? gla::EImageAtomicUMin : gla::EImageAtomicSMin; break;
    case glslang::EOpImageAtomicMax:      imageOp = isUnsigned ? gla::EImageAtomicUMax : gla::EImageAtomicSMax; break;
    case glslang::EOpImageAtomicAnd:      imageOp = gla::EImageAtomicAnd;      break;
    case glslang::EOpImageAtomicOr:       imageOp = gla::EImageAtomicOr;       break;
    case glslang::EOpImageAtomicXor:      imageOp = gla::EImageAtomicXor;      break;
    case glslang::EOpImageAtomicExchange: imageOp = gla::EImageAtomicExchange; break;
    case glslang::EOpImageAtomicCompSwap: imageOp = gla::EImageAtomicCompSwap; break;
    default:
        gla::UnsupportedFunctionality("image access");
        break;
    }

    if (imageOp != gla::EImageLoad) {
        if (imageOp == gla::EImageAtomicCompSwap) {
            params.ETPCompare = arguments[2];
            params.ETPData = arguments[3];
        } else
            params.ETPData = arguments[2];
    }

    return glaBuilder->createImageCall(GetMdPrecision(node->getType()), convertGlslangToGlaType(node->getType()), samplerType, imageOp, params, leftName);
}

llvm::Value* TGlslangToTopTraverser::handleTextureAccess(const glslang::TIntermOperator* node, const glslang::TCrackedTextureOp& cracked, 
                                                         const std::vector<llvm::Value*>& arguments, gla::ESamplerType samplerType, int texFlags)
{
    if (cracked.lod) {
        texFlags |= gla::ETFLod;
        texFlags |= gla::ETFBiasLodArg;
    }

    if (cracked.proj)
        texFlags |= gla::ETFProjected;

    if (cracked.offset || cracked.offsets) {
        texFlags |= gla::ETFOffsetArg;
        if (cracked.offsets)
            texFlags |= gla::ETFOffsets;
    }

    if (cracked.fetch) {
        texFlags |= gla::ETFFetch;
        switch (samplerType) {
        case gla::ESampler1D:
        case gla::ESampler2D:
        case gla::ESampler3D:
            texFlags |= gla::ETFLod;
            texFlags |= gla::ETFBiasLodArg;
            break;
        case gla::ESampler2DMS:
            texFlags |= gla::ETFSampleArg;
            texFlags |= gla::ETFBiasLodArg;
        default:
            break;
        }
    }

    if (cracked.gather) {
        texFlags |= gla::ETFGather;
        if (texFlags & gla::ETFShadow)
            texFlags |= gla::ETFRefZArg;
    }

    // check for bias argument
    if (! (texFlags & gla::ETFLod) && ! (texFlags & gla::ETFGather) && ! (texFlags & gla::ETFSampleArg)) {
        int nonBiasArgCount = 2;
        if (texFlags & gla::ETFOffsetArg)
            ++nonBiasArgCount;
        if (texFlags & gla::ETFBiasLodArg)
            ++nonBiasArgCount;
        if (cracked.grad)
            nonBiasArgCount += 2;

        if ((int)arguments.size() > nonBiasArgCount) {
            texFlags |= gla::ETFBias;
            texFlags |= gla::ETFBiasLodArg;
        }
    }

    // check for comp argument
    if ((texFlags & gla::ETFGather) && ! (texFlags & gla::ETFShadow)) {
        int nonCompArgCount = 2;
        if (texFlags & gla::ETFOffsetArg)
            ++nonCompArgCount;
        if ((int)arguments.size() > nonCompArgCount)
            texFlags |= gla::ETFComponentArg;
    }

    // set the arguments
    gla::Builder::TextureParameters params = {};
    params.ETPSampler = arguments[0];
    params.ETPCoords = arguments[1];
    int extraArgs = 0;
    if ((texFlags & gla::ETFLod) || (texFlags & gla::ETFSampleArg)) {
        params.ETPBiasLod = arguments[2];
        ++extraArgs;
    }
    if (cracked.grad) {
        params.ETPGradX = arguments[2 + extraArgs];
        params.ETPGradY = arguments[3 + extraArgs];
        extraArgs += 2;
    }
    if (texFlags & gla::ETFRefZArg) {
        params.ETPShadowRef = arguments[2 + extraArgs];
        ++extraArgs;
    }
    if (texFlags & gla::ETFOffsetArg) {
        params.ETPOffset = arguments[2 + extraArgs];
        ++extraArgs;
    }
    if ((texFlags & gla::ETFBias) || (texFlags & gla::ETFComponentArg)) {
        params.ETPBiasLod = arguments[2 + extraArgs];
        ++extraArgs;
    }

    return glaBuilder->createTextureCall(GetMdPrecision(node->getType()), convertGlslangToGlaType(node->getType()), samplerType, texFlags, params, leftName);
}

llvm::Value* TGlslangToTopTraverser::handleUserFunctionCall(const glslang::TIntermAggregate* node)
{
    // Overall design is to allocate new space for all arguments and pass 
    // pointers to the arguments.
    //
    // For input arguments, they could be expressions, and their value could be
    // overwritten without impacting anything in the caller, so store the answer
    // and pass a pointer to it.

    // Grab the function's pointer from the previously created function
    llvm::Function* function = functionMap[node->getName().c_str()];
    if (! function)
        return 0;

    // First step:  Allocate the space for the arguments and build llvm
    // pointers to it as the passed in arguments.
    llvm::SmallVector<llvm::Value*, 4> llvmArgs;
    llvm::Function::arg_iterator param;
    llvm::Function::arg_iterator end = function->arg_end();
    for (param = function->arg_begin(); param != end; ++param) {
        // param->getType() should be a pointer, we need the type it points to
        llvm::Value* space = glaBuilder->createVariable(gla::Builder::ESQLocal, 0, param->getType()->getContainedType(0), 0, 0, "param");
        llvmArgs.push_back(space);
    }

    // Copy-in time...
    // Compute the access chains of output argument l-values before making the call,
    // to be used after making the call.  Also compute r-values of inputs and store
    // them into the space allocated above.
    const glslang::TIntermSequence& glslangArgs = node->getSequence();
    const glslang::TQualifierList& qualifiers = node->getQualifierList();
    llvm::SmallVector<gla::Builder::AccessChain, 2> lValuesOut;
    for (int i = 0; i < (int)glslangArgs.size(); ++i) {
        // build l-value
        glaBuilder->clearAccessChain();
        glslangArgs[i]->traverse(this);
        if (qualifiers[i] == glslang::EvqOut || qualifiers[i] == glslang::EvqInOut) {
            // save l-value
            lValuesOut.push_back(glaBuilder->getAccessChain());
        }
        if (qualifiers[i] == glslang::EvqIn || qualifiers[i] == glslang::EvqConstReadOnly || qualifiers[i] == glslang::EvqInOut) {
            // process r-value
            glaBuilder->createStore(glaBuilder->accessChainLoad(GetMdPrecision(glslangArgs[i]->getAsTyped()->getType())), llvmArgs[i]);
        }
    }

    // Make the call
    llvm::Value* result = llvmBuilder.Insert(llvm::CallInst::Create(function, llvmArgs));

    // Copy-out time...
    llvm::SmallVector<gla::Builder::AccessChain, 2>::iterator savedIt = lValuesOut.begin();
    for (int i = 0; i < (int)glslangArgs.size(); ++i) {
        if (qualifiers[i] == glslang::EvqOut || qualifiers[i] == glslang::EvqInOut) {
            glaBuilder->setAccessChain(*savedIt);
            llvm::Value* output = glaBuilder->createLoad(llvmArgs[i]);
            glaBuilder->accessChainStore(output);
            ++savedIt;
        }
    }

    return result;
}

// Intended for return values that are Top IR structures, but GLSL out params.
// Move the member of the structure to the out param.
void TGlslangToTopTraverser::storeResultMemberToOperand(llvm::Value* structure, int member, TIntermNode& node)
{
    llvm::Value* memberVal = llvmBuilder.CreateExtractValue(structure, member);
    glaBuilder->clearAccessChain();
    node.traverse(this);
    glaBuilder->accessChainStore(memberVal);
}

// Intended for return values that are Top IR structures, but GLSL out params.
// Move the member of the structure to the out param.
void TGlslangToTopTraverser::storeResultMemberToReturnValue(llvm::Value* structure, int member)
{
    llvm::Value* result = llvmBuilder.CreateExtractValue(structure, member);
    glaBuilder->clearAccessChain();
    glaBuilder->setAccessChainRValue(result);
}

llvm::Value* TGlslangToTopTraverser::createBinaryOperation(glslang::TOperator op, gla::EMdPrecision precision, llvm::Value* left, llvm::Value* right, bool isUnsigned, bool reduceComparison)
{
    llvm::Instruction::BinaryOps binOp = llvm::Instruction::BinaryOps(0);
    bool needsPromotion = true;
    bool leftIsFloat = (gla::GetBasicTypeID(left) == llvm::Type::FloatTyID);
    bool comparison = false;

    switch (op) {
    case glslang::EOpAdd:
    case glslang::EOpAddAssign:
        if (leftIsFloat)
            binOp = llvm::Instruction::FAdd;
        else
            binOp = llvm::Instruction::Add;
        break;
    case glslang::EOpSub:
    case glslang::EOpSubAssign:
        if (leftIsFloat)
            binOp = llvm::Instruction::FSub;
        else
            binOp = llvm::Instruction::Sub;
        break;
    case glslang::EOpMul:
    case glslang::EOpMulAssign:
    case glslang::EOpVectorTimesScalar:
    case glslang::EOpVectorTimesScalarAssign:
    case glslang::EOpVectorTimesMatrixAssign:
    case glslang::EOpMatrixTimesScalarAssign:
    case glslang::EOpMatrixTimesMatrixAssign:
        if (leftIsFloat)
            binOp = llvm::Instruction::FMul;
        else
            binOp = llvm::Instruction::Mul;
        break;
    case glslang::EOpDiv:
    case glslang::EOpDivAssign:
        if (leftIsFloat)
            binOp = llvm::Instruction::FDiv;
        else if (isUnsigned)
            binOp = llvm::Instruction::UDiv;
        else
            binOp = llvm::Instruction::SDiv;
        break;
    case glslang::EOpMod:
    case glslang::EOpModAssign:
        if (leftIsFloat)
            binOp = llvm::Instruction::FRem;
        else if (isUnsigned)
            binOp = llvm::Instruction::URem;
        else
            binOp = llvm::Instruction::SRem;
        break;
    case glslang::EOpRightShift:
    case glslang::EOpRightShiftAssign:
        if (isUnsigned)
            binOp = llvm::Instruction::LShr;
        else
            binOp = llvm::Instruction::AShr;
        break;
    case glslang::EOpLeftShift:
    case glslang::EOpLeftShiftAssign:
        binOp = llvm::Instruction::Shl;
        break;
    case glslang::EOpAnd:
    case glslang::EOpAndAssign:
        binOp = llvm::Instruction::And;
        break;
    case glslang::EOpInclusiveOr:
    case glslang::EOpInclusiveOrAssign:
    case glslang::EOpLogicalOr:
        binOp = llvm::Instruction::Or;
        break;
    case glslang::EOpExclusiveOr:
    case glslang::EOpExclusiveOrAssign:
    case glslang::EOpLogicalXor:
        binOp = llvm::Instruction::Xor;
        break;
    case glslang::EOpLogicalAnd:
        assert(gla::IsBoolean(left->getType()) && gla::IsScalar(left->getType()));
        assert(gla::IsBoolean(right->getType()) && gla::IsScalar(right->getType()));
        needsPromotion = false;
        binOp = llvm::Instruction::And;
        break;

    case glslang::EOpLessThan:
    case glslang::EOpGreaterThan:
    case glslang::EOpLessThanEqual:
    case glslang::EOpGreaterThanEqual:
    case glslang::EOpEqual:
    case glslang::EOpNotEqual:
        comparison = true;
        break;
    default:
        break;
    }

    if (binOp != 0) {
        if (gla::IsAggregate(left) || gla::IsAggregate(right)) {
            switch (op) {
            case glslang::EOpVectorTimesMatrixAssign:
            case glslang::EOpMatrixTimesScalarAssign:
            case glslang::EOpMatrixTimesMatrixAssign:
                return glaBuilder->createMatrixMultiply(precision, left, right);
            default:
                return glaBuilder->createMatrixOp(precision, binOp, left, right);
            }
        }

        if (needsPromotion)
            glaBuilder->promoteScalar(precision, left, right);

        llvm::Value* value = llvmBuilder.CreateBinOp(binOp, left, right);
        glaBuilder->setInstructionPrecision(value, precision);

        return value;
    }

    if (! comparison)
        return 0;

    // Comparison instructions

    if (reduceComparison && (gla::IsVector(left) || gla::IsAggregate(left))) {
        assert(op == glslang::EOpEqual || op == glslang::EOpNotEqual);

        return glaBuilder->createCompare(precision, left, right, op == glslang::EOpEqual);
    }

    if (leftIsFloat) {
        llvm::FCmpInst::Predicate pred = llvm::FCmpInst::Predicate(0);
        switch (op) {
        case glslang::EOpLessThan:
            pred = llvm::FCmpInst::FCMP_OLT;
            break;
        case glslang::EOpGreaterThan:
            pred = llvm::FCmpInst::FCMP_OGT;
            break;
        case glslang::EOpLessThanEqual:
            pred = llvm::FCmpInst::FCMP_OLE;
            break;
        case glslang::EOpGreaterThanEqual:
            pred = llvm::FCmpInst::FCMP_OGE;
            break;
        case glslang::EOpEqual:
            pred = llvm::FCmpInst::FCMP_OEQ;
            break;
        case glslang::EOpNotEqual:
            pred = llvm::FCmpInst::FCMP_ONE;
            break;
        default:
            break;
        }

        if (pred != 0) {
            llvm::Value* result = llvmBuilder.CreateFCmp(pred, left, right);
            glaBuilder->setInstructionPrecision(result, precision);

            return result;
        }
    } else {
        llvm::ICmpInst::Predicate pred = llvm::ICmpInst::Predicate(0);
        if (isUnsigned) {
            switch (op) {
            case glslang::EOpLessThan:
                pred = llvm::ICmpInst::ICMP_ULT;
                break;
            case glslang::EOpGreaterThan:
                pred = llvm::ICmpInst::ICMP_UGT;
                break;
            case glslang::EOpLessThanEqual:
                pred = llvm::ICmpInst::ICMP_ULE;
                break;
            case glslang::EOpGreaterThanEqual:
                pred = llvm::ICmpInst::ICMP_UGE;
                break;
            case glslang::EOpEqual:
                pred = llvm::ICmpInst::ICMP_EQ;
                break;
            case glslang::EOpNotEqual:
                pred = llvm::ICmpInst::ICMP_NE;
                break;
            default:
                break;
            }
        } else {
            switch (op) {
            case glslang::EOpLessThan:
                pred = llvm::ICmpInst::ICMP_SLT;
                break;
            case glslang::EOpGreaterThan:
                pred = llvm::ICmpInst::ICMP_SGT;
                break;
            case glslang::EOpLessThanEqual:
                pred = llvm::ICmpInst::ICMP_SLE;
                break;
            case glslang::EOpGreaterThanEqual:
                pred = llvm::ICmpInst::ICMP_SGE;
                break;
            case glslang::EOpEqual:
                pred = llvm::ICmpInst::ICMP_EQ;
                break;
            case glslang::EOpNotEqual:
                pred = llvm::ICmpInst::ICMP_NE;
                break;
            default:
                break;
            }
        }

        if (pred != 0) {
            llvm::Value* result = llvmBuilder.CreateICmp(pred, left, right);
            glaBuilder->setInstructionPrecision(result, precision);

            return result;
        }
    }

    return 0;
}

llvm::Value* TGlslangToTopTraverser::createUnaryOperation(glslang::TOperator op, gla::EMdPrecision precision, llvm::Value* operand)
{
    // Unary ops that map to llvm operations
    switch (op) {
    case glslang::EOpNegative:
        if (gla::IsAggregate(operand)) {
            // emulate by subtracting from 0.0
            llvm::Value* zero = gla::MakeFloatConstant(context, 0.0);

            return glaBuilder->createMatrixOp(precision, llvm::Instruction::FSub, zero, operand);
        }

        llvm::Value* result;
        if (gla::GetBasicTypeID(operand) == llvm::Type::FloatTyID)
            result = llvmBuilder.CreateFNeg(operand);
        else
            result = llvmBuilder.CreateNeg (operand);
        glaBuilder->setInstructionPrecision(result, precision);

        return result;

    case glslang::EOpLogicalNot:
    case glslang::EOpVectorLogicalNot:
    case glslang::EOpBitwiseNot:
        return llvmBuilder.CreateNot(operand);
    
    case glslang::EOpDeterminant:
        return glaBuilder->createMatrixDeterminant(precision, operand);
    case glslang::EOpMatrixInverse:
        return glaBuilder->createMatrixInverse(precision, operand);
    case glslang::EOpTranspose:
        return glaBuilder->createMatrixTranspose(precision, operand);
    default:
        return 0;
    }
}

llvm::Value* TGlslangToTopTraverser::createConversion(glslang::TOperator op, gla::EMdPrecision precision, llvm::Type* destType, llvm::Value* operand)
{
    llvm::Instruction::CastOps castOp = llvm::Instruction::CastOps(0);
    switch (op) {
    case glslang::EOpConvIntToBool:
    case glslang::EOpConvUintToBool:
    case glslang::EOpConvFloatToBool:
        {
            // any non-zero should return true
            llvm::Value* zero;
            if (op == glslang::EOpConvFloatToBool)
                zero = gla::MakeFloatConstant(context, 0.0f);
            else
                zero = gla::MakeIntConstant(context, 0);

            if (gla::GetComponentCount(operand) > 1)
                zero = glaBuilder->smearScalar(gla::EMpNone, zero, operand->getType());

            return createBinaryOperation(glslang::EOpNotEqual, precision, operand, zero, false, false);
        }

    case glslang::EOpConvIntToFloat:
        castOp = llvm::Instruction::SIToFP;
        break;
    case glslang::EOpConvBoolToFloat:
        castOp = llvm::Instruction::UIToFP;
        break;
    case glslang::EOpConvUintToFloat:
        castOp = llvm::Instruction::UIToFP;
        break;

    case glslang::EOpConvFloatToInt:
        castOp = llvm::Instruction::FPToSI;
        break;
    case glslang::EOpConvBoolToInt:
        // GLSL says true is converted to 1
        castOp = llvm::Instruction::ZExt;
        break;
    case glslang::EOpConvUintToInt:

        return operand;

    case glslang::EOpConvBoolToUint:
        // GLSL says true is converted to 1
        castOp = llvm::Instruction::ZExt;
        break;
    case glslang::EOpConvFloatToUint:
        castOp = llvm::Instruction::FPToUI;
        break;
    case glslang::EOpConvIntToUint:

        return operand;

    case glslang::EOpConvDoubleToInt:
    case glslang::EOpConvDoubleToBool:
    case glslang::EOpConvDoubleToFloat:
    case glslang::EOpConvDoubleToUint:
    case glslang::EOpConvIntToDouble:
    case glslang::EOpConvUintToDouble:
    case glslang::EOpConvFloatToDouble:
    case glslang::EOpConvBoolToDouble:
        gla::UnsupportedFunctionality("double conversion");
        break;
    default:
        break;
    }

    if (castOp == 0)

        return 0;

    llvm::Value* result = llvmBuilder.CreateCast(castOp, operand, destType);
    glaBuilder->setInstructionPrecision(result, precision);

    return result;
}

llvm::Value* TGlslangToTopTraverser::createUnaryIntrinsic(glslang::TOperator op, gla::EMdPrecision precision, llvm::Value* operand)
{
    // Unary ops that require an intrinsic
    llvm::Intrinsic::ID intrinsicID = llvm::Intrinsic::ID(0);

    switch (op) {
    case glslang::EOpRadians:
        intrinsicID = llvm::Intrinsic::gla_fRadians;
        break;
    case glslang::EOpDegrees:
        intrinsicID = llvm::Intrinsic::gla_fDegrees;
        break;

    case glslang::EOpSin:
        intrinsicID = llvm::Intrinsic::gla_fSin;
        break;
    case glslang::EOpCos:
        intrinsicID = llvm::Intrinsic::gla_fCos;
        break;
    case glslang::EOpTan:
        intrinsicID = llvm::Intrinsic::gla_fTan;
        break;
    case glslang::EOpAcos:
        intrinsicID = llvm::Intrinsic::gla_fAcos;
        break;
    case glslang::EOpAsin:
        intrinsicID = llvm::Intrinsic::gla_fAsin;
        break;
    case glslang::EOpAtan:
        intrinsicID = llvm::Intrinsic::gla_fAtan;
        break;

    case glslang::EOpAcosh:
        intrinsicID = llvm::Intrinsic::gla_fAcosh;
        break;
    case glslang::EOpAsinh:
        intrinsicID = llvm::Intrinsic::gla_fAsinh;
        break;
    case glslang::EOpAtanh:
        intrinsicID = llvm::Intrinsic::gla_fAtanh;
        break;
    case glslang::EOpTanh:
        intrinsicID = llvm::Intrinsic::gla_fTanh;
        break;
    case glslang::EOpCosh:
        intrinsicID = llvm::Intrinsic::gla_fCosh;
        break;
    case glslang::EOpSinh:
        intrinsicID = llvm::Intrinsic::gla_fSinh;
        break;

    case glslang::EOpLength:
        intrinsicID = llvm::Intrinsic::gla_fLength;
        break;
    case glslang::EOpNormalize:
        intrinsicID = llvm::Intrinsic::gla_fNormalize;
        break;

    case glslang::EOpExp:
        intrinsicID = llvm::Intrinsic::gla_fExp;
        break;
    case glslang::EOpLog:
        intrinsicID = llvm::Intrinsic::gla_fLog;
        break;
    case glslang::EOpExp2:
        intrinsicID = llvm::Intrinsic::gla_fExp2;
        break;
    case glslang::EOpLog2:
        intrinsicID = llvm::Intrinsic::gla_fLog2;
        break;
    case glslang::EOpSqrt:
        intrinsicID = llvm::Intrinsic::gla_fSqrt;
        break;
    case glslang::EOpInverseSqrt:
        intrinsicID = llvm::Intrinsic::gla_fInverseSqrt;
        break;

    case glslang::EOpFloor:
        intrinsicID = llvm::Intrinsic::gla_fFloor;
        break;
    case glslang::EOpTrunc:
        intrinsicID = llvm::Intrinsic::gla_fRoundZero;
        break;
    case glslang::EOpRound:
        intrinsicID = llvm::Intrinsic::gla_fRoundFast;
        break;
    case glslang::EOpRoundEven:
        intrinsicID = llvm::Intrinsic::gla_fRoundEven;
        break;
    case glslang::EOpCeil:
        intrinsicID = llvm::Intrinsic::gla_fCeiling;
        break;
    case glslang::EOpFract:
        intrinsicID = llvm::Intrinsic::gla_fFraction;
        break;

    case glslang::EOpIsNan:
        intrinsicID = llvm::Intrinsic::gla_fIsNan;
        break;
    case glslang::EOpIsInf:
        intrinsicID = llvm::Intrinsic::gla_fIsInf;
        break;

    case glslang::EOpFloatBitsToInt:
    case glslang::EOpFloatBitsToUint:
        intrinsicID = llvm::Intrinsic::gla_fFloatBitsToInt;
        break;
    case glslang::EOpIntBitsToFloat:
    case glslang::EOpUintBitsToFloat:
        intrinsicID = llvm::Intrinsic::gla_fIntBitsTofloat;
        break;
    case glslang::EOpPackSnorm2x16:
        intrinsicID = llvm::Intrinsic::gla_fPackSnorm2x16;
        break;
    case glslang::EOpUnpackSnorm2x16:
        intrinsicID = llvm::Intrinsic::gla_fUnpackSnorm2x16;
        break;
    case glslang::EOpPackUnorm2x16:
        intrinsicID = llvm::Intrinsic::gla_fPackUnorm2x16;
        break;
    case glslang::EOpUnpackUnorm2x16:
        intrinsicID = llvm::Intrinsic::gla_fUnpackUnorm2x16;
        break;
    case glslang::EOpPackHalf2x16:
        intrinsicID = llvm::Intrinsic::gla_fPackHalf2x16;
        break;
    case glslang::EOpUnpackHalf2x16:
        intrinsicID = llvm::Intrinsic::gla_fUnpackHalf2x16;
        break;
    case glslang::EOpPackUnorm4x8:
        intrinsicID = llvm::Intrinsic::gla_fPackUnorm4x8;
        break;
    case glslang::EOpUnpackUnorm4x8:
        intrinsicID = llvm::Intrinsic::gla_fUnpackUnorm4x8;
        break;
    case glslang::EOpPackSnorm4x8:
        intrinsicID = llvm::Intrinsic::gla_fPackSnorm4x8;
        break;
    case glslang::EOpUnpackSnorm4x8:
        intrinsicID = llvm::Intrinsic::gla_fUnpackSnorm4x8;
        break;

    case glslang::EOpDPdx:
        intrinsicID = llvm::Intrinsic::gla_fDFdx;
        break;
    case glslang::EOpDPdy:
        intrinsicID = llvm::Intrinsic::gla_fDFdy;
        break;
    case glslang::EOpFwidth:
        intrinsicID = llvm::Intrinsic::gla_fFilterWidth;
        break;
    case glslang::EOpInterpolateAtCentroid:
        intrinsicID = llvm::Intrinsic::gla_interpolateAtCentroid;
        break;

    case glslang::EOpAny:
        intrinsicID = llvm::Intrinsic::gla_any;
        break;
    case glslang::EOpAll:
        intrinsicID = llvm::Intrinsic::gla_all;
        break;

    case glslang::EOpAbs:
        if (gla::GetBasicTypeID(operand) == llvm::Type::FloatTyID)
            intrinsicID = llvm::Intrinsic::gla_fAbs;
        else
            intrinsicID = llvm::Intrinsic::gla_abs;
        break;
    case glslang::EOpSign:
        if (gla::GetBasicTypeID(operand) == llvm::Type::FloatTyID)
            intrinsicID = llvm::Intrinsic::gla_fSign;
        else
            intrinsicID = llvm::Intrinsic::gla_sign;
        break;
    case glslang::EOpModf:
        intrinsicID = llvm::Intrinsic::gla_fModF;
        break;
    case glslang::EOpFrexp:
        intrinsicID = llvm::Intrinsic::gla_fFrexp;
        break;

    case glslang::EOpEmitStreamVertex:
        glaBuilder->setExplicitPipelineCopyOut();
        glaBuilder->copyOutPipeline();
        intrinsicID = llvm::Intrinsic::gla_emitStreamVertex;
        break;
    case glslang::EOpEndStreamPrimitive:
        intrinsicID = llvm::Intrinsic::gla_endStreamPrimitive;
        break;

    case glslang::EOpAtomicCounterIncrement:
        intrinsicID = llvm::Intrinsic::gla_atomicCounterIncrement;
        break;
    case glslang::EOpAtomicCounterDecrement:
        intrinsicID = llvm::Intrinsic::gla_atomicCounterDecrement;
        break;
    case glslang::EOpAtomicCounter:
        intrinsicID = llvm::Intrinsic::gla_atomicCounterLoad;
        break;
    case glslang::EOpBitFieldReverse:
        intrinsicID = llvm::Intrinsic::gla_bitReverse;
        break;
    case glslang::EOpBitCount:
        intrinsicID = llvm::Intrinsic::gla_bitCount;
        break;
    case glslang::EOpFindLSB:
        intrinsicID = llvm::Intrinsic::gla_findLSB;
        break;
    case glslang::EOpFindMSB:
        intrinsicID = llvm::Intrinsic::gla_sFindMSB;
        break;

    default:
        break;
    }

    if (intrinsicID != 0)
        return glaBuilder->createIntrinsicCall(precision, intrinsicID, operand, leftName);

    return 0;
}

llvm::Value* TGlslangToTopTraverser::createIntrinsic(glslang::TOperator op, gla::EMdPrecision precision, std::vector<llvm::Value*>& operands, bool isUnsigned)
{
    // Binary ops that require an intrinsic
    llvm::Value* result = 0;
    llvm::Intrinsic::ID intrinsicID = llvm::Intrinsic::ID(0);

    switch (op) {
    case glslang::EOpMin:
        if (gla::GetBasicTypeID(operands.front()) == llvm::Type::FloatTyID)
            intrinsicID = llvm::Intrinsic::gla_fMin;
        else if (isUnsigned)
            intrinsicID = llvm::Intrinsic::gla_uMin;
        else
            intrinsicID = llvm::Intrinsic::gla_sMin;
        break;
    case glslang::EOpMax:
        if (gla::GetBasicTypeID(operands.front()) == llvm::Type::FloatTyID)
            intrinsicID = llvm::Intrinsic::gla_fMax;
        else if (isUnsigned)
            intrinsicID = llvm::Intrinsic::gla_uMax;
        else
            intrinsicID = llvm::Intrinsic::gla_sMax;
        break;
    case glslang::EOpPow:
        if (gla::GetBasicTypeID(operands.front()) == llvm::Type::FloatTyID)
            intrinsicID = llvm::Intrinsic::gla_fPow;
        else
            intrinsicID = llvm::Intrinsic::gla_fPowi;
        break;
    case glslang::EOpDot:
        switch (gla::GetComponentCount(operands[0])) {
        case 2:
            intrinsicID = llvm::Intrinsic::gla_fDot2;
            break;
        case 3:
            intrinsicID = llvm::Intrinsic::gla_fDot3;
            break;
        case 4:
            intrinsicID = llvm::Intrinsic::gla_fDot4;
            break;
        default:
            assert(! "bad component count for dot");
            break;
        }
        break;
    case glslang::EOpFma:
        if (gla::GetBasicTypeID(operands.front()) == llvm::Type::FloatTyID)
            intrinsicID = llvm::Intrinsic::gla_fFma;
        else if (isUnsigned)
            intrinsicID = llvm::Intrinsic::gla_uFma;
        else
            intrinsicID = llvm::Intrinsic::gla_sFma;
        break;
    case glslang::EOpLdexp:
        intrinsicID = llvm::Intrinsic::gla_fLdexp;
        break;
    case glslang::EOpAddCarry:
        intrinsicID = llvm::Intrinsic::gla_addCarry;
        break;
    case glslang::EOpSubBorrow:
        intrinsicID = llvm::Intrinsic::gla_subBorrow;
        break;
    case glslang::EOpUMulExtended:
        intrinsicID = llvm::Intrinsic::gla_umulExtended;
        break;
    case glslang::EOpIMulExtended:
        intrinsicID = llvm::Intrinsic::gla_smulExtended;
        break;
    case glslang::EOpBitfieldExtract:
        if (isUnsigned)
            intrinsicID = llvm::Intrinsic::gla_uBitFieldExtract;
        else
            intrinsicID = llvm::Intrinsic::gla_sBitFieldExtract;
        break;
    case glslang::EOpBitfieldInsert:
        intrinsicID = llvm::Intrinsic::gla_bitFieldInsert;
        break;

    case glslang::EOpAtan:
        intrinsicID = llvm::Intrinsic::gla_fAtan2;
        break;

    case glslang::EOpClamp:
        if (gla::GetBasicTypeID(operands.front()) == llvm::Type::FloatTyID)
            intrinsicID = llvm::Intrinsic::gla_fClamp;
        else if (isUnsigned)
            intrinsicID = llvm::Intrinsic::gla_uClamp;
        else
            intrinsicID = llvm::Intrinsic::gla_sClamp;
        break;
    case glslang::EOpMix:
        if (gla::GetBasicTypeID(operands.front()) == llvm::Type::IntegerTyID)
            assert(0 && "integer type mix handled with intrinsic");
        else if (gla::GetBasicTypeID(operands.back()) == llvm::Type::IntegerTyID)
            intrinsicID = llvm::Intrinsic::gla_fbMix;
        else
            intrinsicID = llvm::Intrinsic::gla_fMix;
        break;
    case glslang::EOpStep:
        intrinsicID = llvm::Intrinsic::gla_fStep;
        break;
    case glslang::EOpSmoothStep:
        intrinsicID = llvm::Intrinsic::gla_fSmoothStep;
        break;

    case glslang::EOpDistance:
        intrinsicID = llvm::Intrinsic::gla_fDistance;
        break;
    case glslang::EOpCross:
        intrinsicID = llvm::Intrinsic::gla_fCross;
        break;
    case glslang::EOpFaceForward:
        intrinsicID = llvm::Intrinsic::gla_fFaceForward;
        break;
    case glslang::EOpReflect:
        intrinsicID = llvm::Intrinsic::gla_fReflect;
        break;
    case glslang::EOpRefract:
        intrinsicID = llvm::Intrinsic::gla_fRefract;
        break;
    case glslang::EOpInterpolateAtOffset:
        intrinsicID = llvm::Intrinsic::gla_interpolateAtOffset;
        break;
    case glslang::EOpInterpolateAtSample:
        intrinsicID = llvm::Intrinsic::gla_interpolateAtSample;
        break;

    case glslang::EOpAtomicAdd:
        intrinsicID = llvm::Intrinsic::gla_atomicAdd;
        break;
    case glslang::EOpAtomicMin:
        if (isUnsigned)
            intrinsicID = llvm::Intrinsic::gla_uAtomicMin;
        else
            intrinsicID = llvm::Intrinsic::gla_sAtomicMin;
        break;
    case glslang::EOpAtomicMax:
        if (isUnsigned)
            intrinsicID = llvm::Intrinsic::gla_uAtomicMax;
        else
            intrinsicID = llvm::Intrinsic::gla_sAtomicMax;
        break;
    case glslang::EOpAtomicAnd:
        intrinsicID = llvm::Intrinsic::gla_atomicAnd;
        break;
    case glslang::EOpAtomicOr:
        intrinsicID = llvm::Intrinsic::gla_atomicOr;
        break;
    case glslang::EOpAtomicXor:
        intrinsicID = llvm::Intrinsic::gla_atomicXor;
        break;
    case glslang::EOpAtomicExchange:
        intrinsicID = llvm::Intrinsic::gla_atomicExchange;
        break;
    case glslang::EOpAtomicCompSwap:
        intrinsicID = llvm::Intrinsic::gla_atomicCompExchange;
        break;

    default:
        break;
    }

    // If intrinsic was assigned, then call the function and return
    if (intrinsicID != 0) {
        switch (operands.size()) {
        case 0:
            result = glaBuilder->createIntrinsicCall(precision, intrinsicID);
            break;
        case 1:
            // should all be handled by createUnaryIntrinsic
            assert(0);
            break;
        case 2:
            result = glaBuilder->createIntrinsicCall(precision, intrinsicID, operands[0], operands[1], leftName ? leftName : "misc2a");
            break;
        case 3:
            result = glaBuilder->createIntrinsicCall(precision, intrinsicID, operands[0], operands[1], operands[2], leftName ? leftName : "misc3a");
            break;
        case 4:
            result = glaBuilder->createIntrinsicCall(precision, intrinsicID, operands[0], operands[1], operands[2], operands[3], leftName ? leftName : "misc4a");
            break;
        default:
            // These do not exist yet
            assert(0 && "intrinsic with more than 3 operands");
            break;
        }
    }

    return result;
}

// Intrinsics with no arguments, no return value, and no precision.
llvm::Value* TGlslangToTopTraverser::createIntrinsic(glslang::TOperator op)
{
    llvm::Value* result = 0;
    llvm::Intrinsic::ID intrinsicID = llvm::Intrinsic::ID(0);

    switch (op) {
    case glslang::EOpEmitVertex:
        glaBuilder->setExplicitPipelineCopyOut();
        glaBuilder->copyOutPipeline();
        intrinsicID = llvm::Intrinsic::gla_emitVertex;
        break;
    case glslang::EOpEndPrimitive:
        intrinsicID = llvm::Intrinsic::gla_endPrimitive;
        break;
    case glslang::EOpBarrier:
        intrinsicID = llvm::Intrinsic::gla_barrier;
        break;
    case glslang::EOpMemoryBarrier:
        intrinsicID = llvm::Intrinsic::gla_memoryBarrier;
        break;
    case glslang::EOpMemoryBarrierAtomicCounter:
        intrinsicID = llvm::Intrinsic::gla_memoryBarrierAtomicCounter;
        break;
    case glslang::EOpMemoryBarrierBuffer:
        intrinsicID = llvm::Intrinsic::gla_memoryBarrierBuffer;
        break;
    case glslang::EOpMemoryBarrierImage:
        intrinsicID = llvm::Intrinsic::gla_memoryBarrierImage;
        break;
    case glslang::EOpMemoryBarrierShared:
        intrinsicID = llvm::Intrinsic::gla_memoryBarrierShared;
        break;
    case glslang::EOpGroupMemoryBarrier:
        intrinsicID = llvm::Intrinsic::gla_groupMemoryBarrier;
        break;
    default:
        break;
    }

    // If intrinsic was assigned, then call the function and return
    if (intrinsicID != 0)
        result = glaBuilder->createIntrinsicCall(intrinsicID);

    return result;
}

// Set up to recursively traverse the structure to read, while flattening it into slots
void TGlslangToTopTraverser::createPipelineRead(glslang::TIntermSymbol* node, llvm::Value* storage, int firstSlot, llvm::MDNode* md)
{
    if (glaBuilder->useLogicalIo())
        return;

    gla::EInterpolationMethod method;
    gla::EInterpolationLocation location;
    GetInterpolationLocationMethod(node->getType(), method, location);
    // For pipeline inputs, and we will generate a fresh pipeline read at each reference,
    // which gets optimized later.
    std::string name(node->getName().c_str());

    std::vector<llvm::Value*> gepChain;
    createPipelineSubread(node->getType(), storage, gepChain, firstSlot, md, name, method, location);
}

// Recursively read the input structure
void TGlslangToTopTraverser::createPipelineSubread(const glslang::TType& type, llvm::Value* storage, std::vector<llvm::Value*>& gepChain, int& slot, llvm::MDNode* md, 
                                                   std::string& name, gla::EInterpolationMethod method, gla::EInterpolationLocation location)
{
    // gla types can be both arrays and matrices or arrays and structures at the same time;
    // make sure to process arrayness first, so it is stripped to get to elements

    if (type.isArray()) {
        // read the array elements, recursively

        int arraySize = type.getOuterArraySize();

        glslang::TType elementType(type, 0);

        if (gepChain.size() == 0)
            gepChain.push_back(gla::MakeIntConstant(context, 0));
        for (int element = 0; element < arraySize; ++element) {
            gepChain.push_back(gla::MakeIntConstant(context, element));
            createPipelineSubread(elementType, storage, gepChain, slot, md, name, method, location);
            gepChain.pop_back();
        }
        if (gepChain.size() == 1)
            gepChain.pop_back();
    } else if (const glslang::TTypeList* typeList = getStructIfIsStruct(type)) {
        if (gepChain.size() == 0)
            gepChain.push_back(gla::MakeIntConstant(context, 0));
        for (int field = 0; field < (int)typeList->size(); ++field) {
            gepChain.push_back(gla::MakeIntConstant(context, field));
            createPipelineSubread(*(*typeList)[field].type, storage, gepChain, slot, md, name, method, location);
            gepChain.pop_back();
        }
        if (gepChain.size() == 1)
            gepChain.pop_back();
        
    } else if (type.isMatrix()) {
        // Read the whole matrix now, one slot at a time.

        int numColumns = type.getMatrixCols();            
        
        glslang::TType columnType(type, 0);
        llvm::Type* readType = convertGlslangToGlaType(columnType);

        // fill in the whole aggregate shadow, slot by slot
        if (gepChain.size() == 0)
            gepChain.push_back(gla::MakeIntConstant(context, 0));
        for (int column = 0; column < numColumns; ++column, ++slot) {
            gepChain.push_back(gla::MakeIntConstant(context, column));               
            llvm::Value* pipeRead = glaBuilder->readPipeline(GetMdPrecision(type), readType, name, slot, md, -1 /*mask*/, method, location);
            llvmBuilder.CreateStore(pipeRead, glaBuilder->createGEP(storage, gepChain));                
            gepChain.pop_back();
        }
        if (gepChain.size() == 1)
            gepChain.pop_back();
    } else {
        llvm::Type* readType = convertGlslangToGlaType(type);
        llvm::Value* pipeRead = glaBuilder->readPipeline(GetMdPrecision(type), readType, name, slot, md, -1 /*mask*/, method, location);
        ++slot;
        if (gepChain.size() > 0)
            llvmBuilder.CreateStore(pipeRead, glaBuilder->createGEP(storage, gepChain));
        else
            llvmBuilder.CreateStore(pipeRead, storage);
    }
}

//
// Find and use the user-specified location as a slot, or if a location was not
// specified, pick the next non-user available slot. User-specified locations
// directly use the location specified, while non-user-specified will use locations
// starting after MaxUserLayoutLocation to avoid collisions.
//
// Ensure enough slots are consumed to cover the size of the data represented by the node symbol.
//
// 'numSlots' means number of GLSL locations when using logical IO.  Note: The design, when used
// for physical-IO slot writes, is inherently contradictory for "arrayed io", like output from 
// a tessellation control shader, where the number of visible locations to use is based on 
// ignoring the outer layer of arrayness, but the number of vec4s to keep track of for dumping
// on exit to main has to be the full number.  This is tested for and a message given.
//
int TGlslangToTopTraverser::assignSlot(glslang::TIntermSymbol* node, bool input, int& numSlots)
{
    // Base the numbers of slots on the front-end's computation, if possible, otherwise estimate it.
    const glslang::TType& type = node->getType();
    if (glslangIntermediate) {
        // Use the array element type if this variable has an extra layer of arrayness
        if (type.isArray() && type.getQualifier().isArrayedIo(glslangIntermediate->getStage())) {
            // See note above.
            //glslang::TType elementType(type, 0);
            //numSlots = glslangIntermediate->computeTypeLocationSize(elementType);
            if (! glaBuilder->useLogicalIo())
                gla::UnsupportedFunctionality("arrayed IO in physical IO mode (use logical IO instead)", gla::EATContinue);
            numSlots = glslangIntermediate->computeTypeLocationSize(type, glslangIntermediate->getStage());
        } else
            numSlots = glslangIntermediate->computeTypeLocationSize(type, glslangIntermediate->getStage());
    } else {
        numSlots = 1;
        if (type.isArray() && ! type.getQualifier().isArrayedIo(glslangIntermediate->getStage()))
            numSlots = type.getOuterArraySize();
        if (type.isStruct() || type.isMatrix() || type.getBasicType() == glslang::EbtDouble)
            gla::UnsupportedFunctionality("complex I/O type; use new glslang C++ interface", gla::EATContinue);
    }

    // Get the index for this interpolant, or create a new unique one
    int slot;
    if (node->getQualifier().hasLocation()) {
        slot = node->getQualifier().layoutLocation;

        return slot;
    }

    // Not found in the symbol, see if we've assigned one before

    std::map<std::string, int>::iterator iter;
    const char* name = node->getName().c_str();
    iter = slotMap.find(name);

    if (slotMap.end() == iter) {
        slotMap[name] = nextSlot;
        nextSlot += numSlots;
    }

    return slotMap[name];
}

llvm::Value* TGlslangToTopTraverser::getSymbolStorage(const glslang::TIntermSymbol* symbol, bool& firstTime)
{
    std::map<int, llvm::Value*>::iterator iter;
    iter = symbolValues.find(symbol->getId());
    llvm::Value* storage;
    if (symbolValues.end() == iter) {
        // it was not found, create it
        firstTime = true;
        storage = createLLVMVariable(symbol);
        symbolValues[symbol->getId()] = storage;
    } else {
        firstTime = false;
        storage = iter->second;
    }

    return storage;
}

// Use 'consts' as the flattened glslang source of scalar constants to recursively
// build the hierarchical LLVM constant.
//
// If there are not enough elements present in 'consts', 0 will be substituted;
// an empty 'consts' can be used to create a fully zeroed LLVM constant.
//
llvm::Constant* TGlslangToTopTraverser::createLLVMConstant(const glslang::TType& glslangType, const glslang::TConstUnionArray& consts, int& nextConst)
{
    // vector of constants for LLVM
    std::vector<llvm::Constant*> llvmConsts;

    // Type is used for struct and array constants
    llvm::Type* type = convertGlslangToGlaType(glslangType);

    if (glslangType.isArray()) {
        glslang::TType elementType(glslangType, 0);
        for (int i = 0; i < glslangType.getOuterArraySize(); ++i)
            llvmConsts.push_back(llvm::dyn_cast<llvm::Constant>(createLLVMConstant(elementType, consts, nextConst)));
    } else if (glslangType.isMatrix()) {
        glslang::TType vectorType(glslangType, 0);
        for (int col = 0; col < glslangType.getMatrixCols(); ++col)
            llvmConsts.push_back(llvm::dyn_cast<llvm::Constant>(createLLVMConstant(vectorType, consts, nextConst)));
    } else if (glslangType.isStruct()) {
        glslang::TVector<glslang::TTypeLoc>::const_iterator iter;
        for (iter = glslangType.getStruct()->begin(); iter != glslangType.getStruct()->end(); ++iter)
            llvmConsts.push_back(llvm::dyn_cast<llvm::Constant>(createLLVMConstant(*iter->type, consts, nextConst)));
    } else {
        // a vector or scalar, both will work the same way
        // this is where we actually consume the constants, rather than walk a tree

        for (unsigned int i = 0; i < (unsigned int)glslangType.getVectorSize(); ++i) {
            bool zero = nextConst >= consts.size();
            switch (glslangType.getBasicType()) {
            case glslang::EbtInt:
                llvmConsts.push_back(gla::MakeIntConstant(context, zero ? 0 : consts[nextConst].getIConst()));
                break;
            case glslang::EbtUint:
                llvmConsts.push_back(gla::MakeUnsignedConstant(context, zero ? 0 : consts[nextConst].getUConst()));
                break;
            case glslang::EbtFloat:
                llvmConsts.push_back(gla::MakeFloatConstant(context, zero ? 0.0f : (float)consts[nextConst].getDConst()));
                break;
            case glslang::EbtDouble:
                llvmConsts.push_back(gla::MakeDoubleConstant(context, zero ? 0.0 : consts[nextConst].getDConst()));
                break;
            case glslang::EbtBool:
                llvmConsts.push_back(gla::MakeBoolConstant(context, zero ? false : consts[nextConst].getBConst()));
                break;
            default:
                gla::UnsupportedFunctionality("scalar or vector element type");
                break;
            }
            ++nextConst;
        }
    }

    return glaBuilder->getConstant(llvmConsts, type);
}

// Make a type proxy that won't be optimized away (we still want the real llvm::Value to get optimized away when it can)
llvm::Value* TGlslangToTopTraverser::MakePermanentTypeProxy(llvm::Type* type, llvm::StringRef name)
{
    // bypass pointers
    while (type->getTypeID() == llvm::Type::PointerTyID)
        type = llvm::dyn_cast<llvm::PointerType>(type)->getContainedType(0);

    // Don't hook this global into the module, that will cause LLVM to optimize it away.
    llvm::Value* typeProxy = new llvm::GlobalVariable(type, true, llvm::GlobalVariable::ExternalLinkage, 0, name + "_typeProxy");
    manager.addToFreeList(typeProxy);

    return typeProxy;
}

llvm::MDNode* TGlslangToTopTraverser::makeMdSampler(const glslang::TType& type, llvm::Type* llvmType, llvm::StringRef name)
{
    // Figure out sampler information, if it's a sampler
    if (type.getBasicType() == glslang::EbtSampler) {
        llvm::Value* typeProxy;
        if (llvmType == nullptr) {
            // Don't hook this global into the module, that will cause LLVM to optimize it away.
            typeProxy = new llvm::GlobalVariable(convertGlslangToGlaType(type), true, llvm::GlobalVariable::ExternalLinkage, 0, "sampler_typeProxy");
            manager.addToFreeList(typeProxy);
        } else
            typeProxy = MakePermanentTypeProxy(llvmType, name);

        return metadata.makeMdSampler(GetMdSampler(type), typeProxy, GetMdSamplerDim(type), type.getSampler().arrayed,
                                      type.getSampler().shadow, GetMdSamplerBaseType(type.getSampler().type));
    } else
        return 0;
}

// Make a !aggregate, hierarchically, in metadata, as per metadata.h,
// for either a block or a structure.
// This function walks the hierarchicy recursively.
// If a structure is used more than once in the hierarchy, it is walked more than once,
// giving a chance to have different majorness (e.g.) each time (LLVM reuses MD nodes when it can).
// 'inheritMatrix' will get corrected each time a top-level block member is visited,
// and should then stay the same while visiting the substructure of that member.
llvm::MDNode* TGlslangToTopTraverser::declareMdType(const glslang::TType& type, gla::EMdTypeLayout inheritMatrix)
{
    // Figure out sampler information if it's a sampler
    llvm::MDNode* samplerMd = makeMdSampler(type, nullptr, "");

    std::vector<llvm::Value*> mdArgs;

    // name of aggregate, if an aggregate (struct or block)
    if (type.isStruct())
        mdArgs.push_back(llvm::MDString::get(context, type.getTypeName().c_str()));
    else
        mdArgs.push_back(llvm::MDString::get(context, ""));

    // !typeLayout
    mdArgs.push_back(metadata.makeMdTypeLayout(GetMdTypeLayout(type, inheritMatrix), GetMdPrecision(type), GetMdSlotLocation(type), samplerMd, -1, GetMdBuiltIn(type),
                                               GetMdBinding(type), GetMdQualifiers(type), GetMdOffset(type, useUniformOffsets)));

    const glslang::TTypeList* typeList = getStructIfIsStruct(type);
    if (typeList) {
        for (int t = 0; t < (int)typeList->size(); ++t) {
            const glslang::TType* fieldType = (*typeList)[t].type;
            if (fieldType->hiddenMember())
                continue;

            // name of member
            mdArgs.push_back(llvm::MDString::get(context, fieldType->getFieldName().c_str()));
            
            // type of member
            llvm::MDNode* mdType = declareMdType(*fieldType, inheritMatrix);
            mdArgs.push_back(mdType);
        }
    }

    return llvm::MDNode::get(context, mdArgs);
}

// Make a !gla.uniform/input/output node, as per metadata.h, selected by "kind"
// Called at the block level.
// If using useSingleTypeTree(), then it is mutually recursive with declareChildMdIo.
llvm::MDNode* TGlslangToTopTraverser::declareMdIo(llvm::StringRef instanceName, const glslang::TType& type, llvm::Type* proxyType, llvm::StringRef proxyName,
                                                  int slot, gla::EMdTypeLayout inheritMatrix, const char* kind)
{
    llvm::MDNode* samplerMd = makeMdSampler(type, proxyType, proxyName);
    gla::EInterpolationMode interpolationMode = -1;
    int location;
    gla::EMdTypeLayout layout = GetMdTypeLayout(type, inheritMatrix);
    gla::EMdInputOutput ioType = GetMdInputOutput(type);

    switch (ioType) {
    case gla::EMioDefaultUniform:
    case gla::EMioUniformBlockMember:
    case gla::EMioBufferBlockMember:
    case gla::EMioBufferBlockMemberArrayed:
        // uniforms
        location = GetMdLocation(type);
        break;

    default:
        // in/out
        gla::EInterpolationMethod interpMethod;
        gla::EInterpolationLocation interpLocation;
        GetInterpolationLocationMethod(type, interpMethod, interpLocation);
        interpolationMode = gla::MakeInterpolationMode(interpMethod, interpLocation);
        location = slot;
        break;
    }

    if (glaBuilder->useSingleTypeTree()) {
        // Make hierarchical type information (a recursive !gla.io node, mutually-recursive with the current function)
        const char* typeName = nullptr;
        llvm::SmallVector<llvm::MDNode*, 10> members;
        if (type.getBasicType() == glslang::EbtStruct || type.getBasicType() == glslang::EbtBlock) {
            typeName = type.getTypeName().c_str();
            declareChildMdIo(type, proxyType, members, inheritMatrix);
        }

        // Make the !typeLayout for this level
        llvm::MDNode* layoutMd = metadata.makeMdTypeLayout(layout, GetMdPrecision(type), location, samplerMd, interpolationMode, GetMdBuiltIn(type),
                                                           GetMdBinding(type), GetMdQualifiers(type), GetMdOffset(type, useUniformOffsets));

        // Make the !gla.uniform/input/output for this level
        llvm::MDNode* ioMd = metadata.makeMdSingleTypeIo(instanceName, typeName, ioType, MakePermanentTypeProxy(proxyType, proxyName), layoutMd, members);

        // If we're top level (should correspond to having 'kind'), add this to the right !gla.XXXX list
        if (kind) {
            llvm::NamedMDNode* namedNode = module->getOrInsertNamedMetadata(kind);
            namedNode->addOperand(ioMd);
        }

        return ioMd;
    } else {
        // Make hierarchical type information (a recursive !aggregate node)
        llvm::MDNode* aggregate = nullptr;
        if (type.getBasicType() == glslang::EbtStruct || type.getBasicType() == glslang::EbtBlock)
            aggregate = declareMdType(type, inheritMatrix);

        // Make the top-level !gla.uniform/input/output node that points to the recursive !aggregate node
        return metadata.makeMdInputOutput(instanceName, kind, ioType, MakePermanentTypeProxy(proxyType, proxyName),
                                          layout, GetMdPrecision(type), location, samplerMd, aggregate,
                                          interpolationMode, GetMdBuiltIn(type), GetMdBinding(type), GetMdQualifiers(type), GetMdOffset(type, useUniformOffsets));
    }
}

// Make a !gla.uniform/input/output child node, as per metadata.h
// Operates mutually recursively with declareMdIo().
// If a structure is used more than once in the hierarchy, it is walked more than once,
// giving a chance to have different majorness (e.g.) each time (LLVM reuses MD nodes when it can).
// 'inheritMatrix' will get corrected each time a top-level block member is visited,
// and should then stay the same while visiting the substructure of that member.
void TGlslangToTopTraverser::declareChildMdIo(const glslang::TType& type, llvm::Type* proxyType, llvm::SmallVector<llvm::MDNode*, 10>& members, gla::EMdTypeLayout inheritMatrix)
{
    const glslang::TTypeList* typeList = getStructIfIsStruct(type);
    if (typeList) {
        // Get the llvm type of the struct holding the members (bypassing arrays and pointers)
        llvm::Type* structType = proxyType;
        while (structType->getTypeID() == llvm::Type::PointerTyID ||
                structType->getTypeID() == llvm::Type::ArrayTyID)
            structType = structType->getContainedType(0);

        int nonHiddenCount = 0;
        for (int t = 0; t < (int)typeList->size(); ++t) {
            const glslang::TType* fieldType = (*typeList)[t].type;
            if (fieldType->hiddenMember())
                continue;
            // build a child md node and add it as an argument
            members.push_back(declareMdIo(fieldType->getFieldName().c_str(), *fieldType, structType->getContainedType(nonHiddenCount), fieldType->getFieldName().c_str(),
                                          GetMdSlotLocation(type), inheritMatrix));
            ++nonHiddenCount;
        }
    }
}

llvm::MDNode* TGlslangToTopTraverser::declareUniformMetadata(glslang::TIntermSymbol* node, llvm::Value* value)
{
    llvm::MDNode* md;
    const std::string name = node->getName().c_str();
    md = uniformMdMap[name];
    if (md)
        return md;

    gla::EMdTypeLayout inheritMatrix = gla::EMtlNone;
    md = declareMdIo(filterMdName(node->getName().c_str()), node->getType(), value->getType(), value->getName(), 0, inheritMatrix, gla::UniformListMdName);
    uniformMdMap[name] = md;

    if (linkageOnly)
        metadata.addNoStaticUse(md);

    return md;
}

// Make metadata node for an 'out' variable/block and associate it with the 
// output-variable cache in the gla builder.
void TGlslangToTopTraverser::setOutputMetadata(glslang::TIntermSymbol* node, llvm::Value* storage, int slot, int numSlots)
{
    gla::EMdTypeLayout inheritMatrix = gla::EMtlNone;
    llvm::MDNode* md = declareMdIo(filterMdName(node->getName().c_str()), node->getType(), storage->getType(), storage->getName(), slot, inheritMatrix, gla::OutputListMdName);

    if (node->getQualifier().invariant)
        module->getOrInsertNamedMetadata(gla::InvariantListMdName)->addOperand(md);

    if (linkageOnly)
        metadata.addNoStaticUse(md);

    glaBuilder->setOutputMetadata(storage, md, slot, numSlots);
}

llvm::MDNode* TGlslangToTopTraverser::makeInputMetadata(glslang::TIntermSymbol* node, llvm::Value* value, int slot)
{
    llvm::MDNode* mdNode = inputMdMap[slot];
    if (mdNode == 0) {
        // set up metadata for pipeline intrinsic read
        gla::EMdTypeLayout inheritMatrix = gla::EMtlNone;
        mdNode = declareMdIo(filterMdName(node->getName().c_str()), node->getType(), value->getType(), value->getName(), slot, inheritMatrix, gla::InputListMdName);
        inputMdMap[slot] = mdNode;
        if (linkageOnly)
            metadata.addNoStaticUse(mdNode);
    }

    return mdNode;
}

//
// Set up the glslang traversal
//

// Glslang C++ interface
void GlslangToTop(const glslang::TIntermediate& intermediate, gla::Manager& manager)
{
    TIntermNode* root = intermediate.getTreeRoot();

    if (root == 0)
        return;

    glslang::GetThreadPoolAllocator().push();
    TGlslangToTopTraverser it(&manager, &intermediate);
    root->traverse(&it);
    glslang::GetThreadPoolAllocator().pop();
}

// Glslang deprecated interface
void GlslangToTop(TIntermNode* root, gla::Manager* manager)
{
    if (root == 0)
        return;

    glslang::GetThreadPoolAllocator().push();
    TGlslangToTopTraverser it(manager, 0);
    root->traverse(&it);
    glslang::GetThreadPoolAllocator().pop();
}
