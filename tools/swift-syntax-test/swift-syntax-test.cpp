//===--- swift-syntax-test.cpp - Reflection Syntax testing application ----===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This is a host-side tool to perform round-trip testing of "full-fidelity"
// lexing and parsing. That is, when this application ingests a .swift file,
// it should be able to create a list of full tokens, or a full-fidelity AST,
// print them, and get the same file back out. This ensures that we aren't
// losing any source information in these structures.
//
//===----------------------------------------------------------------------===//

#include "swift/AST/DiagnosticEngine.h"
#include "swift/AST/DiagnosticsFrontend.h"
#include "swift/Basic/LangOptions.h"
#include "swift/Basic/SourceManager.h"
#include "swift/Frontend/Frontend.h"
#include "swift/Frontend/PrintingDiagnosticConsumer.h"
#include "swift/Parse/Lexer.h"
#include "swift/Subsystems.h"
#include "swift/Syntax/LegacyASTTransformer.h"
#include "swift/Syntax/Serialization/SyntaxSerialization.h"
#include "swift/Syntax/SyntaxData.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

using namespace swift;
using llvm::StringRef;

enum class ActionType {
  DumpRawTokenSyntax,
  FullLexRoundTrip,
  FullParseRoundTrip,
  SerializeRawTree,
  None
};

namespace options {
static llvm::cl::opt<ActionType>
Action(llvm::cl::desc("Action (required):"),
       llvm::cl::init(ActionType::None),
       llvm::cl::values(
        clEnumValN(ActionType::DumpRawTokenSyntax,
                   "dump-full-tokens",
                   "Lex the source file and dump the tokens "
                   "and their absolute line/column locations"),
        clEnumValN(ActionType::FullLexRoundTrip,
                   "round-trip-lex",
                   "Lex the source file and print it back out for "
                   "comparing against the original"),
        clEnumValN(ActionType::FullParseRoundTrip,
                   "round-trip-parse",
                   "Parse the source file and print it back out for "
                   "comparing against the input"),
        clEnumValN(ActionType::SerializeRawTree,
                   "serialize-raw-tree",
                   "Parse the source file and serialize the raw tree"
                   "to JSON")));

static llvm::cl::opt<std::string>
InputSourceFilename("input-source-filename",
                    llvm::cl::desc("Path to the input .swift file"));
} // end namespace options

int getTokensFromFile(unsigned BufferID,
                      LangOptions &LangOpts,
                      SourceManager &SourceMgr,
                      DiagnosticEngine &Diags,
                      std::vector<std::pair<RC<syntax::RawTokenSyntax>,
                      syntax::AbsolutePosition>> &Tokens) {
  Tokens = tokenizeWithTrivia(LangOpts, SourceMgr, BufferID);
  return Diags.hadAnyError() ? EXIT_FAILURE : EXIT_SUCCESS;
}


int
getTokensFromFile(const StringRef InputFilename,
                  LangOptions &LangOpts,
                  SourceManager &SourceMgr,
                  DiagnosticEngine &Diags,
                  std::vector<std::pair<RC<syntax::RawTokenSyntax>,
                                        syntax::AbsolutePosition>> &Tokens) {
  auto Buffer = llvm::MemoryBuffer::getFile(InputFilename);
  if (!Buffer) {
    Diags.diagnose(SourceLoc(), diag::cannot_open_file,
                   InputFilename, Buffer.getError().message());
    return EXIT_FAILURE;
  }

  auto BufferID = SourceMgr.addNewSourceBuffer(std::move(Buffer.get()));
  return getTokensFromFile(BufferID, LangOpts, SourceMgr, Diags, Tokens);
}

void anchorForGetMainExecutable() {}

int getSyntaxTree(const char *MainExecutablePath,
                  const StringRef InputFilename,
                  CompilerInstance &Instance,
                  llvm::SmallVectorImpl<syntax::Syntax> &TopLevelDecls,
                  std::vector<std::pair<RC<syntax::RawTokenSyntax>,
                              syntax::AbsolutePosition>> &Tokens) {
  CompilerInvocation Invocation;
  Invocation.addInputFilename(InputFilename);

  Invocation.setMainExecutablePath(
    llvm::sys::fs::getMainExecutable(MainExecutablePath,
      reinterpret_cast<void *>(&anchorForGetMainExecutable)));

  Invocation.setModuleName("Test");

  auto &SourceMgr = Instance.getSourceMgr();

  PrintingDiagnosticConsumer DiagPrinter;
  Instance.addDiagnosticConsumer(&DiagPrinter);
  if (Instance.setup(Invocation)) {
    return EXIT_FAILURE;
  }

  // First, parse the file normally and get the regular old AST.
  Instance.performParseOnly();

  if (Instance.getDiags().hadAnyError()) {
    return EXIT_FAILURE;
  }

  auto BufferID = Instance.getInputBufferIDs().back();
  SourceFile *SF = nullptr;
  for (auto Unit : Instance.getMainModule()->getFiles()) {
    SF = dyn_cast<SourceFile>(Unit);
    if (SF != nullptr) {
      break;
    }
  }
  assert(SF && "No source file");

  // Retokenize the buffer with full fidelity
  if (getTokensFromFile(BufferID, Invocation.getLangOptions(),
                        SourceMgr,
                        Instance.getDiags(), Tokens) == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }

  SmallVector<Decl *, 256> FileDecls;
  SF->getTopLevelDecls(FileDecls);
  sema::Semantics Sema;
  // Convert the old ASTs to the new full-fidelity syntax tree and print
  // them out.
  for (auto *Decl : FileDecls) {
    if (Decl->escapedFromIfConfig()) {
      continue;
    }
    auto NewNode = transformAST(ASTNode(Decl), Sema, SourceMgr,
                                BufferID, Tokens);
    if (NewNode.hasValue()) {
      TopLevelDecls.push_back(NewNode.getValue());
    }
  }

  return EXIT_SUCCESS;
}

