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

#include "preprocessorcallback.h"
#include "annotator.h"
#include <clang/Lex/Token.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/Version.h>
#include <llvm/ADT/Twine.h>
#include "stringbuilder.h"
#include "projectmanager.h"


void PreprocessorCallback::MacroExpands(const clang::Token& MacroNameTok,
                                        MyMacroDefinition MD,
                                        clang::SourceRange Range, const clang::MacroArgs *)
{
    if (disabled)
        return;

#if CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR >= 7
    auto *MI = MD.getMacroInfo();
#else
    auto *MI = MD->getMacroInfo();
#endif
    clang::SourceLocation loc = MacroNameTok.getLocation();
    if (!loc.isValid() || !loc.isFileID())
        return;
    clang::SourceManager &sm = annotator.getSourceMgr();
    clang::FileID FID = sm.getFileID(loc);
    if (!annotator.shouldProcess(FID))
        return;

    const char *begin = sm.getCharacterData(Range.getBegin());
    int len = sm.getCharacterData(Range.getEnd()) - begin;
    len += clang::Lexer::MeasureTokenLength(Range.getEnd(), sm, PP.getLangOpts());

    std::string copy(begin, len);
    begin = copy.c_str();
    clang::Lexer lex(loc, PP.getLangOpts(), begin, begin, begin + len);
    std::vector<clang::Token> tokens;
    std::string expansion;

    //Lousely based on code from clang::html::HighlightMacros

    // Lex all the tokens in raw mode, to avoid entering #includes or expanding
    // macros.
    clang::Token tok;
    do {
        lex.LexFromRawLexer(tok);

        // If this is a # at the start of a line, discard it from the token stream.
        // We don't want the re-preprocess step to see #defines, #includes or other
        // preprocessor directives.
        if (tok.is(clang::tok::hash) && tok.isAtStartOfLine())
            continue;

        // If this is a ## token, change its kind to unknown so that repreprocessing
        // it will not produce an error.
        if (tok.is(clang::tok::hashhash))
            tok.setKind(clang::tok::unknown);

        // If this raw token is an identifier, the raw lexer won't have looked up
        // the corresponding identifier info for it.  Do this now so that it will be
        // macro expanded when we re-preprocess it.
        if (tok.is(clang::tok::raw_identifier))
            PP.LookUpIdentifierInfo(tok);

        tokens.push_back(tok);

    } while(!tok.is(clang::tok::eof));

    // Temporarily change the diagnostics object so that we ignore any generated
    // diagnostics from this pass.
    clang::DiagnosticsEngine TmpDiags(PP.getDiagnostics().getDiagnosticIDs(),
                                      &PP.getDiagnostics().getDiagnosticOptions(),
                                      new clang::IgnoringDiagConsumer);

    disabled = true;
    clang::DiagnosticsEngine *OldDiags = &PP.getDiagnostics();
    PP.setDiagnostics(TmpDiags);

    // We don't want pragmas either. Although we filtered out #pragma, removing
    // _Pragma and __pragma is much harder.
    bool pragmasPreviouslyEnabled = PP.getPragmasEnabled();
    PP.setPragmasEnabled(false);
    seenPragma = false;

#if CLANG_VERSION_MAJOR >= 9
    PP.EnterTokenStream(tokens, /*DisableMacroExpansion=*/false, /*IsReinject=*/false);
#elif CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR >= 9
    PP.EnterTokenStream(tokens, /*DisableMacroExpansion=*/false);
#else
    PP.EnterTokenStream(tokens.data(), tokens.size(), false, false);
#endif


    PP.Lex(tok);
    while(tok.isNot(clang::tok::eof)) {
        if (seenPragma) {
            // skip pragma
            while(tok.isNot(clang::tok::eof) && tok.isNot(clang::tok::eod))
                PP.Lex(tok);
            seenPragma = false;
            PP.Lex(tok);
            continue;
        }

        // If the tokens were already space separated, or if they must be to avoid
        // them being implicitly pasted, add a space between them.
        if (tok.hasLeadingSpace())
            expansion += ' ';
           // ConcatInfo.AvoidConcat(PrevPrevTok, PrevTok, Tok)) //FIXME
        // Escape any special characters in the token text.
        expansion += PP.getSpelling(tok);

        if (expansion.size() >= 30 * 1000) {
            // Don't let the macro expansion grow too large.
            expansion += "...";
            while(tok.isNot(clang::tok::eof))
                PP.LexUnexpandedToken(tok);
            break;
        }

        PP.Lex(tok);
    }

    PP.setDiagnostics(*OldDiags);
    PP.setPragmasEnabled(pragmasPreviouslyEnabled);
    disabled = false;

    std::string ref = llvm::Twine("_M/", MacroNameTok.getIdentifierInfo()->getName()).str();

    clang::SourceLocation defLoc = MI->getDefinitionLoc();
    clang::FileID defFID = sm.getFileID(defLoc);
    llvm::SmallString<128> expansionBuffer;
    std::string link;
    std::string dataProj;
    if (defFID != FID) {
        link = annotator.pathTo(FID, defFID, &dataProj);
        if (link.empty()) {
            std::string tag = "class=\"macro\" title=\"" % Generator::escapeAttr(expansion, expansionBuffer)
                % "\" data-ref=\"" % ref % "\"";
            annotator.generator(FID).addTag("span", tag, sm.getFileOffset(loc), MacroNameTok.getLength());
            return;
        }

        if (!dataProj.empty()) {
            dataProj = " data-proj=\"" % dataProj % "\"";
        }
    }

    if (sm.getMainFileID() != defFID) {
        annotator.registerMacro(ref, MacroNameTok.getLocation(), Annotator::Use_Call);
    }

    std::string tag = "class=\"macro\" href=\"" % link % "#" % llvm::Twine(sm.getExpansionLineNumber(defLoc)).str()
        % "\" title=\"" % Generator::escapeAttr(expansion, expansionBuffer)
        % "\" data-ref=\"" % ref % "\"" % dataProj;
    annotator.generator(FID).addTag("a", tag, sm.getFileOffset(loc), MacroNameTok.getLength());
}

