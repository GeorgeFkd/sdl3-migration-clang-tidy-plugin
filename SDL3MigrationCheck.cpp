#include <clang-tidy/ClangTidyCheck.h>
#include <clang-tidy/ClangTidyModule.h>
#include <clang-tidy/ClangTidyModuleRegistry.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/OperationKinds.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <memory>

using namespace clang;
using namespace clang::tidy;
using namespace clang::ast_matchers;

static auto FnCallMatcher(StringRef FunctionName, StringRef BindName) {
  return callExpr(callee(implicitCastExpr(
                      hasCastKind(CK_FunctionToPointerDecay),
                      hasSourceExpression(declRefExpr(
                          to(functionDecl(hasName(FunctionName))))))))
      .bind(BindName);
}

class SDLIncludeCallback : public PPCallbacks {
public:
  SDLIncludeCallback(ClangTidyCheck &Check, const SourceManager &SM)
      : Check(Check), SM(SM) {};
  void InclusionDirective(SourceLocation HashLoc, const Token &IncludeTok,
                          StringRef FileName, bool isAngled,
                          CharSourceRange FilenameRange,
                          OptionalFileEntryRef file, StringRef SearchPath,
                          StringRef RelativePath, const Module *imported,
                          bool ModuleImported,
                          SrcMgr::CharacteristicKind FileType) override {
    if (FileName.contains("SDL") ||
        FileName.contains("SDL2") && !FileName.contains("SDL3")) {
      FileID fid = SM.getFileID(HashLoc);
      const FileEntry *FEntry = SM.getFileEntryForID(fid);
      if (FEntry) {
        StringRef OriginFileName = FEntry->tryGetRealPathName();
        bool InSDL2File = OriginFileName.contains("SDL2");
        bool InSDLFile = OriginFileName.contains("SDL");
        if (!InSDL2File && !InSDLFile) {
          std::string Replacement;
          if (FileName == "SDL2/SDL.h") {
            Replacement = "SDL3/SDL.h";
          } else if (FileName.starts_with("SDL2/")) {
            Replacement = "SDL3/" + FileName.substr(5).str();
          } else if (FileName == "SDL.h") {
            Replacement = "SDL3/SDL.h";
          } else if (FileName.starts_with("SDL_")) {
            Replacement = "SDL3/" + FileName.str();
          }

          if (!Replacement.empty()) {
            std::string FormattedReplacement =
                isAngled ? ("<" + Replacement + ">")
                         : ("\"" + Replacement + "\"");

            Check.diag(HashLoc, "replace with '%0'", DiagnosticIDs::Note)
                << Replacement
                << FixItHint::CreateReplacement(FilenameRange,
                                                FormattedReplacement);
          }
        }
      }
    }
  }

private:
  ClangTidyCheck &Check;
  const SourceManager &SM;
};
static const char *FunctionRenames[][3] = {
    // Byte swap migrations
    {"SDL_SwapBE16", "SDL_Swap16BE", ""},
    {"SDL_SwapBE32", "SDL_Swap32BE", ""},
    {"SDL_SwapBE64", "SDL_Swap64BE", ""},
    {"SDL_SwapLE16", "SDL_Swap16LE", ""},
    {"SDL_SwapLE32", "SDL_Swap32LE", ""},
    {"SDL_SwapLE64", "SDL_Swap64LE", ""},

    {"SDL_GetCPUCount", "SDL_GetNumLogicalCPUCores", ""},
    {"SDL_SIMDGetAlignment", "SDL_GetSIMDAlignment", ""},

};

