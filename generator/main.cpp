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

#include "llvm/Support/CommandLine.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/ASTContext.h"
#include "llvm/Support/Signals.h"

#include <clang/Frontend/CompilerInstance.h>

#include <iostream>
#include <fstream>
#include <limits>
#include <stdexcept>
#include "annotator.h"
#include "stringbuilder.h"
#include "browserastvisitor.h"
#include "preprocessorcallback.h"
#include "projectmanager.h"
#include "filesystem.h"
#include <ctime>

namespace cl = llvm::cl;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static cl::OptionCategory MyToolCategory("my-tool options");

static cl::list<std::string> SourcePaths(cl::Positional,
                                         cl::desc("[<file> ...]"),
                                         cl::ZeroOrMore,
                                         cl::cat(MyToolCategory));

cl::opt<std::string> BuildPath("b", cl::desc("<build-path>"), cl::Optional,
                               cl::cat(MyToolCategory));

cl::opt<std::string> OutputPath("o", cl::desc("<output path>"), cl::Required,
                                cl::cat(MyToolCategory));

cl::list<std::string> ProjectPaths("p",
                                   cl::desc("<project>:<path>[:<revision>]"),
                                   cl::ZeroOrMore, cl::cat(MyToolCategory));

cl::list<std::string> ExternalProjectPaths("e",
                                           cl::desc("<project>:<path>:<url>"),
                                           cl::ZeroOrMore,
                                           cl::cat(MyToolCategory));

cl::opt<std::string> DataPath("d", cl::desc("<data path>"), cl::Optional,
                              cl::cat(MyToolCategory));

cl::opt<bool> ProcessAllSources(
    "a", cl::desc("Process all sources in the compilation_database.json "
                  "(should not have sources then)"),
    cl::cat(MyToolCategory));

#if 1
std::string locationToString(clang::SourceLocation loc,
                             clang::SourceManager &sm) {
  clang::PresumedLoc fixed = sm.getPresumedLoc(loc);
  if (!fixed.isValid())
    return "???";
  return (llvm::Twine(fixed.getFilename()) + ":" + llvm::Twine(fixed.getLine()))
      .str();
}
#endif

struct BrowserDiagnosticClient : clang::DiagnosticConsumer {
  Annotator &annotator;
  BrowserDiagnosticClient(Annotator &fm) : annotator(fm) {}

#if CLANG_VERSION_MAJOR == 3 && CLANG_VERSION_MINOR <= 2
  virtual DiagnosticConsumer *
  clone(clang::DiagnosticsEngine &Diags) const override {
    return new BrowserDiagnosticClient(annotator);
  }
#endif

  virtual void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel,
                                const clang::Diagnostic &Info) override {
    std::string clas;
    llvm::SmallString<1000> diag;
    Info.FormatDiagnostic(diag);

    switch (DiagLevel) {
    case clang::DiagnosticsEngine::Fatal:
      std::cerr << "FATAL ";
    case clang::DiagnosticsEngine::Error:
      std::cerr << "Error: " << locationToString(Info.getLocation(),
                                                 annotator.getSourceMgr())
                << ": " << diag.c_str() << std::endl;
      clas = "error";
      break;
    case clang::DiagnosticsEngine::Warning:
      clas = "warning";
      break;
    default:
      return;
    }
    clang::SourceRange Range = Info.getLocation();
    annotator.reportDiagnostic(Range, diag.c_str(), clas);
  }
};

class BrowserASTConsumer : public clang::ASTConsumer {
  clang::CompilerInstance &ci;
  Annotator annotator;
  bool WasInDatabase;

public:
  BrowserASTConsumer(clang::CompilerInstance &ci,
                     ProjectManager &projectManager, bool WasInDatabase)
      : clang::ASTConsumer(), ci(ci), annotator(projectManager),
        WasInDatabase(WasInDatabase) {
    // ci.getLangOpts().DelayedTemplateParsing = (true);
    ci.getPreprocessor().enableIncrementalProcessing();
  }
  virtual ~BrowserASTConsumer() {
    ci.getDiagnostics().setClient(new clang::IgnoringDiagConsumer, true);
  }

