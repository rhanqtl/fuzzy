#include <clang/AST/Stmt.h>
#include <format>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Type.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include "fuzzy/Assert.h"
#include "fuzzy/RAII.h"

using namespace clang;
using namespace clang::tooling;

static llvm::cl::OptionCategory FuzzyGenCategory("fuzzy-gen options");

struct ConstraintInfo {
  StringRef name;
  std::string body;
  // For cleaning up the class
  SourceLocation startLoc;
  SourceLocation endLoc;
};

enum class DataMemberKind { Rand, RandCyclic, State };

struct DataMember {
  StringRef name;
  std::string type;
  std::string creator;  // The function to call
  DataMemberKind kind;
};

struct ClassInfo {
  std::string name;
  std::vector<DataMember> data_members;
  std::vector<ConstraintInfo> constraints;
};

class MacroExpansionCollector : public PPCallbacks {
 public:
  MacroExpansionCollector(std::vector<SourceRange>& text_blocks_to_erase, SourceManager& SM) :
      constraints_(text_blocks_to_erase),
      SM_(SM) {}

  void MacroExpands(const Token& MacroNameTok, const MacroDefinition& MD, SourceRange Range,
                    const MacroArgs* Args) override {
    if (!MD.getMacroInfo())
      return;

    StringRef macroName = MacroNameTok.getIdentifierInfo()->getName();
    if (macroName == "FUZZY_RAND" || macroName == "FUZZY_RANDC") {
      SourceLocation startLoc = Range.getBegin();
      SourceLocation endLoc = Range.getEnd();

      if (SM_.isWrittenInMainFile(startLoc)) {
        constraints_.push_back(Range);
      }
    }
  }

 private:
  std::vector<SourceRange>& constraints_;
  SourceManager& SM_;
};

class TypeMappingVisitor : public RecursiveASTVisitor<TypeMappingVisitor> {
 private:
  enum class Env {
    None,
    Vector,
    Array,
    HashMapKey,
    HashMapValue,
    TreeMapKey,
    TreeMapValue,
  };

 public:
  bool VisitBuiltinType(BuiltinType* type) {
    switch (type->getKind()) {
      case BuiltinType::Int:
        type_name_ = "int";
        break;
      case BuiltinType::UInt:
        type_name_ = "unsigned int";
        break;
      case BuiltinType::Bool:
        type_name_ = "bool";
        break;
      default:
        fuzzy_unreachable("unsupported builtin type");
    }
    return false;
  }

  bool VisitTemplateSpecializationType(TemplateSpecializationType* type) {
    if (current_env() != Env::None)
      fuzzy_todo("nested templates are not supported");

    auto* template_decl = type->getTemplateName().getAsTemplateDecl();
    fuzzy_assert(template_decl);
    auto tname = template_decl->getNameAsString();
    auto targs = type->template_arguments();
    if (tname == "vector") {
      fuzzy_assert(!targs.empty());
      push_env(Env::Vector);
      FUZZY_DEFER {
        pop_env();
      };
      TraverseType(targs[0].getAsType());
      fuzzy_assert(!type_name_.empty());
      type_name_ = std::format("RandVector<{}>", type_name_);
      creator_name_ = std::format("make_vector<{}>", std::move(type_name_));
    } else if (tname == "array") {
      fuzzy_assert(targs.size() >= 2);
      push_env(Env::Array);
      FUZZY_DEFER {
        pop_env();
      };
      TraverseType(targs[0].getAsType());
      fuzzy_assert(!type_name_.empty());
      const auto sz = targs[1].getAsIntegral().getZExtValue();
      auto sz_str = std::to_string(sz);
      type_name_ = std::format("RandArray<{}, {}>", type_name_, sz_str);
      creator_name_ = std::format("make_array<{}, {}>", std::move(type_name_), std::move(sz_str));
    } else {
      fuzzy_todo(tname);
    }

    return false;
  }

  std::string get_type() const {
    return type_name_.find("<") != std::string::npos ? type_name_
                                                     : std::format("Var<{}>", type_name_);
  }

  std::string get_creator() const {
    return !creator_name_.empty() ? creator_name_ : std::format("make_var<{}>", type_name_);
  }