static const char *RemovedFunctions[][2] = {
    {"SDL_GetNumAudioDevices", ""},
    {"SDL_GetAudioDeviceSpec", ""},
    {"SDL_ConvertAudio", ""},
    {"SDL_BuildAudioCVT", ""},
    {"SDL_OpenAudio", ""},
    {"SDL_CloseAudio", ""},
    {"SDL_PauseAudio", ""},
    {"SDL_GetAudioStatus", ""},
    {"SDL_GetAudioDeviceStatus", ""},
    {"SDL_GetDefaultAudioInfo", ""},
    {"SDL_LockAudio", ""},
    {"SDL_LockAudioDevice", ""},
    {"SDL_UnlockAudio", ""},
    {"SDL_UnlockAudioDevice", ""},
    {"SDL_MixAudio", ""},
    {"SDL_QueueAudio", ""},
    {"SDL_DequeueAudio", ""},
    {"SDL_ClearAudioQueue", ""},
    {"SDL_GetQueuedAudioSize", ""},
};

static const char *SymbolMigrations[][3] = {
    {"AUDIO_F32", "SDL_AUDIO_F32LE", ""},
    {"AUDIO_F32LSB", "SDL_AUDIO_F32LE", ""},
    {"AUDIO_F32MSB", "SDL_AUDIO_F32BE", ""},
    {"AUDIO_F32SYS", "SDL_AUDIO_F32", ""},
    {"AUDIO_S16", "SDL_AUDIO_S16LE", ""},
    {"AUDIO_S16LSB", "SDL_AUDIO_S16LE", ""},
    {"AUDIO_S16MSB", "SDL_AUDIO_S16BE", ""},
    {"AUDIO_S16SYS", "SDL_AUDIO_S16", ""},
    {"AUDIO_S32", "SDL_AUDIO_S32LE", ""},
    {"AUDIO_S32LSB", "SDL_AUDIO_S32LE", ""},
    {"AUDIO_S32MSB", "SDL_AUDIO_S32BE", ""},
    {"AUDIO_S32SYS", "SDL_AUDIO_S32", ""},
    {"AUDIO_S8", "SDL_AUDIO_S8", ""},
    {"AUDIO_U8", "SDL_AUDIO_U8", ""},

    // Event symbols
    {"SDL_APP_DIDENTERBACKGROUND", "SDL_EVENT_DID_ENTER_BACKGROUND", ""},
    {"SDL_APP_DIDENTERFOREGROUND", "SDL_EVENT_DID_ENTER_FOREGROUND", ""},
    {"SDL_APP_LOWMEMORY", "SDL_EVENT_LOW_MEMORY", ""},
    {"SDL_APP_TERMINATING", "SDL_EVENT_TERMINATING", ""},
    {"SDL_APP_WILLENTERBACKGROUND", "SDL_EVENT_WILL_ENTER_BACKGROUND", ""},
    {"SDL_APP_WILLENTERFOREGROUND", "SDL_EVENT_WILL_ENTER_FOREGROUND", ""},
    {"SDL_AUDIODEVICEADDED", "SDL_EVENT_AUDIO_DEVICE_ADDED", ""},
    {"SDL_AUDIODEVICEREMOVED", "SDL_EVENT_AUDIO_DEVICE_REMOVED", ""},
    {"SDL_CLIPBOARDUPDATE", "SDL_EVENT_CLIPBOARD_UPDATE", ""},
    {"SDL_CONTROLLERAXISMOTION", "SDL_EVENT_GAMEPAD_AXIS_MOTION", ""},
    {"SDL_CONTROLLERBUTTONDOWN", "SDL_EVENT_GAMEPAD_BUTTON_DOWN", ""},
    {"SDL_CONTROLLERBUTTONUP", "SDL_EVENT_GAMEPAD_BUTTON_UP", ""},
    {"SDL_CONTROLLERDEVICEADDED", "SDL_EVENT_GAMEPAD_ADDED", ""},
    {"SDL_CONTROLLERDEVICEREMAPPED", "SDL_EVENT_GAMEPAD_REMAPPED", ""},
    {"SDL_CONTROLLERDEVICEREMOVED", "SDL_EVENT_GAMEPAD_REMOVED", ""},
    {"SDL_CONTROLLERSENSORUPDATE", "SDL_EVENT_GAMEPAD_SENSOR_UPDATE", ""},
    {"SDL_CONTROLLERSTEAMHANDLEUPDATED",
     "SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED", ""},
    {"SDL_CONTROLLERTOUCHPADDOWN", "SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN", ""},
    {"SDL_CONTROLLERTOUCHPADMOTION", "SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION", ""},
    {"SDL_CONTROLLERTOUCHPADUP", "SDL_EVENT_GAMEPAD_TOUCHPAD_UP", ""},
    {"SDL_DROPBEGIN", "SDL_EVENT_DROP_BEGIN", ""},
    {"SDL_DROPCOMPLETE", "SDL_EVENT_DROP_COMPLETE", ""},
    {"SDL_DROPFILE", "SDL_EVENT_DROP_FILE", ""},
    {"SDL_DROPTEXT", "SDL_EVENT_DROP_TEXT", ""},
    {"SDL_FINGERDOWN", "SDL_EVENT_FINGER_DOWN", ""},
    {"SDL_FINGERMOTION", "SDL_EVENT_FINGER_MOTION", ""},
    {"SDL_FINGERUP", "SDL_EVENT_FINGER_UP", ""},
    {"SDL_FIRSTEVENT", "SDL_EVENT_FIRST", ""},
    {"SDL_JOYAXISMOTION", "SDL_EVENT_JOYSTICK_AXIS_MOTION", ""},
    {"SDL_JOYBALLMOTION", "SDL_EVENT_JOYSTICK_BALL_MOTION", ""},
    {"SDL_JOYBATTERYUPDATED", "SDL_EVENT_JOYSTICK_BATTERY_UPDATED", ""},
    {"SDL_JOYBUTTONDOWN", "SDL_EVENT_JOYSTICK_BUTTON_DOWN", ""},
    {"SDL_JOYBUTTONUP", "SDL_EVENT_JOYSTICK_BUTTON_UP", ""},
    {"SDL_JOYDEVICEADDED", "SDL_EVENT_JOYSTICK_ADDED", ""},
    {"SDL_JOYDEVICEREMOVED", "SDL_EVENT_JOYSTICK_REMOVED", ""},
    {"SDL_JOYHATMOTION", "SDL_EVENT_JOYSTICK_HAT_MOTION", ""},
    {"SDL_KEYDOWN", "SDL_EVENT_KEY_DOWN", ""},
    {"SDL_KEYMAPCHANGED", "SDL_EVENT_KEYMAP_CHANGED", ""},
    {"SDL_KEYUP", "SDL_EVENT_KEY_UP", ""},
    {"SDL_LASTEVENT", "SDL_EVENT_LAST", ""},
    {"SDL_LOCALECHANGED", "SDL_EVENT_LOCALE_CHANGED", ""},
    {"SDL_MOUSEBUTTONDOWN", "SDL_EVENT_MOUSE_BUTTON_DOWN", ""},
    {"SDL_MOUSEBUTTONUP", "SDL_EVENT_MOUSE_BUTTON_UP", ""},
    {"SDL_MOUSEMOTION", "SDL_EVENT_MOUSE_MOTION", ""},
    {"SDL_MOUSEWHEEL", "SDL_EVENT_MOUSE_WHEEL", ""},
    {"SDL_POLLSENTINEL", "SDL_EVENT_POLL_SENTINEL", ""},
    {"SDL_QUIT", "SDL_EVENT_QUIT", ""},
    {"SDL_RENDER_DEVICE_RESET", "SDL_EVENT_RENDER_DEVICE_RESET", ""},
    {"SDL_RENDER_TARGETS_RESET", "SDL_EVENT_RENDER_TARGETS_RESET", ""},
    {"SDL_SENSORUPDATE", "SDL_EVENT_SENSOR_UPDATE", ""},
    {"SDL_TEXTEDITING", "SDL_EVENT_TEXT_EDITING", ""},
    {"SDL_TEXTEDITING_EXT", "SDL_EVENT_TEXT_EDITING_EXT", ""},
    {"SDL_TEXTINPUT", "SDL_EVENT_TEXT_INPUT", ""},
    {"SDL_USEREVENT", "SDL_EVENT_USER", ""},
};