  virtual void Initialize(clang::ASTContext &Ctx) override {
    annotator.setSourceMgr(Ctx.getSourceManager(), Ctx.getLangOpts());
    annotator.setMangleContext(Ctx.createMangleContext());
    ci.getPreprocessor().addPPCallbacks(std::unique_ptr<PreprocessorCallback>(
        new PreprocessorCallback(annotator, ci.getPreprocessor())));
    ci.getDiagnostics().setClient(new BrowserDiagnosticClient(annotator), true);
  }

  virtual bool HandleTopLevelDecl(clang::DeclGroupRef D) {
    if (ci.getDiagnostics().hasFatalErrorOccurred()) {
      // Reset errors: (Hack to ignore the fatal errors.)
      ci.getDiagnostics().Reset();
      // When there was fatal error, processing the warnings may cause crashes
      ci.getDiagnostics().setIgnoreAllWarnings(true);
    }
    return true;
  }

  virtual void HandleTranslationUnit(clang::ASTContext &Ctx) override {

    /* if (PP.getDiagnostics().hasErrorOccurred())
         return;*/
    ci.getPreprocessor().getDiagnostics().getClient();

    BrowserASTVisitor v(annotator);
    v.TraverseDecl(Ctx.getTranslationUnitDecl());

    annotator.generate(ci.getSema(), WasInDatabase);
  }

  virtual bool shouldSkipFunctionBody(clang::Decl *D) {
    return !annotator.shouldProcess(
        clang::FullSourceLoc(D->getLocation(), annotator.getSourceMgr())
            .getExpansionLoc()
            .getFileID());
  }
};

namespace HasShouldSkipBody_HELPER {
template <class T>
static decltype(static_cast<T *>(nullptr)->shouldSkipFunctionBody(nullptr))
test(int);
template <class T> static double test(...);
}

class BrowserAction : public clang::ASTFrontendAction {
  static std::set<std::string> processed;
  bool WasInDatabase;

protected:
  virtual std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI,
                    llvm::StringRef InFile) override {
    if (false && processed.count(InFile.str())) {
      std::cerr << "Skipping already processed " << InFile.str() << std::endl;
      return nullptr;
    }
    processed.insert(InFile.str());

    CI.getFrontendOpts().SkipFunctionBodies =
        sizeof(HasShouldSkipBody_HELPER::test<clang::ASTConsumer>(0)) ==
        sizeof(bool);

    return std::unique_ptr<clang::ASTConsumer>(
        new BrowserASTConsumer(CI, *projectManager, WasInDatabase));
  }

public:
  BrowserAction(bool WasInDatabase = true) : WasInDatabase(WasInDatabase) {}
  virtual bool hasCodeCompletionSupport() const { return true; }
  static ProjectManager *projectManager;
};

