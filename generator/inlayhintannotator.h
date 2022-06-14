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
 ****************************************************************************/

#pragma once

#include <clang/AST/Expr.h>
#include <clang/Basic/SourceManager.h>

class Annotator;

class InlayHintsAnnotatorHelper
{
public:
    InlayHintsAnnotatorHelper(Annotator *annotator);

    llvm::DenseMap<clang::SourceLocation, std::string>
    getDesignatorInlayHints(clang::InitListExpr *Syn);

    std::string getParamNameInlayHint(clang::CallExpr *callExpr, clang::ParmVarDecl *paramDecl,
                                      clang::Expr *arg);

private:
    // Checks if "E" is spelled in the main file and preceded by a C-style comment
    // whose contents match ParamName (allowing for whitespace and an optional "="
    // at the end.
    bool isPrecededByParamNameComment(const clang::Expr *E, llvm::StringRef ParamName,
                                      llvm::StringRef mainFileBuf);

    clang::SourceManager &SM;
    clang::FileID mainFileID;
};