class SDL3AtomicCheck : public ClangTidyCheck {
public:
  SDL3AtomicCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  const char *FuncRenames[10][3] = {
      {"SDL_AtomicAdd", "SDL_AddAtomicInt", ""},
      {"SDL_AtomicCAS", "SDL_CompareAndSwapAtomicInt", ""},
      {"SDL_AtomicCASPtr", "SDL_CompareAndSwapAtomicPointer", ""},
      {"SDL_AtomicGet", "SDL_GetAtomicInt", ""},
      {"SDL_AtomicGetPtr", "SDL_GetAtomicPointer", ""},
      {"SDL_AtomicLock", "SDL_LockSpinlock", ""},
      {"SDL_AtomicSet", "SDL_SetAtomicInt", ""},
      {"SDL_AtomicSetPtr", "SDL_SetAtomicPointer", ""},
      {"SDL_AtomicTryLock", "SDL_TryLockSpinlock", ""},
      {"SDL_AtomicUnlock", "SDL_UnlockSpinlock", ""}};

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {

    // TODO: this probably does not work
    for (const auto &Migration : FuncRenames) {
      Finder->addMatcher(callExpr(callee(functionDecl(hasName(Migration[0]))))
                             .bind(Migration[0]),
                         this);
    }
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &Migration : FuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Migration[0])) {
        std::string Message = std::string(Migration[0]) + "() is now " +
                              Migration[1] + "() in SDL3";

        if (strlen(Migration[2]) > 0) {
          Message += " (" + std::string(Migration[2]) + ")";
        }

        diag(Call->getBeginLoc(), Message) << FixItHint::CreateReplacement(
            Call->getCallee()->getSourceRange(), Migration[1]);
        return;
      }
    }
  }
};

