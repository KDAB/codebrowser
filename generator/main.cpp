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
 * purchasing a comerial licence.
 ****************************************************************************/


#include "llvm/Support/CommandLine.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/ASTContext.h"

#include <clang/Frontend/CompilerInstance.h>


#include <iostream>
#include <limits>
#include <stdexcept>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "annotator.h"

#include "browserastvisitor.h"
#include "preprocessorcallback.h"


namespace cl = llvm::cl;

cl::opt<std::string> BuildPath(
  "b",
  cl::desc("<build-path>"),
  cl::Optional);

cl::list<std::string> SourcePaths(
  cl::Positional,
  cl::desc("<source0> [... <sourceN>]"),
  cl::OneOrMore);

cl::opt<std::string> OutputPath(
    "o",
    cl::desc("<output path>"),
    cl::Required);


cl::list<std::string> ProjectPaths(
    "p",
    cl::desc("<project>:<path>"),
    cl::ZeroOrMore);


#if 1
std::string locationToString(clang::SourceLocation loc, clang::SourceManager& sm) {
    clang::PresumedLoc fixed = sm.getPresumedLoc(loc);
    if (!fixed.isValid())
        return "???";
    std::string buffer = fixed.getFilename();
    buffer += ":";
    buffer += boost::lexical_cast<std::string>(fixed.getLine());
//    buffer += ":";
//    buffer += fixed.getColumn();
    return buffer;
}
#endif

struct BrowserDiagnosticClient : clang::DiagnosticConsumer {
    Annotator &annotator;
    BrowserDiagnosticClient(Annotator &fm) : annotator(fm) {}

    virtual DiagnosticConsumer* clone(clang::DiagnosticsEngine& Diags) const override {
        return new BrowserDiagnosticClient(annotator);
    }

    virtual void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel, const clang::Diagnostic& Info) override {
        std::string clas;
        llvm::SmallString<1000> diag;
        Info.FormatDiagnostic(diag);

        switch(DiagLevel) {
            case clang::DiagnosticsEngine::Fatal:
                std::cerr << "FATAL ";
            case clang::DiagnosticsEngine::Error:
                std::cerr << "Error: " << locationToString(Info.getLocation(), annotator.getSourceMgr())
                            << ": " << diag.c_str() << std::endl;
                clas = "error";
                break;
            case clang::DiagnosticsEngine::Warning:
                clas = "warning";
                break;
            default:
                return;
        }
        annotator.reportDiagnostic(Info.getLocation(), diag.c_str(), clas);
    }
};

class MyASTConsumer : public clang::ASTConsumer
{
    clang::CompilerInstance &ci;
    Annotator annotator;
public:
    MyASTConsumer(clang::CompilerInstance &ci) : clang::ASTConsumer(), ci(ci),
            annotator (OutputPath)

    {
        for(std::string &s : ProjectPaths) {
            auto colonPos = s.find(':');
            if (colonPos >= s.size()) {
                std::cerr << "fail to parse project option : " << s << std::endl;
                continue;
            }
            auto secondColonPos = s.find(':', colonPos+1);
            annotator.addProject(s.substr(0, colonPos), s.substr(colonPos+1, secondColonPos - colonPos -1),
                                 secondColonPos < s.size() ? s.substr(secondColonPos + 1) : std::string());
        }

        //ci.getLangOpts().DelayedTemplateParsing = (true);
        ci.getPreprocessor().enableIncrementalProcessing();
    }
    virtual ~MyASTConsumer() {
	        ci.getDiagnostics().setClient(new clang::IgnoringDiagConsumer, true);
	 }

    virtual void Initialize(clang::ASTContext& Ctx) override {
        annotator.setSourceMgr(Ctx.getSourceManager(), Ctx.getLangOpts());
        annotator.setMangleContext(Ctx.createMangleContext());
        ci.getPreprocessor().addPPCallbacks(new PreprocessorCallback(annotator, ci.getPreprocessor()));
        ci.getDiagnostics().setClient(new BrowserDiagnosticClient(annotator), true);
    }

    virtual void HandleTranslationUnit(clang::ASTContext& Ctx) override {

       /* if (PP.getDiagnostics().hasErrorOccurred())
            return;*/
        ci.getPreprocessor().getDiagnostics().getClient();


        BrowserASTVisitor v(annotator);
        v.TraverseDecl(Ctx.getTranslationUnitDecl());


        annotator.generate(ci.getPreprocessor());
    }

    virtual bool shouldSkipFunctionBody(clang::Decl *D) {
        return !annotator.shouldProcess(
            clang::FullSourceLoc(D->getLocation(),annotator.getSourceMgr())
                .getExpansionLoc().getFileID());
    }
};

namespace HasShouldSkipBody_HELPER {
    template<class T> static decltype(static_cast<T*>(nullptr)->shouldSkipFunctionBody(nullptr)) test(int);
    template<class T> static double test(...);
}



class MyAction : public clang::ASTFrontendAction {
    static std::set<std::string> processed;
protected:
    virtual clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &CI,
                                           llvm::StringRef InFile) override {
        if (processed.count(InFile.str())) {
            std::cerr << "####  Skipping already processed " << InFile.str()<< std::endl;
            return nullptr;
        }
        processed.insert(InFile.str());

        CI.getFrontendOpts().SkipFunctionBodies =
            sizeof(HasShouldSkipBody_HELPER::test<clang::ASTConsumer>(0)) == sizeof(bool);

        return new MyASTConsumer(CI);
    }

public:
    virtual bool hasCodeCompletionSupport() const { return true; }
};


std::set<std::string> MyAction::processed;



using namespace clang::tooling;

int main(int argc, const char **argv) {
  llvm::OwningPtr<CompilationDatabase> Compilations(
    FixedCompilationDatabase::loadFromCommandLine(argc, argv));
  llvm::cl::ParseCommandLineOptions(argc, argv);
  if (!Compilations) {
    std::string ErrorMessage;
    Compilations.reset(CompilationDatabase::loadFromDirectory(BuildPath,
                                                            ErrorMessage));

    /*if (!BuildPath.empty()) {
      Compilations.reset(
         CompilationDatabase::autoDetectFromDirectory(BuildPath, ErrorMessage));
    } else {
      Compilations.reset(CompilationDatabase::autoDetectFromSource(
          SourcePaths[0], ErrorMessage));
    }*/
    if (!Compilations)
      llvm::report_fatal_error(ErrorMessage);
  }
  ClangTool Tool(*Compilations, SourcePaths);
  return Tool.run(newFrontendActionFactory<MyAction>());
}

