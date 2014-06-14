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

#include "annotator.h"
#include "generator.h"
#include "filesystem.h"
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/Version.h>
#include <clang/AST/DeclBase.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Mangle.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/ASTContext.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Sema/Sema.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>

#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>

#if CLANG_VERSION_MAJOR==3 && CLANG_VERSION_MINOR<=3
#include <llvm/Support/PathV2.h>
#else
#include <llvm/Support/Path.h>
#endif

#include "stringbuilder.h"

namespace
{

template <class T>
ssize_t getTypeSize(const T &t)
{
    const clang::ASTContext &ctx = t->getASTContext();
    const clang::QualType &ty = ctx.getRecordType(t);

    /** Return size in bytes */
    return ctx.getTypeSize(ty) >> 3;
}

ssize_t getDeclSize(const clang::Decl* decl)
{
    const clang::CXXRecordDecl *cxx = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
    if (cxx && (cxx = cxx->getDefinition())) {
        /**
         * XXX: avoid endless recursion inside
         * clang::ASTContext::getTypeInfo() -> getTypeInfoImpl()
         */
        if (cxx->isDependentContext()) {
            return -1;
        }

        return getTypeSize(cxx);
    }

    const clang::RecordDecl *c = llvm::dyn_cast<clang::RecordDecl>(decl);
    if (c && (c = c->getDefinition())) {
        return getTypeSize(c);
    }

    return -1;
}

};

Annotator::~Annotator()
{ }

Annotator::Visibility Annotator::getVisibility(const clang::NamedDecl *decl)
{
    if (llvm::isa<clang::EnumConstantDecl>(decl)  ||
        llvm::isa<clang::EnumDecl>(decl)  ||
        llvm::isa<clang::NamespaceDecl>(decl) ||
        llvm::isa<clang::NamespaceAliasDecl>(decl) ||
        llvm::isa<clang::TypedefDecl>(decl) ||
        llvm::isa<clang::TypedefNameDecl>(decl)) {

        if (!decl->isDefinedOutsideFunctionOrMethod())
            return Visibility::Local;
        if (decl->isInAnonymousNamespace())
            return Visibility::Static;
        return Visibility::Global; //FIXME
    }

    if (llvm::isa<clang::NonTypeTemplateParmDecl>(decl))
        return Visibility::Static;

    clang::SourceManager &sm = getSourceMgr();
    clang::FileID mainFID = sm.getMainFileID();

#if CLANG_VERSION_MAJOR==3 && CLANG_VERSION_MINOR<=3
    switch (decl->getLinkage())
#else
    switch (decl->getLinkageInternal())
#endif
    {
        default:
        case clang::NoLinkage:
            return Visibility::Local;
        case clang::ExternalLinkage:
            if (decl->getDeclContext()->isRecord()
                && mainFID == sm.getFileID(sm.getSpellingLoc(llvm::dyn_cast<clang::NamedDecl>(decl->getDeclContext())->getCanonicalDecl()->getSourceRange().getBegin()))) {
                // private class
                const clang::CXXMethodDecl* fun = llvm::dyn_cast<clang::CXXMethodDecl>(decl);
                if (fun && fun->isVirtual())
                    return Visibility::Global; //because we need to check overrides
                return Visibility::Static;
            }
            return Visibility::Global;
        case clang::InternalLinkage:
            if (mainFID != sm.getFileID(sm.getSpellingLoc(decl->getSourceRange().getBegin())))
                return Visibility::Global;
            return Visibility::Static;
        case clang::UniqueExternalLinkage:
            return Visibility::Static;
    }
};

void Annotator::addProject(ProjectInfo info) {
    if (info.source_path.empty())
        return;
    llvm::SmallString<256> filename;
    canonicalize(info.source_path, filename);
    if (filename[filename.size()-1] != '/')
        filename += '/';
    info.source_path = filename.c_str();

    projects.push_back( std::move(info) );
}

bool Annotator::shouldProcess(clang::FileID FID)
{
    auto it = cache.find(FID);
    if (it == cache.end()) {
        htmlNameForFile(FID);
        it = cache.find(FID);
        assert(it != cache.end());
    }
    return it->second.first;
}


