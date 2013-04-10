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

#include <clang/Basic/SourceLocation.h>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <llvm/ADT/OwningPtr.h>
#include <clang/AST/Mangle.h>
#include "generator.h"

class PreprocessorCallback;

namespace clang {
class Preprocessor;
class NamedDecl;
class MangleContext;
class Type;
class QualType;
class Decl;
}


struct ProjectInfo {
    std::string name;
    std::string source_path;
//    std::string description;
//    std::string version_info;
//    std::string repo_url; //may contains tags;
    std::string revision;

    std::string external_root_url;

    //TODO
    std::string fileRepoUrl(const std::string &file) const { return {}; }
    enum Type { Normal,
                Internal, //includes and stuffs
                External, //links to external projects somewhere else, do not generate refs or anything,
                          // and link to a different ref source
    } type = Normal;

    ProjectInfo(std::string name, std::string source_path, Type t = Normal)
        :   name(std::move(name)), source_path(std::move(source_path)), type(t)  {}
    ProjectInfo(std::string name, std::string source_path, std::string rev)
        :   name(std::move(name)), source_path(std::move(source_path)), revision(std::move(rev))  {}
};


typedef std::pair<unsigned, unsigned> pathTo_cache_key_t;
namespace std {
    template <>
    struct hash<pathTo_cache_key_t>
    {
        size_t operator()(const pathTo_cache_key_t& k) const noexcept {
            return k.first ^ k.second;
        }
    };
}

class Annotator {
public:
    enum DeclType { Declaration, Definition, Use, Override };
    enum TokenType { Ref, Member, Type, Decl, Call, Namespace, Typedef, Enum };
private:
    enum class Visibility {
        Local, // Local to a Function
        Static, // If it is only visible to this file
        Global // Globaly visible.
    };

    Visibility getVisibility(const clang::NamedDecl *);

    std::map<std::string, ProjectInfo> projects =
        {{"include", {"include", "/usr/include/", ProjectInfo::Internal }}};

    std::string outputPrefix;
    std::string dataPath;

    std::map<clang::FileID, std::pair<bool, std::string> > cache;
    std::map<clang::FileID, std::string > project_cache;
    std::map<clang::FileID, Generator> generators;

    std::string htmlNameForFile(clang::FileID id); // keep a cache;
    ProjectInfo *projectForFile(llvm::StringRef filename); // don't keep a cache;


    void addReference(const std::string& ref, clang::SourceLocation refLoc, Annotator::TokenType type,
                      Annotator::DeclType dt, const std::string &typeRef, clang::Decl *decl);
    // ref -> [ what, loc, typeRef ]
    std::map<std::string, std::vector<std::tuple<DeclType, clang::SourceLocation, std::string>>> references;
    std::multimap<std::string, std::string> docs;
    // fileId -> [ref, visivility]
    std::multimap<clang::SourceLocation, std::pair<std::string, Visibility>> decl_offsets;

    std::unordered_map<pathTo_cache_key_t, std::string> pathTo_cache;

    llvm::OwningPtr<clang::MangleContext> mangle;
    std::unordered_map<void *, std::pair<std::string, std::string> > mangle_cache;  // canonical Decl*  -> ref,  escapred_title
    std::pair<std::string, std::string> getReferenceAndTitle(clang::NamedDecl* decl);
    // project -> { pretty name, ref }
    std::map<std::string, std::map<std::string, std::string>> functionIndex;

    std::unordered_map<unsigned, int> localeNumbers;

    std::string args;
    clang::SourceManager *sourceManager = nullptr;
    const clang::LangOptions *langOption = nullptr;

    void syntaxHighlight(Generator& generator, clang::FileID FID, const clang::Preprocessor& PP);
public:
    Annotator(std::string outputPrefix, std::string _dataPath) : outputPrefix(std::move(outputPrefix)) , dataPath(std::move(_dataPath)) {
        if (dataPath.empty()) dataPath = "../data";
    }
    ~Annotator();
    void addProject(ProjectInfo info);

    void setSourceMgr(clang::SourceManager &sm, const clang::LangOptions &lo)
    { sourceManager = &sm; langOption = &lo;  }
    void setMangleContext(clang::MangleContext *m) { mangle.reset(m); }
    clang::SourceManager &getSourceMgr() { return *sourceManager; }
    const clang::LangOptions &getLangOpts() const { return *langOption; }
    void setArgs(std::string a) { args = std::move(a); }

    bool generate(clang::Preprocessor& PP);

    std::string pathTo(clang::FileID From, clang::FileID To);

    // only use typeRef for declarations (or definition)
    // only use usedContext for uses
    void registerReference(clang::NamedDecl *decl, clang::SourceRange range,
                           TokenType type, DeclType declType = Annotator::Use,
                           std::string typeRef = std::string(),
                           clang::NamedDecl *usedContext = nullptr);
    void registerUse(clang::NamedDecl* decl, clang::SourceRange range, TokenType tt,
                     clang::NamedDecl* currentContext) {
        return registerReference(decl, range, tt, Use, {}, currentContext);
    }
    void registerOverride(clang::NamedDecl* decl, clang::NamedDecl* overrided, clang::SourceLocation loc);

    void reportDiagnostic(clang::SourceRange range, const std::string& msg, const std::string& clas);

    bool shouldProcess(clang::FileID);
    Generator &generator(clang::FileID fid) { return generators[fid]; }
    

    std::string getTypeRef(clang::QualType type);
    std::string computeClas(clang::NamedDecl* decl);
    std::string getContextStr(clang::NamedDecl* usedContext);
};