class SDL3AudioCheck : public ClangTidyCheck {
public:
  SDL3AudioCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  const char *FuncRenames[9][3] = {
      {"SDL_AudioStreamAvailable", "SDL_GetAudioStreamAvailable", ""},
      {"SDL_AudioStreamClear", "SDL_ClearAudioStream", "now returns bool"},
      {"SDL_AudioStreamFlush", "SDL_FlushAudioStream", "now returns bool"},
      {"SDL_AudioStreamGet", "SDL_GetAudioStreamData", ""},
      {"SDL_AudioStreamPut", "SDL_PutAudioStreamData", "now returns bool"},
      {"SDL_FreeAudioStream", "SDL_DestroyAudioStream", ""},
      {"SDL_LoadWAV_RW", "SDL_LoadWAV_IO", "now returns bool"},
      {"SDL_MixAudioFormat", "SDL_MixAudio", "now returns bool"},
      {"SDL_NewAudioStream", "SDL_CreateAudioStream", ""},
  };

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    llvm::outs() << "Registering matchers\n";
    Finder->addMatcher(
        callExpr(callee(implicitCastExpr(
                     hasCastKind(CK_FunctionToPointerDecay),
                     hasSourceExpression(declRefExpr(
                         to(functionDecl(hasName("SDL_AudioInit"))))))))
            .bind("sdl_audio_init"),
        this);

    Finder->addMatcher(FnCallMatcher("SDL_FreeWav", "sdl_free_wav"), this);