std::string Annotator::htmlNameForFile(clang::FileID id)
{
    {
        auto it = cache.find(id);
        if (it != cache.end()) {
            return it->second.second;
        }
    }

    const clang::FileEntry* entry = getSourceMgr().getFileEntryForID(id);
    if (!entry || !entry->getName())
    {
        cache[id] = {false, {} };
        return {};
    }
    llvm::SmallString<256> filename;
    canonicalize(entry->getName(), filename);

    ProjectInfo *project = projectForFile(filename);
    if (project) {
        std::string fn = project->name % "/" % (filename.c_str() + project->source_path.size());
        bool should_process = false;
        if (project->type != ProjectInfo::External) {
            std::string p(outputPrefix %  "/" % fn % ".html");
            should_process =  !llvm::sys::fs::exists(p);
            // || boost::filesystem::last_write_time(p) < entry->getModificationTime();
        }
        project_cache[id] = project;
        cache[id] = { should_process , fn};
        return fn;
    }

    cache[id] = {false, {} };
    return {};
}

ProjectInfo* Annotator::projectForFile(llvm::StringRef filename)
{
    unsigned int match_length = 0;
    ProjectInfo *result = nullptr;

    for (auto &it : projects) {
        const std::string &source_path = it.source_path;
        if (source_path.size() < match_length)
            continue;
        if (filename.startswith(source_path)) {
            result = &it;
            match_length = source_path.size();
        }
    }

    return result;
}

static char normalizeForfnIndex(char c) {
    if (c >= 'A' && c <= 'Z')
        c = c - 'A' + 'a';
    if (c < 'a' || c > 'z')
        return '_';
    return c;
}