void PreprocessorCallback::MacroDefined(const clang::Token& MacroNameTok, const clang::MacroDirective *MD)
{
    clang::SourceLocation loc = MacroNameTok.getLocation();
    if (!loc.isValid() || !loc.isFileID())
        return;

    clang::SourceManager &sm = annotator.getSourceMgr();
    clang::FileID FID = sm.getFileID(loc);
    if (!annotator.shouldProcess(FID))
        return;

    std::string ref = llvm::Twine("_M/", MacroNameTok.getIdentifierInfo()->getName()).str();

    if (sm.getMainFileID() != FID) {
        annotator.registerMacro(ref, MacroNameTok.getLocation(), Annotator::Declaration);
    }

    annotator.generator(FID).addTag("dfn", "class=\"macro\" id=\""% ref %"\" data-ref=\"" % ref % "\"", sm.getFileOffset(loc), MacroNameTok.getLength());
}

void PreprocessorCallback::MacroUndefined(const clang::Token& MacroNameTok, PreprocessorCallback::MyMacroDefinition MD
#if CLANG_VERSION_MAJOR >= 5
       , const clang::MacroDirective *
#endif
            )
{
    clang::SourceLocation loc = MacroNameTok.getLocation();
    if (!loc.isValid() || !loc.isFileID())
        return;

    clang::SourceManager &sm = annotator.getSourceMgr();
    clang::FileID FID = sm.getFileID(loc);
    if (!annotator.shouldProcess(FID))
        return;

    std::string ref = llvm::Twine("_M/", MacroNameTok.getIdentifierInfo()->getName()).str();
    std::string link;
    std::string dataProj;
    clang::SourceLocation defLoc;
    clang::FileID defFID;

    if (MD) {
#if CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR >= 7
        auto *MI = MD.getMacroInfo();
#else
        auto *MI = MD->getMacroInfo();
#endif
        if (MI) {
            defLoc = MI->getDefinitionLoc();
            defFID = sm.getFileID(defLoc);
        }
    }

    if (defFID.isInvalid() || defFID != FID) {
        if (!defFID.isInvalid()) {
            link = annotator.pathTo(FID, defFID, &dataProj);
        }
        if (link.empty()) {
            std::string tag = "class=\"macro\" data-ref=\"" % ref % "\"";
            annotator.generator(FID).addTag("span", tag, sm.getFileOffset(loc), MacroNameTok.getLength());
            return;
        }

        if (!dataProj.empty()) {
            dataProj = " data-proj=\"" % dataProj % "\"";
        }
    }

    if (sm.getMainFileID() != defFID) {
        annotator.registerMacro(ref, MacroNameTok.getLocation(), Annotator::Use_Write);
    }

    std::string tag = "class=\"macro\" href=\"" % link % "#" % llvm::Twine(sm.getExpansionLineNumber(defLoc)).str()
        % "\" data-ref=\"" % ref % "\"" % dataProj;
    annotator.generator(FID).addTag("a", tag, sm.getFileOffset(loc), MacroNameTok.getLength());
}

