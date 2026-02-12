#include <clang-tidy/ClangTidyCheck.h>
#include <clang-tidy/ClangTidyModule.h>
#include <clang-tidy/ClangTidyModuleRegistry.h>
#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>

using namespace clang;
using namespace clang::tidy;
using namespace clang::ast_matchers;

// Example check: SDL_Init replacement
class SDL3InitCheck : public ClangTidyCheck {
public:
  SDL3InitCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    // Match calls to SDL_Init
    Finder->addMatcher(
        callExpr(callee(functionDecl(hasName("SDL_Init")))).bind("init_call"),
        this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    const auto *MatchedCall = Result.Nodes.getNodeAs<CallExpr>("init_call");
    if (!MatchedCall)
      return;

    // Issue a diagnostic
    diag(MatchedCall->getBeginLoc(),
         "SDL_Init has changed in SDL3, consider using SDL3_Init")
        << FixItHint::CreateReplacement(
               MatchedCall->getCallee()->getSourceRange(), "SDL3_Init");
  }
};

// The module that contains all SDL3 migration checks
class SDL3MigrationModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<SDL3InitCheck>("sdl3-migration-init");
    // Add more checks here as needed:
    // CheckFactories.registerCheck<SDL3WindowCheck>("sdl3-migration-window");
    // CheckFactories.registerCheck<SDL3RendererCheck>("sdl3-migration-renderer");
  }
};

// Register the module
namespace clang {
namespace tidy {

// This is the plugin registration function that clang-tidy will look for
static ClangTidyModuleRegistry::Add<SDL3MigrationModule>
    X("sdl3-migration-module", "Adds SDL3 migration checks.");

// This extern "C" function is required for the plugin to be loadable
volatile int SDL3MigrationModuleAnchorSource = 0;

} // namespace tidy
} // namespace clang

extern "C" {
// This is what makes it a loadable plugin
ClangTidyModule *createClangTidyModule() {
  return new SDL3MigrationModule();
}
}