bool Annotator::generate(clang::Sema &Sema)
{
    std::ofstream fileIndex;
    fileIndex.open(outputPrefix + "/fileIndex", std::ios::app);
    if (!fileIndex) {
        create_directories(outputPrefix);
        fileIndex.open(outputPrefix + "/fileIndex", std::ios::app);
        if (!fileIndex) {
            std::cerr << "Can't generate index for " << std::endl;
            return false;
        }
    }

    std::set<std::string> done;
    for(auto it : cache) {
        if (!it.second.first)
            continue;
        const std::string &fn = it.second.second;
        if (done.count(fn))
            continue;
        done.insert(fn);

        auto project_it = std::find_if(projects.cbegin(), projects.cend(),
                              [&fn](const ProjectInfo &it)
                              { return llvm::StringRef(fn).startswith(it.name); } );
        if (project_it == projects.cend()) {
            std::cerr << "GENERATION ERROR: " << fn << " not in a project" << std::endl;
            continue;
        }

        clang::FileID FID = it.first;

        Generator &g = generator(FID);

        syntaxHighlight(g, FID, Sema);
//        clang::html::HighlightMacros(R, FID, PP);

        std::string footer;
        clang::FileID mainFID = getSourceMgr().getMainFileID();
        if (FID != mainFID) {
            footer  = "Generated while processing <a href='" %  pathTo(FID, mainFID) % "'>" % htmlNameForFile(mainFID) % "</a><br/>";
        }

        auto now = time(0);
        auto tm = localtime(&now);
        char buf[80];
        strftime(buf, sizeof(buf), "%Y-%b-%d", tm);

        const ProjectInfo &projectinfo = *project_it;
        footer %= "Generated on <em>" % std::string(buf) % "</em>"
            % " from project " % projectinfo.name % "</a>";
        if (!projectinfo.revision.empty())
            footer %= " revision <em>" % projectinfo.revision % "</em>";

        /*     << " from file <a href='" << projectinfo.fileRepoUrl(filename) << "'>" << filename << "</a>"
        title=\"Arguments: << " << Generator::escapeAttr(args)   <<"\"" */

        // Emit the HTML.
        const llvm::MemoryBuffer *Buf = getSourceMgr().getBuffer(FID);
        g.generate(outputPrefix, dataPath, fn, Buf->getBufferStart(), Buf->getBufferEnd(), footer);

        fileIndex << fn << '\n';
    }

    // make sure all the docs are in the references
    // (There might not be when the comment is in the .cpp file (for \class))
    for (auto it : commentHandler.docs) references[it.first];

    create_directories(llvm::Twine(outputPrefix, "/refs"));
    for (auto it : references) {
        if (llvm::StringRef(it.first).startswith("__builtin"))
            continue;
        if (it.first == "main")
            continue;
        std::string filename = outputPrefix % "/refs/" % it.first;
        std::string error;
#if CLANG_VERSION_MAJOR==3 && CLANG_VERSION_MINOR<=3
        llvm::raw_fd_ostream myfile(filename.c_str(), error, llvm::raw_fd_ostream::F_Append);
#else
        llvm::raw_fd_ostream myfile(filename.c_str(), error, llvm::sys::fs::F_Append);
#endif
        if (!error.empty()) {
            std::cerr << error<< std::endl;
            continue;
        }
        for (auto &it2 : it.second) {
            clang::SourceLocation loc = std::get<1>(it2);
            clang::SourceManager &sm = getSourceMgr();
            clang::SourceLocation exp = sm.getExpansionLoc(loc);
            std::string fn = htmlNameForFile(sm.getFileID(exp));
            if (fn.empty())
                continue;
            clang::PresumedLoc fixed = sm.getPresumedLoc(exp);
            const char *tag = "";
            switch(std::get<0>(it2)) {
                case Use:
                    tag = "use";
                    break;
                case Declaration:
                    tag = "dec";
                    break;
                case Definition:
                    tag = "def";
                    break;
                case Override:
                    tag = "ovr";
                    break;
                case Inherit:
                    tag = "inh";
            }
            myfile << "<" << tag << " f='";
            Generator::escapeAttr(myfile, fn);
            myfile << "' l='"<<  fixed.getLine()  <<"'";
            if (loc.isMacroID()) myfile << " macro='1'";
            const auto &refType = std::get<2>(it2);
            if (!refType.empty()) {
                myfile << ((std::get<0>(it2) < Use) ? " type='" : " c='");
                Generator::escapeAttr(myfile, refType);
                myfile <<"'";
            }
            myfile <<"/>\n";
        }
        auto itS = structure_sizes.find(it.first);
        if (itS != structure_sizes.end() && itS->second != -1) {
            myfile << "<size>"<< itS->second <<"</size>\n";
        }
        auto range =  commentHandler.docs.equal_range(it.first);
        for (auto it2 = range.first; it2 != range.second; ++it2) {
            myfile << "<doc>";
            Generator::escapeAttr(myfile, it2->second);
            myfile << "</doc>\n";
        }
    }

    // now the function names
    create_directories(llvm::Twine(outputPrefix, "/fnSearch"));
    for(auto &fnIt : functionIndex) {
        auto fnName = fnIt.first;
        if (fnName.size() < 4)
            continue;
        if (fnName.find("__") != std::string::npos)
            continue; // remove internals
        if (fnName.find('<') != std::string::npos || fnName.find('>') != std::string::npos)
            continue; // remove template stuff
        if (fnName == "main")
            continue;

        llvm::SmallString<8> saved;
        size_t pos = 0;
        while (true) {
            if (fnName.size() - pos < 4)
                break;
            char idx[3] = { normalizeForfnIndex(fnName[pos]), normalizeForfnIndex(fnName[pos+1]) , '\0' };
            llvm::StringRef idxRef(idx, 3); // include the '\0' on purpose
            if (saved.find(idxRef) == std::string::npos) {
                std::string funcIndexFN = outputPrefix % "/fnSearch/" % idx;
                std::string error;
#if CLANG_VERSION_MAJOR==3 && CLANG_VERSION_MINOR<=3
                llvm::raw_fd_ostream funcIndexFile(funcIndexFN.c_str(), error, llvm::raw_fd_ostream::F_Append);
#else
                llvm::raw_fd_ostream funcIndexFile(funcIndexFN.c_str(), error, llvm::sys::fs::F_Append);
#endif
                if (!error.empty()) {
                    std::cerr << error << std::endl;
                    return false;
                }
                funcIndexFile << fnIt.second << '|'<< fnIt.first << '\n';
                saved.append(idxRef); //include \0;
            }
            pos = fnName.find("::", pos);

            if (pos == std::string::npos)
                break;
            pos += 2; // skip ::
        }
    }
    return true;
}


std::string Annotator::pathTo(clang::FileID From, clang::FileID To)
{
    std::string &result = pathTo_cache[{From.getHashValue(), To.getHashValue()}];
    if (!result.empty())
        return result;
    std::string fromFN = htmlNameForFile(From);
    std::string toFN = htmlNameForFile(To);

    auto pr_it =  project_cache.find(To);
    if (pr_it == project_cache.end())
        return result = {};
    if (pr_it->second->type == ProjectInfo::External)
        return result = pr_it->second->external_root_url % "/" % toFN % ".html";

    return result = naive_uncomplete(llvm::sys::path::parent_path(fromFN), toFN) + ".html";
}

