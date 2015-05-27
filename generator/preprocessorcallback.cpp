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

#include "preprocessorcallback.h"
#include "annotator.h"
#include <clang/Lex/Token.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Basic/Version.h>
#include <llvm/ADT/Twine.h>
#include "stringbuilder.h"


void PreprocessorCallback::MacroExpands(const clang::Token& MacroNameTok,
                                        MyMacroDefinition MD,
                                        clang::SourceRange Range, const clang::MacroArgs *)
{
    if (disabled)
        return;

#if CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR >= 7
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

    PP.EnterTokenStream(tokens.data(), tokens.size(), false, false);

    PP.Lex(tok);
    while(tok.isNot(clang::tok::eof)) {
        // If the tokens were already space separated, or if they must be to avoid
        // them being implicitly pasted, add a space between them.
        if (tok.hasLeadingSpace())
            expansion += ' ';
           // ConcatInfo.AvoidConcat(PrevPrevTok, PrevTok, Tok)) //FIXME
        // Escape any special characters in the token text.
        expansion += PP.getSpelling(tok);
        PP.Lex(tok);
    }

    PP.setDiagnostics(*OldDiags);
    disabled = false;


    clang::SourceLocation defLoc = MI->getDefinitionLoc();
    clang::FileID defFID = sm.getFileID(defLoc);
    llvm::SmallString<128> expansionBuffer;
    std::string link;
    if (defFID != FID) {
        link = annotator.pathTo(FID, defFID);
        if (link.empty()) {
            std::string tag = "class=\"macro\" title=\"" % Generator::escapeAttr(expansion, expansionBuffer) % "\"";
            annotator.generator(FID).addTag("span", tag, sm.getFileOffset(loc), MacroNameTok.getLength());
            return;
        }
    }

    std::string tag = "class=\"macro\" href=\"" % link % "#" % llvm::Twine(sm.getExpansionLineNumber(defLoc)).str()
        % "\" title=\"" % Generator::escapeAttr(expansion, expansionBuffer) % "\"";
    annotator.generator(FID).addTag("a", tag, sm.getFileOffset(loc), MacroNameTok.getLength());
}

void PreprocessorCallback::InclusionDirective(clang::SourceLocation HashLoc, const clang::Token& IncludeTok,
                                              llvm::StringRef FileName, bool IsAngled,
                                              clang::CharSourceRange FilenameRange, const clang::FileEntry* File,
                                              llvm::StringRef SearchPath, llvm::StringRef RelativePath,
                                              const clang::Module* Imported)
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

#if  CLANG_VERSION_MAJOR != 3 || CLANG_VERSION_MINOR > 3
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
#endif
