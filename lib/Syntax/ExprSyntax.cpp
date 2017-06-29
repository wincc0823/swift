//===--- ExprSyntax.cpp - Swift Expression Syntax Impl. -------------------===//
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

#include "swift/Syntax/ExprSyntax.h"
#include "swift/Syntax/GenericSyntax.h"

using namespace swift;
using namespace swift::syntax;

#pragma mark - expression API

ExprSyntax ExprSyntax::makeBlank() {
  return make<ExprSyntax>(RawSyntax::missing(SyntaxKind::MissingExpr));
}

#pragma mark - unknown-expression API

void UnknownExprSyntax::validate() const {
  assert(Data->Raw->Kind == SyntaxKind::UnknownExpr);
}

#pragma mark - integer-literal-expression API

void IntegerLiteralExprSyntax::validate() const {
  assert(Data->Raw->Kind == SyntaxKind::IntegerLiteralExpr);
  assert(Data->Raw->Layout.size() == 2);
  syntax_assert_child_token(Data->Raw, Cursor::Sign,
                            tok::oper_prefix);
  syntax_assert_child_token(Data->Raw, Cursor::Digits,
                            tok::integer_literal);
}

IntegerLiteralExprSyntax IntegerLiteralExprSyntax::makeBlank() {
  auto Raw = RawSyntax::make(SyntaxKind::IntegerLiteralExpr,
    {
      RawTokenSyntax::missingToken(tok::oper_prefix, ""),
      RawTokenSyntax::missingToken(tok::integer_literal, "")
    },
    SourcePresence::Present);
  return make<IntegerLiteralExprSyntax>(Raw);
}

IntegerLiteralExprSyntax
IntegerLiteralExprSyntax::withDigits(TokenSyntax NewDigits) const {
  assert(NewDigits.getTokenKind() == tok::integer_literal);
  return Data->replaceChild<IntegerLiteralExprSyntax>(NewDigits.getRaw(),
                                                      Cursor::Digits);
}

IntegerLiteralExprSyntax
IntegerLiteralExprSyntax::withSign(TokenSyntax NewSign) const {
    assert(NewSign.getTokenKind() == tok::oper_prefix);
    return Data->replaceChild<IntegerLiteralExprSyntax>(NewSign.getRaw(),
                                                        Cursor::Sign);
}

#pragma mark - symbolic-reference API

void SymbolicReferenceExprSyntax::validate() const {
  assert(Data->Raw->Layout.size() == 2);
  syntax_assert_child_token(Data->Raw, Cursor::Identifier, tok::identifier);
  syntax_assert_child_kind(Data->Raw, Cursor::GenericArgumentClause,
                           SyntaxKind::GenericArgumentClause);
}

SymbolicReferenceExprSyntax SymbolicReferenceExprSyntax::makeBlank() {
  auto Raw = RawSyntax::make(SyntaxKind::SymbolicReferenceExpr,
    {
      RawTokenSyntax::missingToken(tok::identifier, ""),
      RawSyntax::missing(SyntaxKind::GenericArgumentClause),
    },
    SourcePresence::Present);
  return make<SymbolicReferenceExprSyntax>(Raw);
}

TokenSyntax SymbolicReferenceExprSyntax::getIdentifier() const {
  return { Root, Data->getChild(Cursor::Identifier).get() };
}

SymbolicReferenceExprSyntax SymbolicReferenceExprSyntax::
withIdentifier(TokenSyntax NewIdentifier) const {
  assert(NewIdentifier.getTokenKind() == tok::identifier);
  return Data->replaceChild<SymbolicReferenceExprSyntax>(NewIdentifier.getRaw(),
                                                         Cursor::Identifier);
}

llvm::Optional<GenericArgumentClauseSyntax>
SymbolicReferenceExprSyntax::getGenericArgumentClause() const {
  auto RawClause = getRaw()->getChild(Cursor::GenericArgumentClause);
  if (RawClause->isMissing()) {
    return llvm::None;
  }

  return llvm::Optional<GenericArgumentClauseSyntax> {
    GenericArgumentClauseSyntax {
      Root,
      Data->getChild(Cursor::GenericArgumentClause).get()
    }
  };
}

SymbolicReferenceExprSyntax SymbolicReferenceExprSyntax::
withGenericArgumentClause(GenericArgumentClauseSyntax NewGenericArgs) const {
  return Data->replaceChild<SymbolicReferenceExprSyntax>(
    NewGenericArgs.getRaw(), Cursor::GenericArgumentClause);
}

