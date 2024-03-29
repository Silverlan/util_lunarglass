//===- GlslManager.h - Translate bottom IR to GLSL ------------------------===//
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
// LunarG's customization of gla::PrivateManager to manage its GLSL back end.
//
//===----------------------------------------------------------------------===//

// LunarGLASS includes
#include "Core/PrivateManager.h"
#include "Backends/GLSL/GlslTarget.h"

// LLVM includes
#include "llvm/IR/LLVMContext.h"

namespace gla {

class GlslManager : public gla::PrivateManager {
public:
    explicit GlslManager(bool obfuscate = false, bool filterInactive = false, int substitutionLevel = 1) :
        obfuscate(obfuscate), filterInactive(filterInactive), substitutionLevel(substitutionLevel)
    {
        createNonreusable();
        backEnd = gla::GetGlslBackEnd();
    }

    virtual ~GlslManager()
    {
        freeNonreusable();
        gla::ReleaseGlslBackEnd(backEnd);
    }

    virtual void clear()
    {
        freeNonreusable();
        createNonreusable();
    }

    virtual void createContext()
    {
        delete context;
        context = new llvm::LLVMContext;
    }

    const char* getGeneratedShader() { return glslBackEndTranslator->getGeneratedShader(); }
    const char* getIndexShader() { return glslBackEndTranslator->getIndexShader(); }

protected:
    void createNonreusable()
    {
        glslBackEndTranslator = gla::GetGlslTranslator(this, obfuscate, filterInactive, substitutionLevel);
        backEndTranslator = glslBackEndTranslator;
    }
    void freeNonreusable()
    {
        gla::ReleaseGlslTranslator(backEndTranslator);
        while (! freeList.empty()) {
            delete freeList.back();
            freeList.pop_back();
        }
        delete module;
        module = 0;
        delete context;
        context = 0;
    }

    GlslTranslator* glslBackEndTranslator;
    bool obfuscate;
    bool filterInactive;
    int substitutionLevel;
};

} // end namespace gla