std::string Annotator::pathTo(clang::FileID From, const clang::FileEntry *To)
{
  //this is a bit duplicated with the other pathTo and htmlNameForFile

    if (!To || !To->getName())
        return {};

    std::string fromFN = htmlNameForFile(From);

    llvm::SmallString<256> filename;
    canonicalize(To->getName(), filename);


    ProjectInfo *project = projectForFile(filename);
    if (!project)
        return {};

    if (project->type == ProjectInfo::External) {
        return project->external_root_url % "/" % project->name % "/" % (filename.c_str() + project->source_path.size()) % ".html";
    }

    return naive_uncomplete(llvm::sys::path::parent_path(fromFN),
                            std::string(project->name % "/" % (filename.c_str() + project->source_path.size()) % ".html"));
}

static const clang::Decl *getDefinitionDecl(clang::Decl *decl) {
    if (const clang::RecordDecl* rec = llvm::dyn_cast<clang::RecordDecl>(decl)) {
        rec = rec->getDefinition();
        if (rec) return rec;
    } else if (const clang::FunctionDecl* fnc = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        if (fnc->hasBody(fnc) && fnc) {
            return fnc;
        }
    }
    return decl->getCanonicalDecl();
}

void Annotator::registerReference(clang::NamedDecl* decl, clang::SourceRange range, Annotator::TokenType type,
                                  Annotator::DeclType declType, std::string typeText,
                                  clang::NamedDecl *usedContext)
{
    //annonymouse namespace, anonymous struct, or unnamed argument.
    if (decl->getDeclName().isIdentifier() && decl->getName().empty())
        return;

    clang::SourceManager &sm = getSourceMgr();

    Visibility visibility = getVisibility(decl);

    if (!range.getBegin().isFileID()) { //macro expension.
        clang::SourceLocation expensionloc = sm.getExpansionLoc(range.getBegin());
        clang::FileID FID = sm.getFileID(expensionloc);
        if (!shouldProcess(FID) || sm.getMacroArgExpandedLocation(range.getBegin()) !=
                                   sm.getMacroArgExpandedLocation(range.getEnd())) {
            return;
        }

        clang::SourceLocation spel1 = sm.getSpellingLoc(range.getBegin());
        clang::SourceLocation spel2 = sm.getSpellingLoc(range.getEnd());
        if (sm.getFileID(spel1) != FID
            || sm.getFileID(spel2) != FID) {

            if (visibility == Visibility::Global) {
                if (usedContext && typeText.empty() && declType == Use) {
                    typeText = getContextStr(usedContext);
                }
                addReference(getReferenceAndTitle(decl).first, range.getBegin(), type, declType, typeText, decl);
            }
            return;
        }

        range = {spel1, spel2 };
    }
    clang::FileID FID = sm.getFileID(range.getBegin());

    if (FID != sm.getFileID(range.getEnd())) {
        return;
    }

    if (!shouldProcess(FID))
        return;

    std::string tags;
    std::string clas = computeClas(decl);
    std::string ref;

    const clang::Decl* canonDecl = decl->getCanonicalDecl();
    if (type != Namespace) {
        if (visibility == Visibility::Local) {
            if (!decl->getDeclName().isIdentifier())
                return; //skip local operators (FIXME)

            clang::SourceLocation loc = canonDecl->getLocation();
            int &id = localeNumbers[loc.getRawEncoding()];
            if (id == 0) id = localeNumbers.size();
            llvm::StringRef name = decl->getName();
            ref = (llvm::Twine(id) + name).str();
            llvm::SmallString<40> buffer;
            tags %= " title='" % Generator::escapeAttr(name, buffer) % "'";
            clas %= " local col" % llvm::Twine(id % 10).str();
        } else {
            auto cached =  getReferenceAndTitle(decl);
            ref = cached.first;
            tags %= " title='" % cached.second % "'";
        }

        if (visibility == Visibility::Global && type != Typedef) {
            if (usedContext && typeText.empty() && declType == Use) {
                typeText = getContextStr(usedContext);
            }

            addReference(ref, range.getBegin(), type, declType, typeText, decl);

             if (declType == Definition && ref.find('{') >= ref.size()) {
                 if (clang::FunctionDecl* fun = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
                     functionIndex.insert({fun->getQualifiedNameAsString(), ref});
                 }
             }
        } else {
            if (!typeText.empty()) {
                llvm::SmallString<40> buffer;
                tags %= " data-type='" % Generator::escapeAttr(typeText, buffer) % "'";
            }
        }

        if (visibility == Visibility::Static) {
            if (declType != Use)
                commentHandler.decl_offsets.insert({ decl->getLocStart(), {ref, false} });
            clas += " tu";
        }
    }

    switch(type) {
        case Ref: clas += " ref"; break;
        case Member: clas += " member"; break;
        case Type: clas += " type"; break;
        case Typedef: clas += " typedef"; break;
        case Decl: clas += " decl"; break;
        case Call: clas += " call"; break;
        case Namespace: clas += " namespace"; break;
        case Enum:  // fall through
        case EnumDecl: clas += " enum"; break;
    }

    if (declType == Definition && visibility != Visibility::Local) {
        clas += " def";
    }

//    const llvm::MemoryBuffer *Buf = sm.getBuffer(FID);
    clang::SourceLocation B = range.getBegin();
    clang::SourceLocation E = range.getEnd();

    int pos = sm.getFileOffset(B);
    int len =  sm.getFileOffset(E) - pos;

    // Include the whole end token in the range.
    len += clang::Lexer::MeasureTokenLength(E, sm, getLangOpts());

    canonDecl = getDefinitionDecl(decl);

    if (clas[0] == ' ') clas = clas.substr(1);

    if (ref.empty()) {
        generator(FID).addTag("span", "class=\"" % clas % "\"", pos, len);
        return;
    }

    llvm::SmallString<40> escapedRefBuffer;
    auto escapedRef = Generator::escapeAttr(ref, escapedRefBuffer);
    tags %= " data-ref=\"" % escapedRef % "\" ";

    if (declType == Annotator::Use || (decl != canonDecl && declType != Annotator::Definition) ) {
        std::string link;
        clang::SourceLocation loc = canonDecl->getLocation();
        clang::FileID declFID = sm.getFileID(sm.getExpansionLoc(loc));
        if (declFID != FID) {
            auto pr_it = project_cache.find(declFID);
            if (pr_it != project_cache.end()) {
                if (pr_it->second->type == ProjectInfo::External) {
                    tags %= "data-proj=\"" % pr_it->second->name % "\" ";
                    generator(FID).addProject(pr_it->second->name, pr_it->second->external_root_url);
                }
                link = pathTo(FID, declFID);
            }

            if (declType != Annotator::Use) {
                tags %= "id=\"" % escapedRef % "\" ";
            }

            if (link.empty()) {
                generator(FID).addTag(declType == Annotator::Use ? "span" : "dfn",
                                      "class=\'" % clas % "\'" % tags, pos, len);
                return;
            }
        }
        llvm::SmallString<6> locBuffer;
        link %= "#" % (loc.isFileID() ? escapedRef : llvm::Twine(sm.getExpansionLineNumber(loc)).toStringRef(locBuffer));
        std::string tag = "class=\"" % clas % "\" href=\"" % link % "\"" % tags;
        generator(FID).addTag("a", tag, pos, len);
    } else {
        std::string tag = "class=\"" % clas % "\" id=\"" % escapedRef % "\"" % tags;
        generator(FID).addTag("dfn", tag, pos, len);
    }
}

