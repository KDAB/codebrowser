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

#include "commenthandler.h"
#include "annotator.h"
#include "generator.h"
#include "stringbuilder.h"
#include <cctype>
#include <clang/AST/ASTContext.h>
#include <clang/AST/CommentParser.h>
#include <clang/AST/CommentVisitor.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RawCommentList.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/Version.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Sema.h>
#include <iostream>

clang::NamedDecl *parseDeclarationReference(llvm::StringRef Text, clang::Sema &Sema,
                                            bool isFunction)
{

    clang::Preprocessor &PP = Sema.getPreprocessor();

    auto Buf = llvm::MemoryBuffer::getMemBufferCopy(Text);
    llvm::MemoryBuffer *Buf2 = &*Buf;
#if CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR <= 4
    auto FID = PP.getSourceManager().createFileIDForMemBuffer(Buf);
#else
    auto FID = PP.getSourceManager().createFileID(std::move(Buf));
#endif


#if CLANG_VERSION_MAJOR >= 12
    auto MemBufRef = Buf2->getMemBufferRef();
    clang::Lexer Lex(FID, MemBufRef, PP.getSourceManager(), PP.getLangOpts());
#else
    clang::Lexer Lex(FID, Buf2, PP.getSourceManager(), PP.getLangOpts());
#endif

    auto TuDecl = Sema.getASTContext().getTranslationUnitDecl();
    clang::CXXScopeSpec SS;
    clang::Token Tok, Next;
    Lex.LexFromRawLexer(Tok);

    for (; !Tok.is(clang::tok::eof); Tok = Next) {
        Lex.LexFromRawLexer(Next);
        clang::IdentifierInfo *II = nullptr;
        if (Tok.is(clang::tok::raw_identifier)) {
            II = PP.LookUpIdentifierInfo(Tok);
        }

        if (Tok.is(clang::tok::coloncolon)) {
            SS.MakeGlobal(Sema.getASTContext(), Tok.getLocation());
            continue;
        } else if (Tok.is(clang::tok::identifier)) {

            if (Next.is(clang::tok::coloncolon)) {

                clang::Sema::TemplateTy Template;
                clang::UnqualifiedId Name;
                Name.setIdentifier(II, Tok.getLocation());
                bool dummy;
                auto TemplateKind = Sema.isTemplateName(Sema.getScopeForContext(TuDecl), SS, false,
                                                        Name, {}, false, Template, dummy);
                if (TemplateKind == clang::TNK_Non_template) {
#if CLANG_VERSION_MAJOR >= 4
                    clang::Sema::NestedNameSpecInfo nameInfo(II, Tok.getLocation(),
                                                             Next.getLocation());
                    if (Sema.ActOnCXXNestedNameSpecifier(Sema.getScopeForContext(TuDecl), nameInfo,
                                                         false, SS))
#else
                    if (Sema.ActOnCXXNestedNameSpecifier(Sema.getScopeForContext(TuDecl), *II,
                                                         Tok.getLocation(), Next.getLocation(), {},
                                                         false, SS))
#endif
                    {
                        SS.SetInvalid(Tok.getLocation());
                    }
                } else if (auto T = Template.get().getAsTemplateDecl()) {
                    // FIXME: For template, it is a bit tricky
                    // It is still a bit broken but works in some cases for most normal functions
                    auto T2 = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(T->getTemplatedDecl());
                    if (T2) {
                        Lex.LexFromRawLexer(Tok);
                        if (!Tok.is(clang::tok::raw_identifier))
                            return nullptr;
                        II = PP.LookUpIdentifierInfo(Tok);
                        Lex.LexFromRawLexer(Next);
                        if (!Next.is(clang::tok::eof) && !Next.is(clang::tok::l_paren))
                            return nullptr;
                        auto Result = T2->lookup(II);
#if CLANG_VERSION_MAJOR >= 13
                        if (Result.isSingleResult())
                            return nullptr;
#else
                        if (Result.size() != 1)
                            return nullptr;
#endif
                        auto D = Result.front();
                        if (isFunction
                            && (llvm::isa<clang::RecordDecl>(D)
                                || llvm::isa<clang::ClassTemplateDecl>(D))) {
                            // TODO constructor
                            return nullptr;
                        }
                        return D;
                    }
                }
                Lex.LexFromRawLexer(Next);
                continue;
            }

            if (Next.is(clang::tok::eof) || Next.is(clang::tok::l_paren)) {
                clang::LookupResult Found(Sema, II, Tok.getLocation(),
                                          clang::Sema::LookupOrdinaryName);
                Found.suppressDiagnostics();

                if (SS.isEmpty())
                    Sema.LookupQualifiedName(Found, TuDecl);
                else {
                    clang::DeclContext *DC = Sema.computeDeclContext(SS);
                    Sema.LookupQualifiedName(Found, DC ? DC : TuDecl);
                }


                if (Found.isSingleResult()) {
                    auto Decl = Found.getFoundDecl();
                    if (isFunction
                        && (llvm::isa<clang::RecordDecl>(Decl)
                            || llvm::isa<clang::ClassTemplateDecl>(Decl))) {
                        // TODO handle constructors.
                        return nullptr;
                    }
                    return Decl;
                }

                if (Found.isOverloadedResult() && Next.is(clang::tok::l_paren)) {
                    // TODO
                }
                return nullptr;
            }
        }
        if (Tok.is(clang::tok::tilde) || Tok.is(clang::tok::kw_operator)) {
            // TODO
            return nullptr;
        }

        if (!isFunction)
            return nullptr;
        SS = {};
        // Then it is probably the return type, just skip it.
    }
    return nullptr;
}