#pragma mark - function-call-argument Data

void FunctionCallArgumentSyntax::validate() const {
  syntax_assert_child_token(Data->Raw, Cursor::Label,
                            tok::identifier);
  syntax_assert_child_token_text(Data->Raw, Cursor::Colon,
                                 tok::colon, ":");
  assert(Data->Raw->getChild(Cursor::Expression)->isExpr());

  syntax_assert_child_token_text(Data->Raw,
                                 Cursor::Comma,
                                 tok::comma, ",");
}

FunctionCallArgumentSyntax FunctionCallArgumentSyntax::makeBlank() {
  auto Raw = RawSyntax::make(SyntaxKind::FunctionCallArgument,
                             {
                               RawTokenSyntax::missingToken(tok::identifier, ""),
                               RawTokenSyntax::missingToken(tok::colon, ":"),
                               RawSyntax::missing(SyntaxKind::MissingExpr),
                               RawTokenSyntax::missingToken(tok::comma, ",")
                             },
                             SourcePresence::Present);
  return make<FunctionCallArgumentSyntax>(Raw);
}

#pragma mark - function-call-argument API

TokenSyntax FunctionCallArgumentSyntax::getLabel() const {
  return { Root, Data->getChild(Cursor::Label).get() };
}

FunctionCallArgumentSyntax
FunctionCallArgumentSyntax::withLabel(TokenSyntax NewLabel) const {
  assert(NewLabel.getTokenKind() == tok::identifier);
  return Data->replaceChild<FunctionCallArgumentSyntax>(NewLabel.getRaw(),
                                                        Cursor::Label);
}

TokenSyntax FunctionCallArgumentSyntax::getColonToken() const {
  return { Root, Data->getChild(Cursor::Colon).get() };
}

FunctionCallArgumentSyntax
FunctionCallArgumentSyntax::withColonToken(TokenSyntax NewColon) const {
  syntax_assert_token_is(NewColon, tok::colon, ":");
  return Data->replaceChild<FunctionCallArgumentSyntax>(NewColon.getRaw(),
                                                        Cursor::Colon);
}

llvm::Optional<ExprSyntax> FunctionCallArgumentSyntax::getExpression() const {

  auto RawExpression = getRaw()->getChild(Cursor::Expression);
  if (RawExpression->isMissing()) {
    return llvm::None;
  }
  
  return llvm::Optional<ExprSyntax> {
    ExprSyntax { Root, Data->getChild(Cursor::Expression).get() }
  };
}

FunctionCallArgumentSyntax
FunctionCallArgumentSyntax::withExpression(ExprSyntax NewExpression) const {
  return Data->replaceChild<FunctionCallArgumentSyntax>(NewExpression.getRaw(),
                                                        Cursor::Expression);
}

TokenSyntax FunctionCallArgumentSyntax::getTrailingComma() const {
  return { Root, Data->getChild(Cursor::Comma).get() };
}

FunctionCallArgumentSyntax FunctionCallArgumentSyntax::
withTrailingComma(TokenSyntax NewTrailingComma) const {
  syntax_assert_token_is(NewTrailingComma, tok::comma, ",");
  return Data->replaceChild<FunctionCallArgumentSyntax>(
            NewTrailingComma.getRaw(), Cursor::Comma);
}

#pragma mark - function-call-expression Data

void FunctionCallExprSyntax::validate() const {
  auto Raw = Data->Raw;
  assert(Raw->getChild(Cursor::CalledExpression)
           ->isExpr());
  syntax_assert_child_token_text(Raw, Cursor::LeftParen,
                                 tok::l_paren, "(");
  syntax_assert_child_kind(Raw, Cursor::ArgumentList,
                           SyntaxKind::FunctionCallArgumentList);
  syntax_assert_child_token_text(Raw,
                                 Cursor::RightParen,
                                 tok::r_paren, ")");
}

FunctionCallExprSyntax FunctionCallExprSyntax::makeBlank() {
  auto Raw = RawSyntax::make(SyntaxKind::FunctionCallExpr,
  {
    RawSyntax::missing(SyntaxKind::MissingExpr),
    RawTokenSyntax::missingToken(tok::l_paren, "("),
    RawSyntax::missing(SyntaxKind::FunctionCallArgumentList),
    RawTokenSyntax::missingToken(tok::r_paren, ")"),
  },
  SourcePresence::Present);
  return make<FunctionCallExprSyntax>(Raw);
}