void Annotator::addReference(const std::string &ref, clang::SourceLocation refLoc, TokenType type,
                             DeclType dt, const std::string &typeRef, clang::Decl *decl)
{
    if (type == Ref || type == Member || type == Decl || type == Call || type == EnumDecl
        || ((type == Type || type == Enum) && dt == Definition)) {
        ssize_t size = getDeclSize(decl);
        if (size >= 0) {
            structure_sizes[ref] = size;
        }
        references[ref].push_back( std::make_tuple(dt, refLoc, typeRef) );
        if (dt != Use) {
            clang::FullSourceLoc fulloc(decl->getLocStart(), getSourceMgr());
            commentHandler.decl_offsets.insert({ fulloc.getSpellingLoc(), {ref, true} });
        }
    }
}

void Annotator::registerOverride(clang::NamedDecl* decl, clang::NamedDecl* overrided, clang::SourceLocation loc)
{
    clang::SourceManager &sm = getSourceMgr();
    clang::SourceLocation expensionloc = sm.getExpansionLoc(loc);
    clang::FileID FID = sm.getFileID(expensionloc);
    if (!shouldProcess(FID))
        return;
    if (getVisibility(overrided) != Visibility::Global)
        return;

    auto ovrRef = getReferenceAndTitle(overrided).first;
    auto declRef = getReferenceAndTitle(decl).first;
    references[ovrRef].push_back( std::make_tuple(Override, expensionloc, declRef) );

    // Register the reversed relation.
    clang::SourceLocation ovrLoc = sm.getExpansionLoc(getDefinitionDecl(overrided)->getLocation());
    references[declRef].push_back( std::make_tuple(Inherit, ovrLoc, ovrRef) );
}


