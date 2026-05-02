#include <memory>
#include <set>
#include <sstream>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

static llvm::cl::OptionCategory MirrorClassCategory("Mirror Class Generator Options");

class MirrorClassGenerator : public MatchFinder::MatchCallback {
 public:
  MirrorClassGenerator(Rewriter& Rewrite, SourceManager& SM) :
      Rewrite(Rewrite),
      SM{SM} {}

  void run(const MatchFinder::MatchResult& Result) override {
    ASTContext* Context = Result.Context;
    const SourceManager& SM = Context->getSourceManager();

    // 处理带标注的声明
    if (const Decl* D = Result.Nodes.getNodeAs<Decl>("annotatedDecl")) {
      if (!SM.isWrittenInMainFile(D->getLocation()))
        return;

      // 收集标注信息
      std::vector<std::string> annotations;
      for (const Attr* A : D->attrs()) {
        if (const auto* Annotate = dyn_cast<AnnotateAttr>(A)) {
          StringRef annotation = Annotate->getAnnotation();
          if (annotation == "fuzzy::rand" || annotation == "fuzzy::randc" ||
              annotation == "fuzzy::constraint") {
            annotations.push_back(annotation.str());
          }
        }
      }

      if (annotations.empty())
        return;

      FullSourceLoc Loc = Context->getFullLoc(D->getBeginLoc());
      if (Loc.isInvalid())
        return;

      // 输出检测信息
      llvm::outs() << "Found annotated declaration: " << SM.getFilename(Loc) << ":"
                   << Loc.getSpellingLineNumber() << ":" << Loc.getSpellingColumnNumber() << "\n";

      if (const NamedDecl* ND = dyn_cast<NamedDecl>(D)) {
        llvm::outs() << "Name: " << ND->getNameAsString() << "\n";
      }

      llvm::outs() << "Kind: " << D->getDeclKindName() << "\n";
      llvm::outs() << "Annotations: [";
      for (size_t i = 0; i < annotations.size(); ++i) {
        if (i > 0)
          llvm::outs() << ", ";
        llvm::outs() << annotations[i];
      }
      llvm::outs() << "]\n";
      llvm::outs() << "----------------------------\n";
    }

    // 处理包含随机成员的结构体/类
    if (const CXXRecordDecl* Record = Result.Nodes.getNodeAs<CXXRecordDecl>("randRecord")) {
      if (!SM.isWrittenInMainFile(Record->getLocation()))
        return;

      llvm::outs() << "Processing struct/class with random members: " << Record->getNameAsString()
                   << "\n";

      // 收集所有约束宏
      ConstraintMacros constraintMacros;
      for (const Decl* D : Record->decls()) {
        if (const FunctionDecl* FD = dyn_cast<FunctionDecl>(D)) {
          if (FD->getNameAsString().ends_with("_thunk")) {
            // 检查是否有对应的约束块
            std::string constraintName =
                FD->getNameAsString().substr(1, FD->getNameAsString().size() - 7);
            constraintMacros[constraintName] = FD;
          }
        }
      }

      // 检查是否是模板类
      bool isTemplate = Record->getDescribedClassTemplate() != nullptr;

      // 生成镜像类
      std::string mirrorClass = generateMirrorClass(Record, SM, Context, constraintMacros);

      // 在原始类定义后插入镜像类
      SourceLocation EndLoc = Record->getEndLoc();
      if (EndLoc.isValid()) {
        Rewrite.InsertTextAfterToken(EndLoc, "\n\n" + mirrorClass);
        llvm::outs() << "Generated mirror class for: " << Record->getNameAsString() << "\n";
      }
    }
  }

 private:
  Rewriter& Rewrite;
  SourceManager& SM;

  // 存储约束宏信息：约束名 -> thunk函数声明
  using ConstraintMacros = std::map<std::string, const FunctionDecl*>;