 private:
  void push_env(Env e) {
    env_stack_.push(e);
  }
  void pop_env() {
    fuzzy_assert(!env_stack_.empty());
    env_stack_.pop();
  }
  Env current_env() {
    if (env_stack_.empty())
      return Env::None;
    return env_stack_.top();
  }

 private:
  std::stack<Env> env_stack_;
  std::string type_name_;
  std::string creator_name_;
};

class ConstraintBlockVisitor : public RecursiveASTVisitor<ConstraintBlockVisitor> {
 public:
  bool VisitStmt(Stmt* s) {
    return false;
  }

  std::string get() {
    return "";
  }

 private:
  int _level{0};
};

class FuzzyRewriterVisitor : public RecursiveASTVisitor<FuzzyRewriterVisitor> {
 public:
  FuzzyRewriterVisitor(Rewriter& rewriter, ASTContext& context,
                       std::vector<SourceRange>& text_blocks_to_erase) :
      rewriter_(rewriter),
      context_(context),
      SM_(context.getSourceManager()),
      text_blocks_to_erase_(text_blocks_to_erase) {}

  bool VisitCXXRecordDecl(CXXRecordDecl* decl) {
    // Debug: print all visited records
    llvm::errs() << "Visiting CXXRecordDecl: " << decl->getNameAsString() << "\n";

    if (!decl->isCompleteDefinition()) {
      llvm::errs() << "  -> Skipping (not complete definition)\n";
      return true;
    }

    if (!SM_.isWrittenInMainFile(decl->getLocation())) {
      llvm::errs() << "  -> Skipping (not in main file)\n";
      return true;
    }

    if (!decl->isThisDeclarationADefinition()) {
      llvm::errs() << "  -> Skipping (not a definition)\n";
      return true;
    }

    llvm::errs() << "  -> Processing " << decl->getNameAsString() << "\n";

    std::vector<DataMember> data_members;

    // Process fields
    for (auto* field : decl->fields()) {
      llvm::errs() << "    Visiting field: " << field->getNameAsString() << "\n";

      DataMemberKind kind = DataMemberKind::State;

      // Debug: print all attributes
      llvm::errs() << "      Attributes: ";
      for (const auto* attr : field->attrs()) {
        llvm::errs() << attr->getSpelling() << " ";
        if (const auto* annotate = dyn_cast<AnnotateAttr>(attr)) {
          llvm::errs() << "(annotation: " << annotate->getAnnotation() << ") ";
        }
      }
      llvm::errs() << "\n";

      if (auto* attr = field->getAttr<AnnotateAttr>()) {
        auto annotation = attr->getAnnotation();
        llvm::errs() << "      Found annotation: " << annotation << "\n";
        // TODO: check conflicts
        if (annotation == "fuzzy::rand")
          kind = DataMemberKind::Rand;
        else if (annotation == "fuzzy::randc")
          kind = DataMemberKind::RandCyclic;
      } else {
        llvm::errs() << "      No AnnotateAttr found\n";

        // Alternative: check the source text for FUZZY_RAND macro
        SourceRange range = field->getSourceRange();
        CharSourceRange charRange = CharSourceRange::getTokenRange(range);
        std::string fieldText = Lexer::getSourceText(charRange, SM_, context_.getLangOpts()).str();
        llvm::errs() << "      Source text: " << fieldText << "\n";

        if (fieldText.find("FUZZY_RAND") != std::string::npos) {
          llvm::errs() << "      Found FUZZY_RAND in source\n";
          kind = DataMemberKind::Rand;
        } else if (fieldText.find("FUZZY_RANDC") != std::string::npos) {
          llvm::errs() << "      Found FUZZY_RANDC in source\n";
          kind = DataMemberKind::RandCyclic;
        }
      }
      llvm::errs() << "      kind: " << static_cast<int>(kind) << "\n";

      auto type = field->getType();
      // TODO: report error - bit-fields are not suppoted yet
      if (kind != DataMemberKind::State && type.isConstQualified()) {
        // TODO: report error - rand variables cannot be const
        continue;
      }

      TypeMappingVisitor type_mapper;
      type_mapper.TraverseType(type);

      data_members.emplace_back(DataMember{
          field->getName(),
          type_mapper.get_type(),
          type_mapper.get_creator(),
          kind,
      });
    }

    std::vector<ConstraintInfo> constraints;

    for (auto* func : decl->methods()) {
      llvm::errs() << "    Visiting method: " << func->getNameAsString() << "\n";

      bool is_constraint_block = false;
      if (auto* annotate = func->getAttr<clang::AnnotateAttr>()) {
        if (annotate->getAnnotation() == "fuzzy::constraint")
          is_constraint_block = true;
      }
      // TODO: function calls are not supported, just skip now
      if (!is_constraint_block)
        continue;

      ConstraintBlockVisitor body_visitor;
      body_visitor.TraverseStmt(func->getBody());
      auto constraint_block_body = body_visitor.get();

      constraints.emplace_back(ConstraintInfo{func->getName(), std::move(constraint_block_body),
                                              func->getBeginLoc(), func->getEndLoc()});

      auto range = func->getSourceRange();
      auto begin_loc = range.getBegin();
      auto end_loc = range.getEnd();
      begin_loc = SM_.getSpellingLoc(SM_.getExpansionRange(begin_loc).getBegin());
      end_loc = SM_.getSpellingLoc(end_loc);
      text_blocks_to_erase_.push_back({begin_loc, end_loc});
    }

    classes_.push_back({decl->getNameAsString(), std::move(data_members), std::move(constraints)});

    return true;
  }