void Annotator::reportDiagnostic(clang::SourceRange range, const std::string& msg, const std::string &clas)
{
    clang::SourceManager &sm = getSourceMgr();
    if (!range.getBegin().isFileID()) {
        range = sm.getSpellingLoc(range.getBegin());
        if(!range.getBegin().isFileID())
            return;
    }

    clang::FileID FID = sm.getFileID(range.getBegin());
    if (FID != sm.getFileID(range.getEnd())) {
        return;
    }
    if (!shouldProcess(FID))
        return;


    clang::SourceLocation B = range.getBegin();
    clang::SourceLocation E = range.getEnd();

    uint pos = sm.getFileOffset(B);
    uint len = sm.getFileOffset(E) - pos;

    // Include the whole end token in the range.
    pos += clang::Lexer::MeasureTokenLength(E, sm, getLangOpts());

    bool Invalid = false;
    if (Invalid)
        return;
    llvm::SmallString<40> buffer;
    generator(FID).addTag("span", "class='" % clas % "' title=\"" % Generator::escapeAttr(msg, buffer) % "\"", pos, len);
}


//basically loosely inspired from clang_getSpecializedCursorTemplate
static clang::NamedDecl *getSpecializedCursorTemplate(clang::NamedDecl *D) {
    using namespace clang;
    using namespace llvm;
    NamedDecl *Template = 0;
    if (CXXRecordDecl *CXXRecord = dyn_cast<CXXRecordDecl>(D)) {
        ClassTemplateDecl* CXXRecordT = 0;
        if (ClassTemplatePartialSpecializationDecl *PartialSpec = dyn_cast<ClassTemplatePartialSpecializationDecl>(CXXRecord))
            CXXRecordT = PartialSpec->getSpecializedTemplate();
        else if (ClassTemplateSpecializationDecl *ClassSpec = dyn_cast<ClassTemplateSpecializationDecl>(CXXRecord)) {
            llvm::PointerUnion<ClassTemplateDecl *,
            ClassTemplatePartialSpecializationDecl *> Result
            = ClassSpec->getSpecializedTemplateOrPartial();
                if (Result.is<ClassTemplateDecl *>())
                    CXXRecordT = Result.get<ClassTemplateDecl *>();
                else
                    D = CXXRecord = Result.get<ClassTemplatePartialSpecializationDecl *>();
        }
        if (CXXRecordT)
            D = CXXRecord = CXXRecordT->getTemplatedDecl();
        Template = CXXRecord->getInstantiatedFromMemberClass();
    } else if (FunctionDecl *Function = dyn_cast<FunctionDecl>(D)) {
        FunctionTemplateDecl* FunctionT = Function->getPrimaryTemplate();
        if (FunctionT) {
            D = Function = FunctionT->getTemplatedDecl();
        }
        Template = Function->getInstantiatedFromMemberFunction();
    } else if (VarDecl *Var = dyn_cast<VarDecl>(D)) {
        if (Var->isStaticDataMember())
            Template = Var->getInstantiatedFromStaticDataMember();
    } else if (RedeclarableTemplateDecl *Tmpl = dyn_cast<RedeclarableTemplateDecl>(D)) {
        Template = Tmpl->getInstantiatedFromMemberTemplate();
    }

    if (Template) return Template;
    else return D;
}


