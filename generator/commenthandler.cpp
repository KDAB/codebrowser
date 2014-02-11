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

#include "commenthandler.h"
#include "generator.h"
#include "stringbuilder.h"
#include "annotator.h"
#include <clang/AST/RawCommentList.h>
#include <clang/AST/CommentParser.h>
#include <clang/AST/CommentVisitor.h>
#include <clang/AST/ASTContext.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Basic/Version.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/Lookup.h>

#include <iostream>


clang::NamedDecl *parseDeclarationReference(llvm::StringRef Text, clang::Sema &Sema) {

    clang::Preprocessor &PP = Sema.getPreprocessor();

    llvm::MemoryBuffer* Buf = llvm::MemoryBuffer::getMemBufferCopy(Text);
    clang::Lexer Lex(PP.getSourceManager().createFileIDForMemBuffer(Buf),
                    Buf, PP.getSourceManager(), PP.getLangOpts());

    auto TuDecl = Sema.getASTContext().getTranslationUnitDecl();
    clang::CXXScopeSpec SS;
    clang::Token Tok, Next;
    Lex.LexFromRawLexer(Tok);
    if (Tok.is(clang::tok::coloncolon)) {
        SS.MakeGlobal(Sema.getASTContext(), Tok.getLocation());
        Lex.LexFromRawLexer(Next);
    }

    for (; !Tok.is(clang::tok::eof); Tok = Next) {
        Lex.LexFromRawLexer(Next);
        clang::IdentifierInfo* II = nullptr;
        if (Tok.is(clang::tok::raw_identifier))
            II = PP.LookUpIdentifierInfo(Tok);

        if (Tok.is(clang::tok::identifier)) {

            if (Next.is(clang::tok::coloncolon)) {
                if (Sema.ActOnCXXNestedNameSpecifier(Sema.getScopeForContext(TuDecl), *II, Tok.getLocation(), Next.getLocation(), {}, false, SS))
                    SS.SetInvalid(Tok.getLocation());
                Lex.LexFromRawLexer(Next);
                continue;
            }

            clang::LookupResult Found(Sema, II, Tok.getLocation(), clang::Sema::LookupAnyName);

            if (SS.isEmpty())
                Sema.LookupQualifiedName(Found, TuDecl);
            else {
                clang::DeclContext* DC = Sema.computeDeclContext(SS);
                Sema.LookupQualifiedName(Found, DC ? DC : TuDecl);
            }

            if (Found.isSingleResult())
                return Found.getFoundDecl();
            return nullptr;
        }
        return nullptr;
    }
    return nullptr;
}

struct CommentHandler::CommentVisitor : clang::comments::ConstCommentVisitor<CommentVisitor>  {
    typedef clang::comments::ConstCommentVisitor<CommentVisitor> Base;
    CommentVisitor(Generator &generator, const clang::comments::CommandTraits &traits, clang::Sema &Sema)
        : generator(generator) , traits(traits), Sema(Sema) {}
    Generator &generator;
    const clang::comments::CommandTraits &traits;
    clang::Sema &Sema;

    clang::NamedDecl *Decl = nullptr;

    void visit(const clang::comments::Comment *C) {
        Base::visit(C);
        for (auto it = C->child_begin(); it != C->child_end(); ++it)
            visit(*it);
    }

    // Inline content.
    //void visitTextComment(const clang::comments::TextComment *C);
    void visitInlineCommandComment(const clang::comments::InlineCommandComment *C) {
        tag("command", C->getCommandNameRange());
        for (int i = 0; i < C->getNumArgs(); ++i)
            tag("arg", C->getArgRange(i));
    }
    void visitHTMLStartTagComment(const clang::comments::HTMLStartTagComment *C) {
        tag("tag", C->getSourceRange());
        /*for (int i = 0; i < C->getNumAttrs(); ++i) {
            auto attr = C->getAttr(i);
            tag("attr", attr.getNameRange());
        }*/
    }
    void visitHTMLEndTagComment(const clang::comments::HTMLEndTagComment *C) {
        tag("tag", C->getSourceRange());
    }

