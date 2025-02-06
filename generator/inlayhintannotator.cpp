/****************************************************************************
 * Copyright (C) 2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
 *
 * This file is part of the Code Browser.
 *
 * Commercial License Usage:
 * Licensees holding valid commercial licenses provided by KDAB may use
 * this file in accordance with the terms contained in a written agreement
 * between the licensee and KDAB.
 * For further information see https://codebrowser.dev/
 *
 * Alternatively, this work may be used under a Creative Commons
 * Attribution-NonCommercial-ShareAlike 3.0 (CC-BY-NC-SA 3.0) License.
 * http://creativecommons.org/licenses/by-nc-sa/3.0/deed.en_US
 * This license does not allow you to use the code browser to assist the
 * development of your commercial software. If you intent to do so, consider
 * purchasing a commercial licence.
 ***************************************************************************
 * Parts of this code is derived from clangd InlayHints.cpp. License of the file:
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *******************************************************************************/
#include "inlayhintannotator.h"
#include "annotator.h"
#include "stringbuilder.h"

#include <clang/AST/ExprCXX.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>

#include <iterator>
#include <utility>

static bool isReservedName(llvm::StringRef id)
{
    if (id.size() < 2)
        return false;
    return id.starts_with("__");
}

static llvm::StringRef getSimpleName(const clang::NamedDecl *d)
{
    if (clang::IdentifierInfo *Ident = d->getDeclName().getAsIdentifierInfo()) {
        return Ident->getName();
    }
    return llvm::StringRef();
}

// If "E" spells a single unqualified identifier, return that name.
// Otherwise, return an empty string.
static llvm::StringRef getSpelledIdentifier(const clang::Expr *e)
{
    e = e->IgnoreUnlessSpelledInSource();

    if (auto *DRE = llvm::dyn_cast<clang::DeclRefExpr>(e))
        if (!DRE->getQualifier())
            return getSimpleName(DRE->getDecl());

    if (auto *ME = llvm::dyn_cast<clang::MemberExpr>(e))
        if (!ME->getQualifier() && ME->isImplicitAccess())
            return getSimpleName(ME->getMemberDecl());

    return {};
}

// Helper class to iterate over the designator names of an aggregate type.
// For aggregate classes, yields null for each base, then .field1, .field2,
class AggregateDesignatorNames
{
public:
    AggregateDesignatorNames(clang::QualType type)
    {
        type = type.getCanonicalType();
        if (const clang::RecordDecl *RD = type->getAsRecordDecl()) {
            valid = true;
            fieldsIt = RD->field_begin();
            fieldsEnd = RD->field_end();
            if (const auto *CRD = llvm::dyn_cast<clang::CXXRecordDecl>(RD)) {
                basesIt = CRD->bases_begin();
                basesEnd = CRD->bases_end();
                valid = CRD->isAggregate();
            }
            oneField = valid && basesIt == basesEnd && fieldsIt != fieldsEnd
                && std::next(fieldsIt) == fieldsEnd;
        }
    }
    // Returns false if the type was not an aggregate.
    operator bool() const
    {
        return valid;
    }
    // Advance to the next element in the aggregate.
    void next()
    {
        if (basesIt != basesEnd)
            ++basesIt;
        else if (fieldsIt != fieldsEnd)
            ++fieldsIt;
    }
    // Print the designator to Out.
    // Returns false if we could not produce a designator for this element.
    bool append(std::string &out, bool forSubobject)
    {
        if (basesIt != basesEnd)
            return false; // Bases can't be designated. Should we make one
                          // up?
        if (fieldsIt != fieldsEnd) {
            llvm::StringRef fieldName;
            if (const clang::IdentifierInfo *ii = fieldsIt->getIdentifier())
                fieldName = ii->getName();

            // For certain objects, their subobjects may be named directly.
            if (forSubobject
                && (fieldsIt->isAnonymousStructOrUnion() ||
                    // std::array<int,3> x = {1,2,3}.
                    // Designators not strictly valid!
                    (oneField && isReservedName(fieldName))))
                return true;

            if (!fieldName.empty() && !isReservedName(fieldName)) {
                out.push_back('.');
                out.append(fieldName.begin(), fieldName.end());
                return true;
            }
            return false;
        }
        return false;
    }

private:
    bool valid = false;
    bool oneField = false; // e.g. std::array { T __elements[N]; }
    clang::CXXRecordDecl::base_class_const_iterator basesIt;
    clang::CXXRecordDecl::base_class_const_iterator basesEnd;
    clang::RecordDecl::field_iterator fieldsIt;
    clang::RecordDecl::field_iterator fieldsEnd;
};