struct CommentHandler::CommentVisitor : clang::comments::ConstCommentVisitor<CommentVisitor>
{
    typedef clang::comments::ConstCommentVisitor<CommentVisitor> Base;
    CommentVisitor(Annotator &annotator, Generator &generator,
                   const clang::comments::CommandTraits &traits, clang::Sema &Sema)
        : annotator(annotator)
        , generator(generator)
        , traits(traits)
        , Sema(Sema)
    {
    }
    Annotator &annotator;
    Generator &generator;
    const clang::comments::CommandTraits &traits;
    clang::Sema &Sema;

    clang::NamedDecl *Decl = nullptr;
    std::string DeclRef;
    std::vector<std::pair<std::string, Doc>> SubDocs; // typically for enum values

    void visit(const clang::comments::Comment *C)
    {
        Base::visit(C);
        for (auto it = C->child_begin(); it != C->child_end(); ++it)
            visit(*it);
    }

    // Inline content.
    // void visitTextComment(const clang::comments::TextComment *C);
    void visitInlineCommandComment(const clang::comments::InlineCommandComment *C)
    {
        tag("command", C->getCommandNameRange());
        for (unsigned int i = 0; i < C->getNumArgs(); ++i)
            tag("arg", C->getArgRange(i));
    }
    void visitHTMLStartTagComment(const clang::comments::HTMLStartTagComment *C)
    {
        tag("tag", C->getSourceRange());
        /*for (int i = 0; i < C->getNumAttrs(); ++i) {
            auto attr = C->getAttr(i);
            tag("attr", attr.getNameRange());
        }*/
    }
    void visitHTMLEndTagComment(const clang::comments::HTMLEndTagComment *C)
    {
        tag("tag", C->getSourceRange());
    }

    // Block content.
    // void visitParagraphComment(const clang::comments::ParagraphComment *C);
    void visitBlockCommandComment(const clang::comments::BlockCommandComment *C)
    {
        auto nameRange = C->getCommandNameRange(traits);
        tag("command", { C->getSourceRange().getBegin(), nameRange.getEnd().getLocWithOffset(-1) });
        for (unsigned int i = 0; i < C->getNumArgs(); ++i)
            tag("arg", C->getArgRange(i));
        if (C->getCommandName(traits) == "value")
            parseEnumValue(C);
    }
    // void visitParamCommandComment(const clang::comments::ParamCommandComment *C);
    // void visitTParamCommandComment(const clang::comments::TParamCommandComment *C);
    /*void visitVerbatimBlockComment(const clang::comments::VerbatimBlockComment *C) {
        Base::visitVerbatimBlockComment(C);
        FIXME
        // highlight the closing command
        auto end = C->getLocEnd();
        tag("arg", {end.getLocWithOffset(-C->getCloseName().size()) ,end});
    }*/
    void visitVerbatimBlockLineComment(const clang::comments::VerbatimBlockLineComment *C)
    {
        tag("verb", C->getSourceRange());
    }
    void visitVerbatimLineComment(const clang::comments::VerbatimLineComment *C)
    {
        auto R = C->getTextRange();
        // We need to adjust because the text starts right after the name, which overlap with the
        // command.  And also includes the end of line, which is useless.
        Base::visitVerbatimLineComment(C);

        std::string ref;
        auto Info = traits.getCommandInfo(C->getCommandID());
        if (Info->IsDeclarationCommand) {
            auto D = parseDeclarationReference(C->getText(), Sema,
                                               Info->IsFunctionDeclarationCommand
                                                   || Info->getID()
                                                       == clang::comments::CommandTraits::KCI_fn);
            if (D) {
                Decl = D;
                DeclRef = annotator.getVisibleRef(Decl);
                ref = DeclRef;
            }
        }
        tag("verb", { R.getBegin().getLocWithOffset(+1), R.getEnd().getLocWithOffset(-1) }, ref);
    }

