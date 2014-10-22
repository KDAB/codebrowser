/****************************************************************************
 * Copyright (C) 2012-2014 Woboq GmbH
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

class PreprocessorCallback : public clang::PPCallbacks {
  Annotator &annotator;
  clang::Preprocessor &PP;
  bool disabled = false; // To prevent recurstion

public:
  PreprocessorCallback(Annotator &fm, clang::Preprocessor &PP)
      : annotator(fm), PP(PP) {}

  void MacroExpands(const clang::Token &MacroNameTok,
                    const clang::MacroInfo *MI, clang::SourceRange Range);
#if CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR > 2
  void MacroExpands(const clang::Token &MacroNameTok,
                    const clang::MacroDirective *MD, clang::SourceRange Range,
                    const clang::MacroArgs *Args) override {
    MacroExpands(MacroNameTok, MD->getMacroInfo(), Range);
  }
#endif

#if CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR > 1
  void InclusionDirective(clang::SourceLocation HashLoc,
                          const clang::Token &IncludeTok,
                          llvm::StringRef FileName, bool IsAngled,
                          clang::CharSourceRange FilenameRange,
                          const clang::FileEntry *File,
                          llvm::StringRef SearchPath,
                          llvm::StringRef RelativePath,
                          const clang::Module *Imported) override;
#endif

#if CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR < 5
  typedef bool ConditionValueKind; // It's an enum in clang 3.5
#endif

#if CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR > 3
  virtual void If(clang::SourceLocation Loc, clang::SourceRange ConditionRange,
                  ConditionValueKind ConditionValue) override {
    HandlePPCond(Loc, Loc);
  }
  virtual void Ifndef(clang::SourceLocation Loc,
                      const clang::Token &MacroNameTok,
                      const clang::MacroDirective *MD) override {
    HandlePPCond(Loc, Loc);
  }
  virtual void Ifdef(clang::SourceLocation Loc,
                     const clang::Token &MacroNameTok,
                     const clang::MacroDirective *MD) override {
    HandlePPCond(Loc, Loc);
  }
  virtual void Elif(clang::SourceLocation Loc,
                    clang::SourceRange ConditionRange,
                    ConditionValueKind ConditionValue,
                    clang::SourceLocation IfLoc) override {
    ElifMapping[Loc] = IfLoc;
    HandlePPCond(Loc, IfLoc);
  }
  virtual void Else(clang::SourceLocation Loc,
                    clang::SourceLocation IfLoc) override {
    HandlePPCond(Loc, IfLoc);
  }
  virtual void Endif(clang::SourceLocation Loc,
                     clang::SourceLocation IfLoc) override {
    HandlePPCond(Loc, IfLoc);
  }

private:
  std::map<clang::SourceLocation, clang::SourceLocation>
      ElifMapping; // Map an elif location to the real if;
  void HandlePPCond(clang::SourceLocation Loc, clang::SourceLocation IfLoc);
#endif
};