std::set<std::string> BrowserAction::processed;
ProjectManager *BrowserAction::projectManager = nullptr;

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal();

  std::unique_ptr<clang::tooling::CompilationDatabase> Compilations(
      clang::tooling::FixedCompilationDatabase::loadFromCommandLine(argc,
                                                                    argv));
  cl::ParseCommandLineOptions(argc, argv);

  if (!Compilations) {
    std::string ErrorMessage;
    if (!BuildPath.empty()) {
      Compilations =
          clang::tooling::CompilationDatabase::autoDetectFromDirectory(
              BuildPath, ErrorMessage);
    } else {
      Compilations = clang::tooling::CompilationDatabase::autoDetectFromSource(
          SourcePaths[0], ErrorMessage);
    }
    if (!Compilations)
      llvm::report_fatal_error(ErrorMessage);
  }

  ProjectManager projectManager(OutputPath, DataPath);
  for (std::string &s : ProjectPaths) {
    auto colonPos = s.find(':');
    if (colonPos >= s.size()) {
      std::cerr << "fail to parse project option : " << s << std::endl;
      continue;
    }
    auto secondColonPos = s.find(':', colonPos + 1);
    ProjectInfo info{s.substr(0, colonPos),
                     s.substr(colonPos + 1, secondColonPos - colonPos - 1),
                     secondColonPos < s.size() ? s.substr(secondColonPos + 1)
                                               : std::string()};
    projectManager.addProject(std::move(info));
  }
  for (std::string &s : ExternalProjectPaths) {
    auto colonPos = s.find(':');
    if (colonPos >= s.size()) {
      std::cerr << "fail to parse project option : " << s << std::endl;
      continue;
    }
    auto secondColonPos = s.find(':', colonPos + 1);
    if (secondColonPos >= s.size()) {
      std::cerr << "fail to parse project option : " << s << std::endl;
      continue;
    }
    ProjectInfo info{s.substr(0, colonPos),
                     s.substr(colonPos + 1, secondColonPos - colonPos - 1),
                     ProjectInfo::External};
    info.external_root_url = s.substr(secondColonPos + 1);
    projectManager.addProject(std::move(info));
  }
  BrowserAction::projectManager = &projectManager;

  std::vector<std::string> Sources = SourcePaths;
  std::vector<std::string> AllFiles = Compilations->getAllFiles();
  std::sort(AllFiles.begin(), AllFiles.end());
  if (Sources.empty() && ProcessAllSources) {
    // Because else the order is too random
    Sources = AllFiles;
  } else if (ProcessAllSources) {
    std::cerr << "Cannot use both sources and  '-a'" << std::endl;
    return EXIT_FAILURE;
  }

  if (Sources.empty()) {
    std::cerr
        << "No source files.  Please pass source files as argument, or use '-a'"
        << std::endl;
    return EXIT_FAILURE;
  }

  for (auto it : Sources) {
    auto file = clang::tooling::getAbsolutePath(it);

    std::cout << "processing source = " << file << std::endl;

    clang::tooling::ClangTool Tool(*Compilations, file);
    Tool.appendArgumentsAdjuster(new clang::tooling::ClangSyntaxOnlyAdjuster());
    Tool.appendArgumentsAdjuster(
        new clang::tooling::ClangStripOutputAdjuster());
    bool success = Tool.run(
        clang::tooling::newFrontendActionFactory<BrowserAction>().get());

    if (success) {
      ProjectInfo *projectinfo = projectManager.projectForFile(file);
      if (!projectinfo)
        continue;

      auto now = std::time(0);
      auto tm = localtime(&now);
      char buf[80];
      std::strftime(buf, sizeof(buf), "%Y-%b-%d", tm);

      std::string footer = "Generated on <em>" % std::string(buf) % "</em>" %
                           " from project " % projectinfo->name % "</a>";
      std::cout << footer << std::endl;
      if (!projectinfo->revision.empty())
        footer %= " revision <em>" % projectinfo->revision % "</em>";

      auto B = llvm::MemoryBuffer::getFile(file);
      if (!B)
        continue;
      std::unique_ptr<llvm::MemoryBuffer> Buf = std::move(B.get());

      std::string fn =
          projectinfo->name % "/" %
          llvm::StringRef(file).substr(projectinfo->source_path.size());

      Generator g;
      g.generate(projectManager.outputPrefix, projectManager.dataPath, fn,
                 Buf->getBufferStart(), Buf->getBufferEnd(), footer,
                 "Warning: This file is not a C or C++ file. It does not have "
                 "highlighting.");

      std::ofstream fileIndex;
      fileIndex.open(projectManager.outputPrefix + "/otherIndex",
                     std::ios::app);
      if (!fileIndex)
        continue;
      fileIndex << fn << '\n';
    }
  }
}