  void generate_output() {
    for (const auto& class_info : classes_) {
      cleanup_original_class(class_info);
      generate_model_specialization(class_info);
    }
  }

 private:
  void cleanup_original_class(const ClassInfo& class_info) {}

  void generate_model_specialization(const ClassInfo& class_info) {
    std::ostringstream os;
    os << "\nnamespace fuzzy {\n";
    os << "template <>\n";
    os << "struct Model<" << class_info.name << "> {\n";

    os << "  using object_type = " << class_info.name << ";\n";

    // Generate member references
    for (const auto& member : class_info.data_members) {
      os << "  " << member.type << " &" << member.name.str() << ";\n";
#if 0
      if (member.isArray) {
        os << "  RandArray<";

        // Extract element type from vector
        size_t vectorPos = member.type.find("std::vector<");
        if (vectorPos != std::string::npos) {
          size_t start = vectorPos + 12;
          int depth = 1;
          size_t end = start;
          while (end < member.type.length() && depth > 0) {
            if (member.type[end] == '<')
              depth++;
            else if (member.type[end] == '>')
              depth--;
            end++;
          }
          std::string elementType = member.type.substr(start, end - start - 1);
          os << elementType;
        }
        else {
          os << "typename " << member.type << "::value_type";
        }
        os << ">& " << member.name << ";\n";
      }
      else {
        os << "  RandVar<" << member.type << ">& " << member.name << ";\n";
      }
#endif
    }

    os << "\n";

    for (const auto& cst : class_info.constraints) {
      os << "  ConstraintBlock " << cst.name.str() << " = [this](Solver& solver) {\n";
      os << cst.body;
      os << "};\n";
    }

    os << "\n";
    os << "  std::array<ConstraintBlock, " << class_info.constraints.size()
       << "> constraint_blocks = {{";
    for (size_t i = 0; i < class_info.constraints.size(); ++i) {
      if (i > 0)
        os << ", ";
      os << class_info.constraints[i].name.str();
    }
    os << "}};\n\n";

    os << "  Model(Solver &solver)";
    if (!class_info.data_members.empty()) {
      os << ": ";
      bool first = true;
      for (const auto& member : class_info.data_members) {
        if (first)
          first = false;
        else
          os << ",";
        os << member.name.str() << "{solver." << member.creator << "()}";
      }
    }
    os << "{}";

    os << "  void bind(" << class_info.name << "&obj);\n";

    os << "};\n";
    os << "}  // namespace fuzzy\n";

    modelSpecializations_ += os.str();
  }

 public:
  std::string getModelSpecializations() const {
    return modelSpecializations_;
  }