  // 为包含随机成员的类生成镜像类
  std::string generateMirrorClass(const CXXRecordDecl* Record, const SourceManager& SM,
                                  ASTContext* Context, const ConstraintMacros& constraintMacros) {
    std::ostringstream OS;

    // 检查是否是模板类
    const ClassTemplateDecl* TemplateDecl = Record->getDescribedClassTemplate();
    bool isTemplate = TemplateDecl != nullptr;

    // 生成模板头（如果是模板类）
    if (isTemplate) {
      OS << "template<";
      const TemplateParameterList* Params = TemplateDecl->getTemplateParameters();
      for (unsigned i = 0; i < Params->size(); ++i) {
        if (i > 0)
          OS << ", ";

        if (const auto* TTP = dyn_cast<TemplateTypeParmDecl>(Params->getParam(i))) {
          OS << "typename " << TTP->getNameAsString();
          if (TTP->hasDefaultArgument()) {
            SourceRange Range = TTP->getDefaultArgument().getSourceRange();
            StringRef DefaultText = Lexer::getSourceText(CharSourceRange::getTokenRange(Range), SM,
                                                         Context->getLangOpts());
            OS << " = " << DefaultText.str();
          }
        }
        else if (const auto* NTTP = dyn_cast<NonTypeTemplateParmDecl>(Params->getParam(i))) {
          OS << NTTP->getType().getAsString() << " " << NTTP->getNameAsString();
          if (NTTP->hasDefaultArgument() && NTTP->getDefaultArgument().getArgument().getAsExpr()) {
            OS << " = "
               << getSourceText(NTTP->getDefaultArgument().getArgument().getAsExpr(), SM, Context);
          }
        }
        else if (const auto* TTP = dyn_cast<TemplateTemplateParmDecl>(Params->getParam(i))) {
          OS << "template <typename> typename " << TTP->getNameAsString();
          // 模板模板参数不支持默认值
        }
      }
      OS << ">\n";
    }

    // 生成镜像类定义
    OS << "struct RandModel<" << Record->getNameAsString();

    // 如果是模板类，添加模板参数
    if (isTemplate) {
      OS << "<";
      const TemplateParameterList* Params = TemplateDecl->getTemplateParameters();
      for (unsigned i = 0; i < Params->size(); ++i) {
        if (i > 0)
          OS << ", ";
        OS << Params->getParam(i)->getNameAsString();
      }
      OS << ">";
    }

    OS << "> {\n";

    // 生成成员变量
    for (const FieldDecl* Field : Record->fields()) {
      QualType Type = Field->getType();
      std::string TypeStr = Type.getAsString();
      std::string Name = Field->getNameAsString();

      // 检查成员是否有模糊标注
      bool isRand = false;
      for (const Attr* A : Field->attrs()) {
        if (const auto* Annotate = dyn_cast<AnnotateAttr>(A)) {
          StringRef annotation = Annotate->getAnnotation();
          if (annotation == "fuzzy::rand" || annotation == "fuzzy::randc") {
            isRand = true;
            break;
          }
        }
      }

      if (isRand) {
        OS << "  fuzzy::RandVar<" << TypeStr << "> &" << Name << ";\n";
      }
      else {
        OS << "  fuzzy::Constant<" << TypeStr << "> &" << Name << ";\n";
      }
    }

    // 生成构造函数
    OS << "\n";
    OS << "  RandModel(" << Record->getNameAsString();

    // 如果是模板类，添加模板参数
    if (isTemplate) {
      OS << "<";
      const TemplateParameterList* Params = TemplateDecl->getTemplateParameters();
      for (unsigned i = 0; i < Params->size(); ++i) {
        if (i > 0)
          OS << ", ";
        OS << Params->getParam(i)->getNameAsString();
      }
      OS << ">";
    }

    OS << " &obj)\n";
    OS << "    : ";

    bool first = true;
    for (const FieldDecl* Field : Record->fields()) {
      std::string Name = Field->getNameAsString();

      if (!first)
        OS << ",\n      ";
      OS << Name << "(obj." << Name << ")";
      first = false;
    }

    OS << " {}\n";

    // 生成约束宏
    for (const auto& [constraintName, thunkFunc] : constraintMacros) {
      // 获取函数体源码
      SourceRange BodyRange = thunkFunc->getBody()->getSourceRange();
      StringRef BodyText = Lexer::getSourceText(CharSourceRange::getTokenRange(BodyRange), SM,
                                                Context->getLangOpts());

      // 生成约束块成员
      OS << "\n";
      OS << "  fuzzy::ConstraintBlock " << constraintName << " = [this]() { _" << constraintName
         << "_thunk(); };\n";

      // 生成thunk函数
      OS << "  void _" << constraintName << "_thunk() " << BodyText.str() << "\n";
    }

    OS << "};\n";

    return OS.str();
  }

  // 辅助函数：获取表达式的源码文本
  std::string getSourceText(const Expr* E, const SourceManager& SM, ASTContext* Context) {
    SourceRange Range = E->getSourceRange();
    return Lexer::getSourceText(CharSourceRange::getTokenRange(Range), SM, Context->getLangOpts())
        .str();
  }
};

class MirrorClassAction : public ASTFrontendAction {
 public:
  MirrorClassAction() {}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef File) override {
    // 为当前文件初始化 Rewriter
    TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

    // 创建匹配器
    Finder = std::make_unique<MatchFinder>();

    // 创建生成器并传入 Rewriter 和 SourceManager
    Generator = std::make_unique<MirrorClassGenerator>(TheRewriter, CI.getSourceManager());

    // 添加匹配器
    Finder->addMatcher(
        decl(anyOf(hasAttr(attr::Annotate), unless(isImplicit()))).bind("annotatedDecl"),
        Generator.get());

    Finder->addMatcher(cxxRecordDecl(isDefinition(), has(fieldDecl(hasAttr(attr::Annotate))),
                                     unless(isImplicit()))
                           .bind("randRecord"),
                       Generator.get());

    return Finder->newASTConsumer();
  }

  void EndSourceFileAction() override {
    // 输出修改后的文件
    const SourceManager& SM = TheRewriter.getSourceMgr();
    llvm::errs() << "Rewritten file: "
                 << SM.getFileEntryForID(SM.getMainFileID())->tryGetRealPathName() << "\n";
    TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
  }

 private:
  Rewriter TheRewriter;
  std::unique_ptr<MatchFinder> Finder;
  std::unique_ptr<MirrorClassGenerator> Generator;
};

int main(int argc, const char** argv) {
  auto ExpectedParser = CommonOptionsParser::create(argc, argv, MirrorClassCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }

  CommonOptionsParser& OptionsParser = *ExpectedParser;
  ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
  return Tool.run(newFrontendActionFactory<MirrorClassAction>().get());
}