    Finder->addMatcher(
        callExpr(callee(implicitCastExpr(
                     hasCastKind(CK_FunctionToPointerDecay),
                     hasSourceExpression(declRefExpr(
                         to(functionDecl(hasName("SDL_AudioQuit"))))))))
            .bind("sdl_audio_quit"),
        this);
    Finder->addMatcher(
        callExpr(callee(implicitCastExpr(
                     hasCastKind(CK_FunctionToPointerDecay),
                     hasSourceExpression(declRefExpr(to(
                         functionDecl(hasName("SDL_GetNumAudioDevices"))))))),
                 hasArgument(0, integerLiteral().bind("device_type")))
            .bind("get_num_audio_devices"),
        this);
    Finder->addMatcher(
        callExpr(callee(implicitCastExpr(
                     hasCastKind(CK_FunctionToPointerDecay),
                     hasSourceExpression(declRefExpr(
                         to(functionDecl(hasName("SDL_PauseAudioDevice"))))))),
                 hasArgument(0, expr().bind("device_arg")),
                 hasArgument(1, integerLiteral().bind("pause_value")))
            .bind("sdl_pause_audio_device"),
        this);
    Finder->addMatcher(
        callExpr(callee(implicitCastExpr(
                     hasCastKind(CK_FunctionToPointerDecay),
                     hasSourceExpression(declRefExpr(to(
                         functionDecl(hasName("SDL_GetAudioDeviceStatus"))))))),
                 hasArgument(0, expr().bind("device_arg")))
            .bind("sdl_get_audio_device_status"),
        this);