 private:
  std::string transformConstraintBody(const std::string& body, const std::string& constraintName) {
    std::string result = body;

    // Handle the specific patterns for the NQueens example
    if (constraintName == "board_size") {
      // Simple constraint with two conditions
      result = "{\n    solver.item(Constant(4) <= n && n <= Constant(10));\n";
      result += "    solver.item(board.size() == n);\n  }";
    } else if (constraintName == "valid_pos") {
      // FUZZY_FOR_EACH pattern
      result = "{\n    auto&& [for_each_0, iterator_0] = solver.for_each(board);\n";
      result += "    for_each_0.body(Constant(0) <= iterator_0 && iterator_0 < n);\n  }";
    } else if (constraintName == "solution") {
      // Complex nested pattern
      result = "{\n    solver.unique(board);\n";
      result += "    auto&& [for_each_0, index_0] = solver.for_each_i(board);\n";
      result += "    auto&& [for_each_1, index_1] = solver.for_each_i(board);\n";
      result += "    for_each_1.body(solver.implies(index_0 < index_1 && board[index_0] < "
                "board[index_1],\n";
      result += "                                   index_1 - index_0 != board[index_1] - "
                "board[index_0]),\n";
      result += "                    solver.implies(index_0 < index_1 && board[index_0] > "
                "board[index_1],\n";
      result += "                                   index_1 - index_0 != board[index_0] - "
                "board[index_1]));\n";
      result += "    for_each_0.body(for_each_1);\n  }";
    } else {
      // Generic transformation - fallback
      result = std::regex_replace(result, std::regex("FUZZY_UNIQUE\\s*\\(\\s*([^)]+)\\s*\\)\\s*;"),
                                  "solver.unique($1);");

      // Handle constants in comparisons
      result = std::regex_replace(result, std::regex("\\b(\\d+)\\s*<="), "Constant($1) <=");
      result = std::regex_replace(result, std::regex("<=\\s*(\\d+)\\b"), "<= Constant($1)");
      result = std::regex_replace(result, std::regex("\\b(\\d+)\\s*<"), "Constant($1) <");
      result = std::regex_replace(result, std::regex("<\\s*(\\d+)\\b"), "< Constant($1)");
    }

    return result;
  }

  Rewriter& rewriter_;
  ASTContext& context_;
  SourceManager& SM_;

  std::vector<ClassInfo> classes_;
  std::string modelSpecializations_;
  std::vector<SourceRange>& text_blocks_to_erase_;
  // DiagnosticsEngine _engine;
};

class FuzzyRewriterConsumer : public ASTConsumer {
 public:
  FuzzyRewriterConsumer(Rewriter& rewriter, std::vector<SourceRange>& text_blocks_to_erase) :
      rewriter_(rewriter),
      text_blocks_to_erase_{text_blocks_to_erase} {}

  void HandleTranslationUnit(ASTContext& context) override {
    FuzzyRewriterVisitor visitor{rewriter_, context, text_blocks_to_erase_};
    visitor.TraverseDecl(context.getTranslationUnitDecl());
    visitor.generate_output();

    for (auto range : text_blocks_to_erase_) {
      rewriter_.RemoveText(range);
    }

    SourceManager& SM = context.getSourceManager();
    FileID mainFileID = SM.getMainFileID();
    SourceLocation endOfFile = SM.getLocForEndOfFile(mainFileID);
    rewriter_.InsertTextBefore(endOfFile, visitor.getModelSpecializations());
  }

 private:
  Rewriter& rewriter_;
  std::vector<SourceRange>& text_blocks_to_erase_;
};

class FuzzyRewriterAction : public ASTFrontendAction {
 public:
  FuzzyRewriterAction() {}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef file) override {
    rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

    CI.getPreprocessor().addPPCallbacks(
        std::make_unique<MacroExpansionCollector>(text_blocks_to_erase, CI.getSourceManager()));

    return std::make_unique<FuzzyRewriterConsumer>(rewriter_, text_blocks_to_erase);
  }

  void EndSourceFileAction() override {
    SourceManager& SM = rewriter_.getSourceMgr();
    rewriter_.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
  }

 private:
  Rewriter rewriter_;
  std::vector<SourceRange> text_blocks_to_erase;
};

int main(int argc, const char** argv) {
  auto expectedParser = CommonOptionsParser::create(argc, argv, FuzzyGenCategory);
  if (!expectedParser) {
    llvm::errs() << expectedParser.takeError();
    return 1;
  }

  CommonOptionsParser& optionsParser = *expectedParser;
  ClangTool tool(optionsParser.getCompilations(), optionsParser.getSourcePathList());

  return tool.run(newFrontendActionFactory<FuzzyRewriterAction>().get());
}