    // void visitFullComment(const clang::comments::FullComment *C);

private:
    void tag(llvm::StringRef className, clang::SourceRange range,
             llvm::StringRef ref = llvm::StringRef())
    {
        int len = range.getEnd().getRawEncoding() - range.getBegin().getRawEncoding() + 1;
        if (len > 0) {
            std::string attr;
            if (ref.empty()) {
                attr = "class=\"" % className % "\"";
            } else {
                attr = "class=\"" % className % "\" data-ref=\"" % ref % "\"";
            }
            auto offset = annotator.getSourceMgr().getFileOffset(range.getBegin());
            generator.addTag("span", attr, offset, len);
        }
    }

    // Parse the \value command  (for enum values)
    void parseEnumValue(const clang::comments::BlockCommandComment *C)
    {
        auto ED = llvm::dyn_cast_or_null<clang::EnumDecl>(Decl);
        if (!ED)
            return;
        auto P = C->getParagraph();
        if (!P)
            return;
        auto valueStartLoc = P->getSourceRange().getBegin();
        const char *data = annotator.getSourceMgr().getCharacterData(valueStartLoc);
        auto begin = data;
        while (clang::isWhitespace(*begin))
            begin++;
        auto end = begin;
#if CLANG_VERSION_MAJOR >= 14
        while (clang::isAsciiIdentifierContinue(*end))
            end++;
#else
        while (clang::isIdentifierBody(*end))
            end++;
#endif
        llvm::StringRef value(begin, end - begin);

        auto it = std::find_if(
            ED->enumerator_begin(), ED->enumerator_end(),
            [&value](const clang::EnumConstantDecl *EC) { return value == EC->getName(); });
        if (it == ED->enumerator_end())
            return;
        auto ref = annotator.getVisibleRef(*it);

        tag("arg",
            { valueStartLoc.getLocWithOffset(begin - data),
              valueStartLoc.getLocWithOffset(end - data) },
            ref);

        auto range = C->getSourceRange();
        auto len = range.getEnd().getRawEncoding() - range.getBegin().getRawEncoding() + 1;
        auto ctn = std::string(annotator.getSourceMgr().getCharacterData(range.getBegin()), len);
        SubDocs.push_back({ std::move(ref), Doc { std::move(ctn), range.getBegin() } });
    }
};