    // TODO: this probs does not work
    for (const auto &Migration : FuncRenames) {
      Finder->addMatcher(callExpr(callee(functionDecl(hasName(Migration[0]))))
                             .bind(Migration[0]),
                         this);
    }
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("sdl_audio_init")) {
      const auto *Callee = Call->getCallee();

      diag(Call->getBeginLoc(),
           "SDL_AudioInit() has been removed in SDL3. "
           "Use SDL_InitSubSystem(SDL_INIT_AUDIO) instead, which properly "
           "refcounts the subsystem. "
           "To choose a specific audio driver, use the SDL_AUDIO_DRIVER hint")
          << FixItHint::CreateReplacement(Call->getSourceRange(),
                                          "SDL_InitSubSystem(SDL_INIT_AUDIO)");
    }

    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("sdl_audio_quit")) {
      const auto *Callee = Call->getCallee();

      diag(Call->getBeginLoc(),
           "SDL_AudioQuit() has been removed in SDL3. "
           "Use SDL_QuitSubSystem(SDL_INIT_AUDIO) instead, which properly "
           "refcounts the subsystem")
          << FixItHint::CreateReplacement(Call->getSourceRange(),
                                          "SDL_QuitSubSystem(SDL_INIT_AUDIO)");
    }

    if (const auto *Call =
            Result.Nodes.getNodeAs<CallExpr>("get_num_audio_devices")) {
      const auto *DeviceType =
          Result.Nodes.getNodeAs<IntegerLiteral>("device_type");

      if (DeviceType) {
        llvm::APInt value = DeviceType->getValue();
        std::string replacement;

        if (value == 0) {
          replacement = "SDL_GetAudioPlaybackDevices";
        } else {
          replacement = "SDL_GetAudioRecordingDevices";
        }

        diag(Call->getBeginLoc(),
             "SDL_GetNumAudioDevices() has been removed in SDL3."
             "Use %0(&num_devices) which returns an array of device IDs "
             "instead of a count.\n"
             "official docs:\n"
             "int i,num_devices;\n"
             "SDL_AudioDeviceID *devices = "
             "SDL_GetAudioPlaybackDevices(&num_devices);\n"
             "if (devices) {\n"
             "\tfor (i = 0; i < num_devices; ++i) {\n"
             "\t\tSDL_AudioDeviceID instance_id = devices[i];\n"
             "\t\tSDL_Log('AudioDevice $' SDL_PRIu32 : $s', "
             "instance_id,SDL_GetAudioDeviceName(instance_id));\n"
             "\t}\n"
             "\tSDL_free(devices);\n"
             "}")
            << replacement;
      }
    }

    if (const auto *Call =
            Result.Nodes.getNodeAs<CallExpr>("sdl_pause_audio_device")) {
      const auto *PauseValue =
          Result.Nodes.getNodeAs<IntegerLiteral>("pause_value");
      const auto *DeviceArg = Result.Nodes.getNodeAs<Expr>("device_arg");

      if (PauseValue && DeviceArg) {
        llvm::APInt value = PauseValue->getValue();

        std::string DeviceText =
            Lexer::getSourceText(
                CharSourceRange::getTokenRange(DeviceArg->getSourceRange()),
                *Result.SourceManager, Result.Context->getLangOpts())
                .str();

        std::string replacement;
        if (value == 0) {
          // pause_on = 0 means unpause/resume
          replacement = "SDL_ResumeAudioDevice(" + DeviceText + ")";
          diag(Call->getBeginLoc(),
               "SDL_PauseAudioDevice() no longer takes a second argument. "
               "To unpause use SDL_ResumeAudioDevice()")
              << FixItHint::CreateReplacement(Call->getSourceRange(),
                                              replacement);
        } else {
          // pause_on = non-zero means pause
          replacement = "SDL_PauseAudioDevice(" + DeviceText + ")";
          diag(Call->getBeginLoc(),
               "SDL_PauseAudioDevice() no longer takes a second argument. "
               "To pause use SDL_PauseAudioDevice() "
               "with one argument")
              << FixItHint::CreateReplacement(Call->getSourceRange(),
                                              replacement);
        }
      }
    }

    if (const auto *Call =
            Result.Nodes.getNodeAs<CallExpr>("sdl_get_audio_device_status")) {
      const auto *DeviceArg = Result.Nodes.getNodeAs<Expr>("device_arg");

      if (DeviceArg) {
        std::string DeviceText =
            Lexer::getSourceText(
                CharSourceRange::getTokenRange(DeviceArg->getSourceRange()),
                *Result.SourceManager, Result.Context->getLangOpts())
                .str();

        std::string replacement = "SDL_AudioDevicePaused(" + DeviceText + ")";

        diag(Call->getBeginLoc(),
             "SDL_GetAudioDeviceStatus() has been removed in SDL3. "
             "Use SDL_AudioDevicePaused() instead. "
             "Now it returns bool (true if device is valid and paused) instead "
             "of "
             "SDL_AudioStatus enum")
            << FixItHint::CreateReplacement(Call->getSourceRange(),
                                            replacement);
      }
    }
    for (const auto &Migration : FuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Migration[0])) {
        std::string Message = std::string(Migration[0]) + "() is now " +
                              Migration[1] + "() in SDL3";

        if (strlen(Migration[2]) > 0) {
          Message += " (" + std::string(Migration[2]) + ")";
        }

        diag(Call->getBeginLoc(), Message) << FixItHint::CreateReplacement(
            Call->getCallee()->getSourceRange(), Migration[1]);
        return;
      }
    }

    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("sdl_free_wav")) {
      const auto *Callee = Call->getCallee();

      diag(Call->getBeginLoc(), "SDL_FreeWAV has been removed and calls can be "
                                "replaced with SDL_free.")
          << FixItHint::CreateReplacement(Callee->getSourceRange(), "SDL_free");
    }
  }
};