    // Block content.
    //void visitParagraphComment(const clang::comments::ParagraphComment *C);
    void visitBlockCommandComment(const clang::comments::BlockCommandComment *C) {
        auto nameRange = C->getCommandNameRange(traits);
        tag("command", {C->getLocStart(),  nameRange.getEnd().getLocWithOffset(-1)});
        for (int i = 0; i < C->getNumArgs(); ++i)
            tag("arg", C->getArgRange(i));
    }
    //void visitParamCommandComment(const clang::comments::ParamCommandComment *C);
    //void visitTParamCommandComment(const clang::comments::TParamCommandComment *C);
    /*void visitVerbatimBlockComment(const clang::comments::VerbatimBlockComment *C) {
        Base::visitVerbatimBlockComment(C);
        FIXME
        // highlight the closing command
        auto end = C->getLocEnd();
        tag("arg", {end.getLocWithOffset(-C->getCloseName().size()) ,end});
    }*/
    void visitVerbatimBlockLineComment(const clang::comments::VerbatimBlockLineComment *C) {
        tag("verb", C->getSourceRange());

    }
    void visitVerbatimLineComment(const clang::comments::VerbatimLineComment *C) {
        auto R = C->getTextRange();
        // We need to adjust because the text starts right after the name, which overlap with the
        // command.  And also includes the end of line, which is useless.
        tag("verb", {R.getBegin().getLocWithOffset(+1), R.getEnd().getLocWithOffset(-1)});
        Base::visitVerbatimLineComment(C);

        auto Info = traits.getCommandInfo(C->getCommandID());
        if (Info->IsDeclarationCommand) {
            auto D = parseDeclarationReference(C->getText(), Sema);
            if (D)
                Decl = D;
        }
    }

    //void visitFullComment(const clang::comments::FullComment *C);

private:
    void tag(llvm::StringRef className, clang::SourceRange range) {
        int len = range.getEnd().getRawEncoding() - range.getBegin().getRawEncoding() + 1;
        if (len > 0)
            generator.addTag("span", "class=\""%className%"\"", range.getBegin().getRawEncoding(), len);
    }
};

void CommentHandler::handleComment(Annotator &A, Generator& generator, clang::Sema &Sema,
                                   const char *bufferStart, int commentStart, int len,
                                   clang::SourceLocation searchLocBegin, clang::SourceLocation searchLocEnd)
{
    llvm::StringRef rawString(bufferStart+commentStart, len);
    std::string attributes;
    clang::NamedDecl *Decl = nullptr;


    if ((rawString.ltrim().startswith("/**") && !rawString.ltrim().startswith("/***"))
            || rawString.ltrim().startswith("/*!") || rawString.ltrim().startswith("//!")
            || (rawString.ltrim().startswith("///") && !rawString.ltrim().startswith("////")))
#if CLANG_VERSION_MAJOR==3 && CLANG_VERSION_MINOR<=4
        if (rawString.find("deprecated") == rawString.npos) // workaround crash in comments::Sema::checkDeprecatedCommand
#endif
    {
        attributes = "class=\"doc\"";

        clang::Preprocessor &PP = Sema.getPreprocessor();
        clang::comments::CommandTraits traits(PP.getPreprocessorAllocator(), clang::CommentOptions());
#if CLANG_VERSION_MAJOR==3 && CLANG_VERSION_MINOR<=4
        traits.registerBlockCommand("deprecated"); // avoid typo correction leading to crash.
#endif
        clang::comments::Lexer lexer(PP.getPreprocessorAllocator(), PP.getDiagnostics(), traits,
                                     clang::SourceLocation::getFromRawEncoding(commentStart), bufferStart + commentStart, bufferStart + commentStart + len);
        clang::comments::Sema sema(PP.getPreprocessorAllocator(), PP.getSourceManager(), PP.getDiagnostics(), traits, &PP);
        clang::comments::Parser parser(lexer, sema, PP.getPreprocessorAllocator(), PP.getSourceManager(),
                                       PP.getDiagnostics(), traits);
        auto fullComment = parser.parseFullComment();
        CommentVisitor visitor{generator, traits, Sema};
        visitor.visit(fullComment);
        Decl = visitor.Decl;
    }

    if (Decl) {
        auto Ref = A.getVisibleRef(Decl);
        if (!Ref.empty()) {
            docs.insert({std::move(Ref), rawString.str()});
            generator.addTag("i", attributes, commentStart, len);
            return;
        }
    }

    // Try to find a matching declaration
    const auto &dof = decl_offsets;
    //is there one and one single decl in that range.
    auto it_before = dof.lower_bound(searchLocBegin);
    auto it_after = dof.upper_bound(searchLocEnd);
    if (it_before != dof.end() && it_before == (--it_after)) {
        if (it_before->second.second) {
            docs.insert({it_before->second.first, rawString.str()});
        } else {
            attributes %= " data-doc=\"" % it_before->second.first % "\"";
        }
    }

    generator.addTag("i", attributes, commentStart, len);
}