static void collectDesignators(const clang::InitListExpr *sem,
                               llvm::DenseMap<clang::SourceLocation, std::string> &out,
                               const llvm::DenseSet<clang::SourceLocation> &nestedBraces,
                               std::string &prefix)
{
    if (!sem || sem->isTransparent() || !sem->isSemanticForm())
        return;
    // Nothing to do for arrays
    if (sem->getType().isNull() || sem->getType().getCanonicalType()->isArrayType())
        return;

    // The elements of the semantic form all correspond to direct subobjects
    // of the aggregate type. `Fields` iterates over these subobject names.
    AggregateDesignatorNames fields(sem->getType());
    if (!fields)
        return;

    for (const clang::Expr *init : sem->inits()) {
        struct RAII
        {
            AggregateDesignatorNames &fields;
            std::string &prefix;
            const int size = prefix.size();
            ~RAII()
            {
                fields.next(); // Always advance to the next subobject name.
                prefix.resize(size); // Erase any designator we appended.
            }
        };
        auto next = RAII { fields, prefix };
        // Skip for a broken initializer or if it is a "hole" in a subobject
        // that was not explicitly initialized.
        if (!init || llvm::isa<clang::ImplicitValueInitExpr>(init))
            continue;

        const auto *braceElidedSubobject = llvm::dyn_cast<clang::InitListExpr>(init);
        if (braceElidedSubobject && nestedBraces.contains(braceElidedSubobject->getLBraceLoc()))
            braceElidedSubobject = nullptr; // there were braces!

        if (!fields.append(prefix, braceElidedSubobject != nullptr))
            continue; // no designator available for this subobject
        if (braceElidedSubobject) {
            // If the braces were elided, this aggregate subobject is
            // initialized inline in the same syntactic list. Descend into
            // the semantic list describing the subobject. (NestedBraces are
            // still correct, they're from the same syntactic list).
            collectDesignators(braceElidedSubobject, out, nestedBraces, prefix);
            continue;
        }
        out.try_emplace(init->getBeginLoc(), prefix);
    }
}

InlayHintsAnnotatorHelper::InlayHintsAnnotatorHelper(Annotator *annotator)
    : SM(annotator->getSourceMgr())
    , mainFileID(SM.getMainFileID())
{
}

llvm::DenseMap<clang::SourceLocation, std::string>
InlayHintsAnnotatorHelper::getDesignatorInlayHints(clang::InitListExpr *Syn)
{
    // collectDesignators needs to know which InitListExprs in the semantic
    // tree were actually written, but InitListExpr::isExplicit() lies.
    // Instead, record where braces of sub-init-lists occur in the syntactic
    // form.
    llvm::DenseSet<clang::SourceLocation> nestedBraces;
    for (const clang::Expr *init : Syn->inits())
        if (auto *nested = llvm::dyn_cast<clang::InitListExpr>(init))
            nestedBraces.insert(nested->getLBraceLoc());

    // Traverse the semantic form to find the designators.
    // We use their SourceLocation to correlate with the syntactic form
    // later.
    llvm::DenseMap<clang::SourceLocation, std::string> designators;
    std::string emptyPrefix;
    collectDesignators(Syn->isSemanticForm() ? Syn : Syn->getSemanticForm(), designators,
                       nestedBraces, emptyPrefix);

    llvm::StringRef Buf = SM.getBufferData(mainFileID);
    llvm::DenseMap<clang::SourceLocation, std::string> designatorSpans;

    for (const clang::Expr *init : Syn->inits()) {
        if (llvm::isa<clang::DesignatedInitExpr>(init))
            continue;
        auto it = designators.find(init->getBeginLoc());
        if (it != designators.end() && !isPrecededByParamNameComment(init, it->second, Buf)) {
            designatorSpans.try_emplace(init->getBeginLoc(), it->second % ":&nbsp;");
        }
    }
    return designatorSpans;
}

bool InlayHintsAnnotatorHelper::isPrecededByParamNameComment(const clang::Expr *E,
                                                             llvm::StringRef paramName,
                                                             llvm::StringRef mainFileBuf)
{
    auto exprStartLoc = SM.getTopMacroCallerLoc(E->getBeginLoc());
    auto decomposed = SM.getDecomposedLoc(exprStartLoc);
    if (decomposed.first != mainFileID)
        return false;

    llvm::StringRef sourcePrefix = mainFileBuf.substr(0, decomposed.second);
    // Allow whitespace between comment and expression.
    sourcePrefix = sourcePrefix.rtrim();
    // Check for comment ending.
    if (!sourcePrefix.consume_back("*/"))
        return false;
    // Ignore some punctuation and whitespace around comment.
    // In particular this allows designators to match nicely.
    llvm::StringLiteral ignoreChars = " =.";
    sourcePrefix = sourcePrefix.rtrim(ignoreChars);
    paramName = paramName.trim(ignoreChars);
    // Other than that, the comment must contain exactly ParamName.
    if (!sourcePrefix.consume_back(paramName))
        return false;
    sourcePrefix = sourcePrefix.rtrim(ignoreChars);
    return sourcePrefix.ends_with("/*");
}

std::string InlayHintsAnnotatorHelper::getParamNameInlayHint(clang::CallExpr *e,
                                                             clang::ParmVarDecl *paramDecl,
                                                             clang::Expr *arg)
{
    if (e->isCallToStdMove() || llvm::isa<clang::UserDefinedLiteral>(e)
        || llvm::isa<clang::CXXOperatorCallExpr>(e))
        return {};

    auto f = e->getDirectCallee();
    if (!f)
        return {};

    // simple setter? => ignore
    if (f->getNumParams() == 1 && getSimpleName(f).starts_with_insensitive("set"))
        return {};

    llvm::StringRef paramName = getSimpleName(paramDecl);
    if (paramName.empty() || paramName == getSpelledIdentifier(arg))
        return {};

    paramName = paramName.ltrim('_');
    if (isPrecededByParamNameComment(arg, paramName, SM.getBufferData(mainFileID)))
        return {};

    auto t = paramDecl->getType();
    bool isLValueRef = t->isLValueReferenceType() && !t.getNonReferenceType().isConstQualified();
    llvm::StringRef refStr = isLValueRef ? "&amp;" : llvm::StringRef();
    return paramName % refStr % ":&nbsp;";
}