class SDL3InitCheck : public ClangTidyCheck {
public:
  SDL3InitCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {

    Finder->addMatcher(
        ifStmt(hasCondition(binaryOperator(
                   hasOperatorName("=="),
                   hasLHS(callExpr(callee(implicitCastExpr(
                       hasCastKind(CK_FunctionToPointerDecay),
                       hasSourceExpression(declRefExpr(
                           to(functionDecl(matchesName("SDL_[A-Z].*"))))))))),
                   hasRHS(unaryOperator(
                       hasOperatorName("-"),
                       hasUnaryOperand(integerLiteral(equals(1))))))))
            .bind("sdl_error_check_minus_one"),
        this);

    Finder->addMatcher(
        ifStmt(hasCondition(binaryOperator(
                   hasOperatorName("<"),
                   hasLHS(callExpr(callee(implicitCastExpr(
                       hasCastKind(CK_FunctionToPointerDecay),
                       hasSourceExpression(declRefExpr(
                           to(functionDecl(matchesName("SDL_[A-Z].*"))))))))),
                   hasRHS(integerLiteral(equals(0))))))
            .bind("sdl_error_check_negative"),
        this);

    Finder->addMatcher(
        ifStmt(hasCondition(binaryOperator(
                   hasOperatorName("=="),
                   hasLHS(callExpr(callee(implicitCastExpr(
                       hasCastKind(CK_FunctionToPointerDecay),
                       hasSourceExpression(declRefExpr(
                           to(functionDecl(matchesName("SDL_[A-Z].*"))))))))),
                   hasRHS(integerLiteral(equals(0))))))
            .bind("sdl_error_check_zero"),
        this);

    Finder->addMatcher(
        ifStmt(hasCondition(unaryOperator(
                   hasOperatorName("!"),
                   hasUnaryOperand(implicitCastExpr(
                       hasCastKind(CK_IntegralToBoolean),
                       hasSourceExpression(callExpr(callee(implicitCastExpr(
                           hasCastKind(CK_FunctionToPointerDecay),
                           hasSourceExpression(declRefExpr(to(functionDecl(
                               matchesName("SDL_[A-Z].*"))))))))))))))
            .bind("sdl_error_check_negation"),
        this);

    for (const auto &Migration : FunctionRenames) {
      Finder->addMatcher(callExpr(callee(functionDecl(hasName(Migration[0]))))
                             .bind(Migration[0]),
                         this);
    }

    for (const auto &Removed : RemovedFunctions) {
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Removed[0])))).bind(Removed[0]),
          this);
    }

    for (const auto &Symbol : SymbolMigrations) {
      Finder->addMatcher(
          declRefExpr(to(namedDecl(hasName(Symbol[0])))).bind(Symbol[0]), this);
    }

    // https://github.com/libsdl-org/SDL/blob/main/build-scripts/rename_headers.py
  }

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {

    if (const auto *SdlResultCheckFound =
            Result.Nodes.getNodeAs<IfStmt>("sdl_error_check_minus_one")) {

      const auto *Condition = SdlResultCheckFound->getCond();
      const auto *BinOp = dyn_cast<BinaryOperator>(Condition);

      if (BinOp) {
        const auto *Call =
            dyn_cast<CallExpr>(BinOp->getLHS()->IgnoreParenImpCasts());
        if (Call) {
          std::string CallText =
              Lexer::getSourceText(
                  CharSourceRange::getTokenRange(Call->getSourceRange()),
                  *Result.SourceManager, Result.Context->getLangOpts())
                  .str();

          diag(SdlResultCheckFound->getIfLoc(),
               "SDL3 functions that previously returned a negative error now "
               "return bool. remove the == -1 part and add a negation to "
               "indicate failure")
              << FixItHint::CreateReplacement(Condition->getSourceRange(),
                                              "!" + CallText);
        }
      }
    }

    if (const auto *SdlResultCheckFound =
            Result.Nodes.getNodeAs<IfStmt>("sdl_error_check_negative")) {
      const auto *Condition = SdlResultCheckFound->getCond();
      const auto *BinOp = dyn_cast<BinaryOperator>(Condition);

      if (BinOp) {
        const auto *Call =
            dyn_cast<CallExpr>(BinOp->getLHS()->IgnoreParenImpCasts());
        if (Call) {
          std::string CallText =
              Lexer::getSourceText(
                  CharSourceRange::getTokenRange(Call->getSourceRange()),
                  *Result.SourceManager, Result.Context->getLangOpts())
                  .str();

          diag(SdlResultCheckFound->getIfLoc(),
               "SDL3 functions that previously returned a negative error now "
               "return bool. remove the < 0 and add a negation to indicate "
               "failure")
              << FixItHint::CreateReplacement(Condition->getSourceRange(),
                                              "!" + CallText);
        }
      }
    }

    if (const auto *SdlResultCheckFound =
            Result.Nodes.getNodeAs<IfStmt>("sdl_error_check_zero")) {
      const auto *Condition = SdlResultCheckFound->getCond();
      const auto *BinOp = dyn_cast<BinaryOperator>(Condition);

      if (BinOp) {
        const auto *Call =
            dyn_cast<CallExpr>(BinOp->getLHS()->IgnoreParenImpCasts());
        if (Call) {
          std::string CallText =
              Lexer::getSourceText(
                  CharSourceRange::getTokenRange(Call->getSourceRange()),
                  *Result.SourceManager, Result.Context->getLangOpts())
                  .str();

          diag(SdlResultCheckFound->getIfLoc(),
               "SDL3 functions that previously returned 0 for success now "
               "return bool. remove the == 0 part for the success branch")
              << FixItHint::CreateReplacement(Condition->getSourceRange(),
                                              CallText);
        }
      }
    }

    if (const auto *SdlResultCheckFound =
            Result.Nodes.getNodeAs<IfStmt>("sdl_error_check_negation")) {
      const auto *Condition = SdlResultCheckFound->getCond();
      const auto *UnaryOp =
          dyn_cast<UnaryOperator>(Condition->IgnoreParenImpCasts());

      if (UnaryOp) {
        const auto *Call =
            dyn_cast<CallExpr>(UnaryOp->getSubExpr()->IgnoreParenImpCasts());
        if (Call) {
          std::string CallText =
              Lexer::getSourceText(
                  CharSourceRange::getTokenRange(Call->getSourceRange()),
                  *Result.SourceManager, Result.Context->getLangOpts())
                  .str();

          diag(SdlResultCheckFound->getIfLoc(),
               "SDL3 functions that previously returned 0 for success now "
               "return bool. remove '!' operator for the success branch")
              << FixItHint::CreateReplacement(Condition->getSourceRange(),
                                              CallText);
        }
      }
    }

    for (const auto &Migration : FunctionRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Migration[0])) {
        std::string Message = std::string(Migration[0]) + "() is now " +
                              Migration[1] + "() in SDL3";

        if (strlen(Migration[2]) > 0) {
          Message += " (" + std::string(Migration[2]) + ")";
        }

        diag(Call->getBeginLoc(), Message) << FixItHint::CreateReplacement(
            Call->getCallee()->getSourceRange(), Migration[1]);
        return;
      }
    }

    for (const auto &Removed : RemovedFunctions) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Removed[0])) {
        std::string Message =
            std::string(Removed[0]) + "() has been " + Removed[1];
        diag(Call->getBeginLoc(), Message);
        return;
      }
    }

    for (const auto &Symbol : SymbolMigrations) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(Symbol[0])) {
        std::string Message =
            std::string(Symbol[0]) + " is now " + Symbol[1] + " in SDL3";

        if (strlen(Symbol[2]) > 0) {
          Message += " (" + std::string(Symbol[2]) + ")";
        }

        diag(DRE->getBeginLoc(), Message)
            << FixItHint::CreateReplacement(DRE->getSourceRange(), Symbol[1]);
        return;
      }
    }
  }
};
// The module that contains all SDL3 migration checks
class SDL3MigrationModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<SDL3InitCheck>("sdl3-migration-init");
    CheckFactories.registerCheck<SDL3AudioCheck>("sdl3-migration-audio");
    CheckFactories.registerCheck<SDL3AtomicCheck>("sdl3-migration-atomic");
    // rename headers
    // rename macros
    // rename symbols
    // rename types
    // rename api
    // change condition checking expressions
    // warn about removed functions
    // rename function calls
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
ClangTidyModule *createClangTidyModule() { return new SDL3MigrationModule(); }
}