std::pair< std::string, std::string > Annotator::getReferenceAndTitle(clang::NamedDecl* decl)
{
    clang::Decl* canonDecl = decl->getCanonicalDecl();
    auto &cached = mangle_cache[canonDecl];
    if (cached.first.empty()) {
        decl = getSpecializedCursorTemplate(decl);

        std::string qualName = decl->getQualifiedNameAsString();
        if ((llvm::isa<clang::FunctionDecl>(decl) || llvm::isa<clang::VarDecl>(decl))
            && mangle->shouldMangleDeclName(decl)
            //workaround crash in clang while trying to mangle some buitins types
            && !llvm::StringRef(qualName).startswith("__")) {
                llvm::raw_string_ostream s(cached.first);
                if (llvm::isa<clang::CXXDestructorDecl>(decl)) {
                    mangle->mangleCXXDtor(llvm::cast<clang::CXXDestructorDecl>(decl), clang::Dtor_Complete, s);
                } else if (llvm::isa<clang::CXXConstructorDecl>(decl)) {
                    mangle->mangleCXXCtor(llvm::cast<clang::CXXConstructorDecl>(decl), clang::Ctor_Complete, s);
                } else {
                    mangle->mangleName(decl, s);
                }
        } else if (clang::FieldDecl *d = llvm::dyn_cast<clang::FieldDecl>(decl)) {
            cached.first = getReferenceAndTitle(d->getParent()).first + "::" + decl->getName().str();
        } else {
            cached.first = qualName;
            std::remove(cached.first.begin(), cached.first.end(), ' ');
            // replace < and > because alse jquery can't match them.
            std::replace(cached.first.begin(), cached.first.end(), '<' , '{');
            std::replace(cached.first.begin(), cached.first.end(), '>' , '}');
        }
        llvm::SmallString<40> buffer;
        cached.second = Generator::escapeAttr(qualName, buffer);
    }
    return cached;
}


std::string Annotator::getTypeRef(clang::QualType type)
{
    return type.getAsString(getLangOpts());
}

std::string Annotator::getContextStr(clang::NamedDecl* usedContext)
{
    clang::FunctionDecl *fun = llvm::dyn_cast<clang::FunctionDecl>(usedContext);
    clang::DeclContext* context = usedContext->getDeclContext();
    while(!fun && context)  {
        fun = llvm::dyn_cast<clang::FunctionDecl>(context);
        if (fun && !fun->isDefinedOutsideFunctionOrMethod())
            fun = nullptr;
        context = context->getParent();
    }
    if (fun)
        return getReferenceAndTitle(fun).first;
    return {};
}

std::string Annotator::getVisibleRef(clang::NamedDecl* Decl)
{
    if (getVisibility(Decl) != Visibility::Global)
        return {};
    return getReferenceAndTitle(Decl).first;
}

//return the classes to add in the span
std::string Annotator::computeClas(clang::NamedDecl* decl)
{
    std::string s;
    if (clang::CXXMethodDecl* f = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
        if (f->isVirtual())
            s = "virtual";
    }
    return s;
}


/* This function is inspired From clang::html::SyntaxHighlight() from HTMLRewrite.cpp
 * from the clang 3.1 from The LLVM Compiler Infrastructure
 * distributed under the University of Illinois Open Source
 * Adapted to the codebrowser generator. Also used to parse the comments.
 * The tags names have been changed, and we make a difference between different kinds of
 * keywords
 */
