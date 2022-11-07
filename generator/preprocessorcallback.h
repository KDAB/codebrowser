/****************************************************************************
 * Copyright (C) 2012-2016 Woboq GmbH
 * Olivier Goffart <contact at woboq.com>
 * https://woboq.com/codebrowser.html
 *
 * This file is part of the Woboq Code Browser.
 *
 * Commercial License Usage:
 * Licensees holding valid commercial licenses provided by Woboq may use
 * this file in accordance with the terms contained in a written agreement
 * between the licensee and Woboq.
 * For further information see https://woboq.com/codebrowser.html
 *
 * Alternatively, this work may be used under a Creative Commons
 * Attribution-NonCommercial-ShareAlike 3.0 (CC-BY-NC-SA 3.0) License.
 * http://creativecommons.org/licenses/by-nc-sa/3.0/deed.en_US
 * This license does not allow you to use the code browser to assist the
 * development of your commercial software. If you intent to do so, consider
 * purchasing a commercial licence.
 ****************************************************************************/


#pragma once
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Basic/Version.h>
#include <clang/AST/Decl.h>
#if CLANG_VERSION_MAJOR >= 15
#include <llvm/ADT/Optional.h>
#endif

namespace clang {
class Preprocessor;
}

class Annotator;

class PreprocessorCallback  : public clang::PPCallbacks {
    Annotator &annotator;
    clang::Preprocessor &PP;
    bool disabled = false; // To prevent recurstion
    bool seenPragma = false; // To detect _Pragma in expansion
    bool recoverIncludePath; // If we should try to find the include paths harder

public:
    PreprocessorCallback(Annotator &fm, clang::Preprocessor &PP, bool recoverIncludePath)
        : annotator(fm), PP(PP), recoverIncludePath(recoverIncludePath) {}

#if CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR >= 7
    using MyMacroDefinition = const clang::MacroDefinition &;
#else
    using MyMacroDefinition = const clang::MacroDirective*;
#endif

    void MacroExpands(const clang::Token& MacroNameTok, MyMacroDefinition MD,
                      clang::SourceRange Range, const clang::MacroArgs *Args) override;

    void MacroDefined(const clang::Token &MacroNameTok, const clang::MacroDirective *MD) override;

    void MacroUndefined(const clang::Token &MacroNameTok, MyMacroDefinition MD
#if CLANG_VERSION_MAJOR >= 5
       , const clang::MacroDirective *
#endif
        ) override;

    void InclusionDirective(clang::SourceLocation HashLoc,
                            const clang::Token& IncludeTok,
                            llvm::StringRef FileName,
                            bool IsAngled,
                            clang::CharSourceRange FilenameRange,
#if CLANG_VERSION_MAJOR >= 15
                            llvm::Optional<clang::FileEntryRef> File,
#else
                            const clang::FileEntry* File,
#endif
                            llvm::StringRef SearchPath,
                            llvm::StringRef RelativePath,
                            const clang::Module* Imported
#if CLANG_VERSION_MAJOR >= 7
                            , clang::SrcMgr::CharacteristicKind
#endif

    ) override;

#if CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR < 5
    typedef bool ConditionValueKind;  // It's an enum in clang 3.5
#endif

    void PragmaDirective(clang::SourceLocation Loc, clang::PragmaIntroducerKind Introducer) override
    { seenPragma = true; }

    virtual void If(clang::SourceLocation Loc, clang::SourceRange ConditionRange, ConditionValueKind ConditionValue) override
    { HandlePPCond(Loc, Loc); }
    virtual void Ifndef(clang::SourceLocation Loc, const clang::Token& MacroNameTok, MyMacroDefinition MD) override
    { HandlePPCond(Loc, Loc); Defined(MacroNameTok, MD, Loc); }
    virtual void Ifdef(clang::SourceLocation Loc, const clang::Token& MacroNameTok, MyMacroDefinition MD) override
    { HandlePPCond(Loc, Loc); Defined(MacroNameTok, MD, Loc); }
    virtual void Elif(clang::SourceLocation Loc, clang::SourceRange ConditionRange, ConditionValueKind ConditionValue, clang::SourceLocation IfLoc) override {
        ElifMapping[Loc] = IfLoc;
        HandlePPCond(Loc, IfLoc);
    }
    virtual void Else(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override
    { HandlePPCond(Loc, IfLoc); }
    virtual void Endif(clang::SourceLocation Loc, clang::SourceLocation IfLoc) override
    { HandlePPCond(Loc, IfLoc); }

    virtual void Defined(const clang::Token &MacroNameTok, MyMacroDefinition MD,
                         clang::SourceRange Range) override;

private:
    std::map<clang::SourceLocation, clang::SourceLocation> ElifMapping;     // Map an elif location to the real if;
    void HandlePPCond(clang::SourceLocation Loc, clang::SourceLocation IfLoc);
};