#pragma mark - function-call-expression API

ExprSyntax FunctionCallExprSyntax::getCalledExpression() const {
  return {
    Root,
    Data->getChild(Cursor::CalledExpression).get(),
  };
}

FunctionCallExprSyntax FunctionCallExprSyntax::
withCalledExpression(ExprSyntax NewBaseExpression) const {
  return Data->replaceChild<FunctionCallExprSyntax>(NewBaseExpression.getRaw(),
                                                    Cursor::CalledExpression);
}

TokenSyntax FunctionCallExprSyntax::getLeftParen() const {
  return { Root, Data->getChild(Cursor::LeftParen).get() };
}

FunctionCallExprSyntax
FunctionCallExprSyntax::withLeftParen(TokenSyntax NewLeftParen) const {
  syntax_assert_token_is(NewLeftParen, tok::l_paren, "(");
  return Data->replaceChild<FunctionCallExprSyntax>(NewLeftParen.getRaw(),
                                                    Cursor::LeftParen);
}

FunctionCallArgumentListSyntax FunctionCallExprSyntax::getArgumentList() const {
  return FunctionCallArgumentListSyntax {
    Root,
    Data->getChild(Cursor::ArgumentList).get(),
  };
}

FunctionCallExprSyntax FunctionCallExprSyntax::
withArgumentList(FunctionCallArgumentListSyntax NewArgumentList) const {
  return Data->replaceChild<FunctionCallExprSyntax>(NewArgumentList.getRaw(),
                                                    Cursor::ArgumentList);
}

TokenSyntax FunctionCallExprSyntax::getRightParen() const {
  return { Root, Data->getChild(Cursor::RightParen).get() };
}

FunctionCallExprSyntax
FunctionCallExprSyntax::withRightParen(TokenSyntax NewRightParen) const {
  syntax_assert_token_is(NewRightParen, tok::r_paren, ")");
  return Data->replaceChild<FunctionCallExprSyntax>(NewRightParen.getRaw(),
                                                    Cursor::RightParen);
}

#pragma mark - function-call-expression Builder

FunctionCallExprSyntaxBuilder::FunctionCallExprSyntaxBuilder()
  : CallLayout(FunctionCallExprSyntax::makeBlank().getRaw()->Layout),
    ListLayout(FunctionCallArgumentListSyntax::makeBlank().getRaw()->Layout) {}

FunctionCallExprSyntaxBuilder &
FunctionCallExprSyntaxBuilder::useLeftParen(TokenSyntax LeftParen) {
  syntax_assert_token_is(LeftParen, tok::l_paren, "(");
  CallLayout[cursorIndex(FunctionCallExprSyntax::Cursor::LeftParen)]
    = LeftParen.getRaw();
  return *this;
}

FunctionCallExprSyntaxBuilder &FunctionCallExprSyntaxBuilder::
appendArgument(FunctionCallArgumentSyntax AdditionalArgument) {
  ListLayout.push_back(AdditionalArgument.getRaw());
  return *this;
}

FunctionCallExprSyntaxBuilder &FunctionCallExprSyntaxBuilder::
useCalledExpression(ExprSyntax CalledExpression) {
  CallLayout[cursorIndex(FunctionCallExprSyntax::Cursor::CalledExpression)]
    = CalledExpression.getRaw();
  return *this;
}

FunctionCallExprSyntaxBuilder &
FunctionCallExprSyntaxBuilder::useRightParen(TokenSyntax RightParen) {
  syntax_assert_token_is(RightParen, tok::r_paren, ")");
  CallLayout[cursorIndex(FunctionCallExprSyntax::Cursor::RightParen)]
    = RightParen.getRaw();
  return *this;
}

FunctionCallExprSyntax FunctionCallExprSyntaxBuilder::build() const {
  auto RawArgs = RawSyntax::make(SyntaxKind::FunctionCallArgumentList,
                                 ListLayout, SourcePresence::Present);
  auto RawCall = RawSyntax::make(SyntaxKind::FunctionCallExpr, CallLayout,
                                 SourcePresence::Present)
    ->replaceChild(FunctionCallExprSyntax::Cursor::ArgumentList, RawArgs);
  auto Data = SyntaxData::make(RawCall);
  return { Data, Data.get() };
}