bool PreprocessorCallback::FileNotFound(llvm::StringRef FileName, llvm::SmallVectorImpl<char> &RecoveryPath)
{
    if (!recoverIncludePath)
        return false;
    clang::SourceLocation currentLoc = static_cast<clang::Lexer *>(PP.getCurrentLexer())->getSourceLocation();
    auto &SM = annotator.getSourceMgr();
    const clang::FileEntry* entry = SM.getFileEntryForID(SM.getFileID(currentLoc));
    if (!entry || llvm::StringRef(entry->getName()).empty())
        return false;
    std::string recovery = annotator.projectManager.includeRecovery(FileName, entry->getName());
    if (recovery.empty() || !llvm::StringRef(recovery).endswith(FileName))
        return false;
    RecoveryPath.clear();
    RecoveryPath.append(recovery.begin(), recovery.begin() + recovery.size() - FileName.size());
    currentLoc.dump(SM);
    llvm::errs() << " WARNING: File not found '" << FileName << "'. Recovering using "
                 << llvm::StringRef(RecoveryPath.data(), RecoveryPath.size()) << "\n";

    return true;
}


void PreprocessorCallback::InclusionDirective(clang::SourceLocation HashLoc, const clang::Token& IncludeTok,
                                              llvm::StringRef FileName, bool IsAngled,
                                              clang::CharSourceRange FilenameRange, const clang::FileEntry* File,
                                              llvm::StringRef SearchPath, llvm::StringRef RelativePath,
                                              const clang::Module* Imported
#if CLANG_VERSION_MAJOR >= 7
                                              , clang::SrcMgr::CharacteristicKind
#endif
   )
{
    if (!HashLoc.isValid() || !HashLoc.isFileID() || !File)
        return;
    clang::SourceManager &sm = annotator.getSourceMgr();
    clang::FileID FID = sm.getFileID(HashLoc);
    if (!annotator.shouldProcess(FID))
        return;

    std::string link = annotator.pathTo(FID, File);
    if (link.empty())
      return;

    auto B = sm.getFileOffset(FilenameRange.getBegin());
    auto E = sm.getFileOffset(FilenameRange.getEnd());

    annotator.generator(FID).addTag("a", "href=\"" % link % "\"", B, E-B);
}

void PreprocessorCallback::Defined(const clang::Token& MacroNameTok, MyMacroDefinition MD,
                                   clang::SourceRange Range)
{
    clang::SourceLocation loc = MacroNameTok.getLocation();
    if (!loc.isValid() || !loc.isFileID())
        return;

    clang::SourceManager &sm = annotator.getSourceMgr();

    clang::FileID FID = sm.getFileID(loc);
    if (!annotator.shouldProcess(FID))
        return;

    std::string ref = llvm::Twine("_M/", MacroNameTok.getIdentifierInfo()->getName()).str();
    std::string link;
    std::string dataProj;
    clang::SourceLocation defLoc;
    clang::FileID defFID;

    if (MD) {
#if CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR >= 7
        auto *MI = MD.getMacroInfo();
#else
        auto *MI = MD->getMacroInfo();
#endif
        if (MI) {
            defLoc = MI->getDefinitionLoc();
            defFID = sm.getFileID(defLoc);
        }
    }

    if (defFID.isInvalid() || defFID != FID) {
        if (!defFID.isInvalid()) {
            link = annotator.pathTo(FID, defFID, &dataProj);
        }
        if (link.empty()) {
            std::string tag = "class=\"macro\" data-ref=\"" % ref % "\"";
            annotator.generator(FID).addTag("span", tag, sm.getFileOffset(loc), MacroNameTok.getLength());
            return;
        }

        if (!dataProj.empty()) {
            dataProj = " data-proj=\"" % dataProj % "\"";
        }
    }

    if (sm.getMainFileID() != defFID) {
        annotator.registerMacro(ref, MacroNameTok.getLocation(), Annotator::Use_Address);
    }

    std::string tag = "class=\"macro\" href=\"" % link % "#" % llvm::Twine(sm.getExpansionLineNumber(defLoc)).str()
        % "\" data-ref=\"" % ref % "\"" % dataProj;
    annotator.generator(FID).addTag("a", tag, sm.getFileOffset(loc), MacroNameTok.getLength());
}

void PreprocessorCallback::HandlePPCond(clang::SourceLocation Loc, clang::SourceLocation IfLoc)
{
    if (!Loc.isValid() || !Loc.isFileID())
        return;

    clang::SourceManager &SM = annotator.getSourceMgr();
    clang::FileID FID = SM.getFileID(Loc);
    if (!annotator.shouldProcess(FID))
        return;

    while(ElifMapping.count(IfLoc)) {
        IfLoc = Loc;
    }

    if (SM.getFileID(IfLoc) != FID) {
        return;
    }

    annotator.generator(FID).addTag("span", ("data-ppcond=\"" + clang::Twine(SM.getExpansionLineNumber(IfLoc)) + "\"").str(),
                                    SM.getFileOffset(Loc), clang::Lexer::MeasureTokenLength(Loc, SM, PP.getLangOpts()));
}