int doFullLexRoundTrip(const StringRef InputFilename) {
  LangOptions LangOpts;
  SourceManager SourceMgr;
  DiagnosticEngine Diags(SourceMgr);
  PrintingDiagnosticConsumer DiagPrinter;
  Diags.addConsumer(DiagPrinter);

  std::vector<std::pair<RC<syntax::RawTokenSyntax>,
                                   syntax::AbsolutePosition>> Tokens;
  if (getTokensFromFile(InputFilename, LangOpts, SourceMgr,
                        Diags, Tokens) == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }

  for (auto TokAndPos : Tokens) {
    TokAndPos.first->print(llvm::outs());
  }

  return Diags.hadAnyError() ? EXIT_FAILURE : EXIT_SUCCESS;
}

int doDumpRawTokenSyntax(const StringRef InputFilename) {
  LangOptions LangOpts;
  SourceManager SourceMgr;
  DiagnosticEngine Diags(SourceMgr);
  PrintingDiagnosticConsumer DiagPrinter;
  Diags.addConsumer(DiagPrinter);

  std::vector<std::pair<RC<syntax::RawTokenSyntax>,
                        syntax::AbsolutePosition>> Tokens;
  if (getTokensFromFile(InputFilename, LangOpts, SourceMgr,
                        Diags, Tokens) == EXIT_FAILURE) {
    return EXIT_FAILURE;
  }

  for (auto TokAndPos : Tokens) {
    TokAndPos.second.printLineAndColumn(llvm::outs());
    llvm::outs() << "\n";
    TokAndPos.first->dump(llvm::outs());
    llvm::outs() << "\n";
  }

  return Diags.hadAnyError() ? EXIT_FAILURE : EXIT_SUCCESS;
}

int doFullParseRoundTrip(const char *MainExecutablePath,
                         const StringRef InputFilename) {

  llvm::SmallVector<syntax::Syntax, 10> TopLevelDecls;
  std::vector<std::pair<RC<syntax::RawTokenSyntax>,
                        syntax::AbsolutePosition>> Tokens;
  CompilerInstance Instance;

  getSyntaxTree(MainExecutablePath, InputFilename, Instance,
                TopLevelDecls, Tokens);

  for (auto &Node : TopLevelDecls) {
    Node.print(llvm::outs());
  }

  if (Tokens.back().first->getTokenKind() == tok::eof) {
    Tokens.back().first->print(llvm::outs());
  }

  return EXIT_SUCCESS;
}

int doSerializeRawTree(const char *MainExecutablePath,
                       const StringRef InputFilename) {

  llvm::SmallVector<syntax::Syntax, 10> TopLevelDecls;
  std::vector<std::pair<RC<syntax::RawTokenSyntax>,
                        syntax::AbsolutePosition>> Tokens;
  CompilerInstance Instance;

  getSyntaxTree(MainExecutablePath, InputFilename, Instance,
                TopLevelDecls, Tokens);

  std::vector<RC<syntax::RawSyntax>> RawTopLevelDecls;
  for (auto &Decl : TopLevelDecls) {
    RawTopLevelDecls.push_back(Decl.getRaw());
  }

  swift::json::Output out(llvm::outs());
  out << RawTopLevelDecls;

  llvm::outs() << "\n";

  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  llvm::cl::ParseCommandLineOptions(argc, argv, "Swift Syntax Test\n");

  int ExitCode = EXIT_SUCCESS;

  if (options::InputSourceFilename.empty()) {
    llvm::errs() << "input source file is required\n";
    ExitCode = EXIT_FAILURE;
  }

  if (ExitCode == EXIT_FAILURE) {
    llvm::cl::PrintHelpMessage();
    return ExitCode;
  }

  switch (options::Action) {
  case ActionType::DumpRawTokenSyntax:
    ExitCode = doDumpRawTokenSyntax(options::InputSourceFilename);
    break;
  case ActionType::FullLexRoundTrip:
    ExitCode = doFullLexRoundTrip(options::InputSourceFilename);
    break;
  case ActionType::FullParseRoundTrip:
    ExitCode = doFullParseRoundTrip(argv[0], options::InputSourceFilename);
    break;
  case ActionType::SerializeRawTree:
    ExitCode = doSerializeRawTree(argv[0], options::InputSourceFilename);
    break;
  case ActionType::None:
    llvm::errs() << "an action is required\n";
    llvm::cl::PrintHelpMessage();
    ExitCode = EXIT_FAILURE;
    break;
  }

  return ExitCode;
}