void Annotator::syntaxHighlight(Generator &generator, clang::FileID FID, clang::Sema &Sema) {
    using namespace clang;

    const clang::Preprocessor &PP = Sema.getPreprocessor();
    const clang::SourceManager &SM = getSourceMgr();
    const llvm::MemoryBuffer *FromFile = SM.getBuffer(FID);
    Lexer L(FID, FromFile, SM, getLangOpts());
    const char *BufferStart = FromFile->getBufferStart();
    const char *BufferEnd = FromFile->getBufferEnd();

    // Inform the preprocessor that we want to retain comments as tokens, so we
    // can highlight them.
    L.SetCommentRetentionState(true);

    // Lex all the tokens in raw mode, to avoid entering #includes or expanding
    // macros.
    Token Tok;
    L.LexFromRawLexer(Tok);

    while (Tok.isNot(tok::eof)) {
        // Since we are lexing unexpanded tokens, all tokens are from the main
        // FileID.
        unsigned TokOffs = SM.getFileOffset(Tok.getLocation());
        unsigned TokLen = Tok.getLength();
        switch (Tok.getKind()) {
            default: break;
            case tok::identifier:
                llvm_unreachable("tok::identifier in raw lexing mode!");
            case tok::raw_identifier: {
                // Fill in Result.IdentifierInfo and update the token kind,
                // looking up the identifier in the identifier table.
                PP.LookUpIdentifierInfo(Tok);
                // If this is a pp-identifier, for a keyword, highlight it as such.
                switch (Tok.getKind()) {
                    case tok::identifier:
                        break;

                    case tok::kw_auto:
                    case tok::kw_char:
                    case tok::kw_const:
                    case tok::kw_double:
                    case tok::kw_float:
                    case tok::kw_int:
                    case tok::kw_long:
                    case tok::kw_register:
//                    case tok::kw_restrict:  // ???  (type or not)
                    case tok::kw_short:
                    case tok::kw_signed:
                    case tok::kw_static:
                    case tok::kw_unsigned:
                    case tok::kw_void:
                    case tok::kw_volatile:
                    case tok::kw_bool:
                    case tok::kw_mutable:
                    case tok::kw_wchar_t:
                    case tok::kw_char16_t:
                    case tok::kw_char32_t:
                        generator.addTag("em", {}, TokOffs, TokLen);
                        break;
                    default: //other keywords
                        generator.addTag("b", {}, TokOffs, TokLen);
                }
                break;
            }
            case tok::comment: {
                unsigned int CommentBegin = TokOffs;
                unsigned int CommentLen = TokLen;
                bool startOfLine = Tok.isAtStartOfLine();
                SourceLocation CommentBeginLocation = Tok.getLocation();
                L.LexFromRawLexer(Tok);
                // Merge consecutive comments
                if (startOfLine /*&&  BufferStart[CommentBegin+1] == '/'*/) {
                    while (Tok.is(tok::comment)) {
                        unsigned int Off = SM.getFileOffset(Tok.getLocation());
                        if (BufferStart[Off+1] != '/')
                            break;
                        CommentLen = Off + Tok.getLength() - CommentBegin;
                        L.LexFromRawLexer(Tok);
                    }
                }

                std::string attributes;

                if (startOfLine) {
                    unsigned int NonCommentBegin = SM.getFileOffset(Tok.getLocation());
                    // Find the location of the next \n
                    const char *nl_it = BufferStart + NonCommentBegin;
                    while (nl_it < BufferEnd && *nl_it && *nl_it != '\n')
                        ++nl_it;
                    commentHandler.handleComment(*this, generator, Sema, BufferStart, CommentBegin, CommentLen,
                                                 Tok.getLocation(),
                                                 Tok.getLocation().getLocWithOffset(nl_it - (BufferStart + NonCommentBegin)));
                } else {
                    //look up the location before
                    const char *nl_it = BufferStart + CommentBegin;
                    while (nl_it > BufferStart && *nl_it && *nl_it != '\n')
                        --nl_it;
                    commentHandler.handleComment(*this, generator, Sema, BufferStart, CommentBegin, CommentLen,
                                                 CommentBeginLocation.getLocWithOffset(nl_it - (BufferStart + CommentBegin)),
                                                 CommentBeginLocation);
                }
                continue; //Don't skip next token
            }
            case tok::utf8_string_literal:
                // Chop off the u part of u8 prefix
                ++TokOffs;
                --TokLen;
                // FALL THROUGH to chop the 8
            case tok::wide_string_literal:
            case tok::utf16_string_literal:
            case tok::utf32_string_literal:
                // Chop off the L, u, U or 8 prefix
                ++TokOffs;
                --TokLen;
                // FALL THROUGH.
            case tok::string_literal:
                // FIXME: Exclude the optional ud-suffix from the highlighted range.
                generator.addTag("q", {}, TokOffs, TokLen);
                break;

            case tok::wide_char_constant:
            case tok::utf16_char_constant:
            case tok::utf32_char_constant:
                ++TokOffs;
                --TokLen;
            case tok::char_constant:
                generator.addTag("kdb", {}, TokOffs, TokLen);
                break;
            case tok::numeric_constant:
                generator.addTag("var", {}, TokOffs, TokLen);
                break;
            case tok::hash: {
                // If this is a preprocessor directive, all tokens to end of line are too.
                if (!Tok.isAtStartOfLine())
                    break;

                // Eat all of the tokens until we get to the next one at the start of
                // line.
                unsigned TokEnd = TokOffs+TokLen;
                L.LexFromRawLexer(Tok);
                while (!Tok.isAtStartOfLine() && Tok.isNot(tok::eof)) {
                    TokEnd = SM.getFileOffset(Tok.getLocation())+Tok.getLength();
                    L.LexFromRawLexer(Tok);
                }

                generator.addTag("u", {}, TokOffs, TokEnd - TokOffs);

                // Don't skip the next token.
                continue;
            }
        }

        L.LexFromRawLexer(Tok);
    }
}