static void handleUrlsInComment(Generator &generator, llvm::StringRef rawString, int commentStart)
{
    std::size_t pos = 0;
    while ((pos = rawString.find("http", pos)) != llvm::StringRef::npos) {
        int begin = pos;
        pos += 4;
        if (begin != 0
            && llvm::StringRef(" \t/*[]()<>|:\"'{}").find(rawString[begin - 1])
                == llvm::StringRef::npos) {
            // the URL need to be the first character, or follow a space or one of the character
            continue;
        }
        if (pos < rawString.size() && rawString[pos] == 's')
            pos++;
#if CLANG_VERSION_MAJOR >= 16
        if (!rawString.substr(pos).starts_with("://"))
#else
        if (!rawString.substr(pos).startswith("://"))
#endif
            continue;
        pos += 3;
        // We have an URL

        llvm::StringRef urlChars = "-._~:/?#[]@!$&'()*+,;=%"; // chars valid in the URL
        while (pos < rawString.size()
               && (std::isalnum(rawString[pos])
                   || urlChars.find(rawString[pos]) != llvm::StringRef::npos))
            pos++;

        // don't end with a period
        if (rawString[pos - 1] == '.')
            pos--;

        // Don't end with a closing parenthese or bracket unless the URL contains an opening one
        // (e.g. wikipedia urls)
        auto candidate = rawString.substr(begin, pos - begin);
        if (rawString[pos - 1] == ')' && candidate.find('(') == llvm::StringRef::npos)
            pos--;
        if (rawString[pos - 1] == ']' && candidate.find('[') == llvm::StringRef::npos)
            pos--;

        // don't end with a period
        if (rawString[pos - 1] == '.')
            pos--;

        auto len = pos - begin;
        generator.addTag("a", "href=\"" % rawString.substr(begin, len) % "\"", commentStart + begin,
                         len);
    }
}

void CommentHandler::handleComment(Annotator &A, Generator &generator, clang::Sema &Sema,
                                   const char *bufferStart, int commentStart, int len,
                                   clang::SourceLocation searchLocBegin,
                                   clang::SourceLocation searchLocEnd,
                                   clang::SourceLocation commentLoc)
{
    llvm::StringRef rawString(bufferStart + commentStart, len);

    handleUrlsInComment(generator, rawString, commentStart);

    std::string attributes;

#if CLANG_VERSION_MAJOR >= 16
    if ((rawString.ltrim().starts_with("/**") && !rawString.ltrim().starts_with("/***"))
        || rawString.ltrim().starts_with("/*!") || rawString.ltrim().starts_with("//!")
        || (rawString.ltrim().starts_with("///") && !rawString.ltrim().starts_with("////")))
#else
    if ((rawString.ltrim().startswith("/**") && !rawString.ltrim().startswith("/***"))
        || rawString.ltrim().startswith("/*!") || rawString.ltrim().startswith("//!")
        || (rawString.ltrim().startswith("///") && !rawString.ltrim().startswith("////")))
#endif
#if CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR <= 4
        if (rawString.find("deprecated")
            == rawString.npos) // workaround crash in comments::Sema::checkDeprecatedCommand
#endif
        {
            attributes = "class=\"doc\"";

            clang::Preprocessor &PP = Sema.getPreprocessor();
            clang::comments::CommandTraits traits(PP.getPreprocessorAllocator(),
                                                  clang::CommentOptions());
            traits.registerBlockCommand("value"); // enum value
#if CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR <= 4
            traits.registerBlockCommand("deprecated"); // avoid typo correction leading to crash.
#endif
            clang::comments::Lexer lexer(PP.getPreprocessorAllocator(), PP.getDiagnostics(), traits,
                                         commentLoc, bufferStart + commentStart,
                                         bufferStart + commentStart + len);
            clang::comments::Sema sema(PP.getPreprocessorAllocator(), PP.getSourceManager(),
                                       PP.getDiagnostics(), traits, &PP);
            clang::comments::Parser parser(lexer, sema, PP.getPreprocessorAllocator(),
                                           PP.getSourceManager(), PP.getDiagnostics(), traits);
            auto fullComment = parser.parseFullComment();
            CommentVisitor visitor { A, generator, traits, Sema };
            visitor.visit(fullComment);
            if (!visitor.DeclRef.empty()) {
                for (auto &p : visitor.SubDocs)
                    docs.insert(std::move(p));
                docs.insert({ std::move(visitor.DeclRef), { rawString.str(), commentLoc } });
                generator.addTag("i", attributes, commentStart, len);
                return;
            }
        }


    // Try to find a matching declaration
    const auto &dof = decl_offsets;
    // is there one and one single decl in that range.
    auto it_before = dof.lower_bound(searchLocBegin);
    auto it_after = dof.upper_bound(searchLocEnd);
    if (it_before != dof.end() && it_after != dof.begin() && it_before == (--it_after)) {
        if (it_before->second.second) {
            docs.insert({ it_before->second.first, { rawString.str(), commentLoc } });
        } else {
            attributes %= " data-doc=\"" % it_before->second.first % "\"";
        }
    }

    generator.addTag("i", attributes, commentStart, len);
}
