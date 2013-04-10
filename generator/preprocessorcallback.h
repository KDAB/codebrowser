/****************************************************************************
 * Copyright (C) 2012 Woboq UG (haftungsbeschraenkt)
 * Olivier Goffart <contact at woboq.com>
 * http://woboq.com/codebrowser.html
 *
 * This file is part of the Woboq Code Browser.
 *
 * Commercial License Usage:
 * Licensees holding valid commercial licenses provided by Woboq may use
 * this file in accordance with the terms contained in a written agreement
 * between the licensee and Woboq.
 * For further information see http://woboq.com/codebrowser.html
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

namespace clang {
class Preprocessor;
}

class Annotator;

class PreprocessorCallback  : public clang::PPCallbacks {
    Annotator &annotator;
    clang::Preprocessor &PP;
    bool disabled = false; // To prevent recurstion

public:
    PreprocessorCallback(Annotator &fm, clang::Preprocessor &PP) : annotator(fm), PP(PP) {}

    void MacroExpands(const clang::Token& MacroNameTok, const clang::MacroInfo* MI, clang::SourceRange Range);
#if  CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR > 2
    void MacroExpands(const clang::Token& MacroNameTok, const clang::MacroDirective* MD, clang::SourceRange Range) override
    { MacroExpands(MacroNameTok, MD->getMacroInfo(), Range); }
#endif

    void InclusionDirective(clang::SourceLocation HashLoc, const clang::Token& IncludeTok, llvm::StringRef FileName,
                            bool IsAngled, clang::CharSourceRange FilenameRange, const clang::FileEntry* File,
                            llvm::StringRef SearchPath, llvm::StringRef RelativePath, const clang::Module* Imported) override;
};
