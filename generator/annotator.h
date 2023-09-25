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

#include "commenthandler.h"
#include "generator.h"
#include <clang/AST/Mangle.h>
#include <clang/Basic/SourceLocation.h>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct ProjectManager;
struct ProjectInfo;
class PreprocessorCallback;

namespace clang {
class Preprocessor;
class NamedDecl;
class MangleContext;
class Type;
class QualType;
class Decl;
class FileEntry;
}

/* Wrapper for the change in the name in clang 3.5 */
template<typename T>
auto getResultType(T *decl) -> decltype(decl->getResultType())
{
    return decl->getResultType();
}
template<typename T>
auto getResultType(T *decl) -> decltype(decl->getReturnType())
{
    return decl->getReturnType();
}

typedef std::pair<unsigned, unsigned> pathTo_cache_key_t;
namespace std {
template<>
struct hash<pathTo_cache_key_t>
{
    size_t operator()(const pathTo_cache_key_t &k) const noexcept
    {
        return k.first ^ k.second;
    }
};
}

class Annotator
{
public:
    enum DeclType {
        Declaration,
        Definition,
        Use,
        Use_Read,
        Use_Write,
        Use_Address,
        Use_Call,
        Use_MemberAccess,
        Use_NestedName,
        Override,
        Inherit
    };
    enum TokenType {
        Ref,
        Member,
        Type,
        Decl,
        Call,
        Namespace,
        Typedef,
        Enum,
        EnumDecl,
        Label
    };

private:
    enum class Visibility {
        Local, // Local to a Function
        Static, // If it is only visible to this file
        Global // Globaly visible.
    };

    Visibility getVisibility(const clang::NamedDecl *);

    std::map<clang::FileID, std::pair<bool, std::string>> cache;
    std::map<clang::FileID, ProjectInfo *> project_cache;
    std::map<clang::FileID, Generator> generators;

    std::string htmlNameForFile(clang::FileID id); // keep a cache;

    void addReference(const std::string &ref, clang::SourceRange refLoc, Annotator::TokenType type,
                      Annotator::DeclType dt, const std::string &typeRef, clang::Decl *decl);

    struct Reference
    {
        DeclType what;
        clang::SourceRange loc;
        std::string typeOrContext;
    };
    std::map<std::string, std::vector<Reference>> references;
    std::map<std::string, ssize_t> structure_sizes;
    std::map<std::string, ssize_t> field_offsets;
    struct SubRef
    {
        std::string ref;
        std::string type;
        enum Type {
            None,
            Function,
            Member,
            Static
        } what = None;
    };
    std::map<std::string, std::vector<SubRef>> sub_refs;
    std::unordered_map<pathTo_cache_key_t, std::string> pathTo_cache;
    CommentHandler commentHandler;

    std::unique_ptr<clang::MangleContext> mangle;
    std::unordered_map<void *, std::pair<std::string, std::string>> mangle_cache; // canonical Decl*
                                                                                  // -> ref,
                                                                                  // escapred_title
    std::pair<std::string, std::string> getReferenceAndTitle(clang::NamedDecl *decl);
    // project -> { pretty name, ref }
    std::map<std::string, std::string> functionIndex;

    std::unordered_map<unsigned, int> localeNumbers;

    std::map<clang::FileID, std::set<std::string>> interestingDefinitionsInFile;

    std::string args;
    clang::SourceManager *sourceManager = nullptr;
    const clang::LangOptions *langOption = nullptr;

    void syntaxHighlight(Generator &generator, clang::FileID FID, clang::Sema &);

public:
    explicit Annotator(ProjectManager &pm)
        : projectManager(pm)
    {
    }
    ~Annotator();

    ProjectManager &projectManager;

    void setSourceMgr(clang::SourceManager &sm, const clang::LangOptions &lo)
    {
        sourceManager = &sm;
        langOption = &lo;
    }
    void setMangleContext(clang::MangleContext *m)
    {
        mangle.reset(m);
    }
    clang::SourceManager &getSourceMgr()
    {
        return *sourceManager;
    }
    const clang::LangOptions &getLangOpts() const
    {
        return *langOption;
    }
    void setArgs(std::string a)
    {
        args = std::move(a);
    }

    bool generate(clang::Sema &, bool WasInDatabase);

    /**
     * Returns a string with the URL to go from one file to an other.
     * In case the file is in an external project that needs a data-proj tag, the proj
     * string is set to the project name.
     **/
    std::string pathTo(clang::FileID From, clang::FileID To, std::string *proj = nullptr);
    std::string pathTo(clang::FileID From, const clang::FileEntry *To);

    /**
     * Registers that the code in the given \a range refers to the specified declaration.
     * This will highlight the rights portion of the code and registers it in the database.
     *
     * When the range.getEndLocation refers to an invalid location, this means this is a
     * virtual range with no associated token (e.g. implicit conversions) which will be handled
     * with an empty <span>.
     *
     * Only use typeRef for declarations (or definition), only use usedContext for uses
     */
    void registerReference(clang::NamedDecl *decl, clang::SourceRange range, TokenType type,
                           DeclType declType = Annotator::Use, std::string typeRef = std::string(),
                           clang::NamedDecl *usedContext = nullptr);
    void registerUse(clang::NamedDecl *decl, clang::SourceRange range, TokenType tt,
                     clang::NamedDecl *currentContext, DeclType useType = Use)
    {
        return registerReference(decl, range, tt, useType, {}, currentContext);
    }
    void registerOverride(clang::NamedDecl *decl, clang::NamedDecl *overrided,
                          clang::SourceLocation loc);
    // same, but for macro
    void registerMacro(const std::string &ref, clang::SourceLocation refLoc, DeclType declType);

    // Class names, structs, objective C identifiers, main function
    void registerInterestingDefinition(clang::SourceRange range, clang::NamedDecl *decl);

    /**
     * Wrap the source range in an HTML tag
     */
    void annotateSourceRange(clang::SourceRange range, std::string tag, std::string attributes);

    void reportDiagnostic(clang::SourceRange range, const std::string &msg,
                          const std::string &clas);

    void addInlayHint(clang::SourceLocation range, std::string inlayHint);

    bool shouldProcess(clang::FileID);
    Generator &generator(clang::FileID fid)
    {
        return generators[fid];
    }

    std::string getTypeRef(clang::QualType type);
    std::string computeClas(clang::NamedDecl *decl);
    std::string getContextStr(clang::NamedDecl *usedContext);
    /**
     * returns the reference of a class iff this class is visible
     * (that is, if a tooltip file should be generated for it)
     */
    std::string getVisibleRef(clang::NamedDecl *Decl);

    std::string externalProjectForFile(clang::FileID fid);

    /**
     * Returns the param name in function decl for the arg @p arg
     * Will be empty if arg->name == paramDecl->name
     */
    std::string getParamNameForArg(clang::CallExpr *callExpr, clang::ParmVarDecl *paramDecl,
                                   clang::Expr *arg);

    llvm::DenseMap<clang::SourceLocation, std::string>
    getDesignatorInlayHints(clang::InitListExpr *Syn);
};
