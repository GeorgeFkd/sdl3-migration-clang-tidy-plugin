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

// Helper: match a plain C function call by name
static auto FnCallMatcher(StringRef FunctionName, StringRef BindName) {
  return callExpr(callee(implicitCastExpr(
                      hasCastKind(CK_FunctionToPointerDecay),
                      hasSourceExpression(declRefExpr(
                          to(functionDecl(hasName(FunctionName))))))))
      .bind(BindName);
}

// ---------------------------------------------------------------------------
// PPCallback: rewrites SDL2 #include directives to SDL3 paths
// ---------------------------------------------------------------------------
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
    if (!SM.isInMainFile(HashLoc)) {
      return;
    }
    if (FileName.contains("SDL") ||
        FileName.contains("SDL2") && !FileName.contains("SDL3")) {
      llvm::outs() << "Found an SDL or SDL2 file\n";
      llvm::outs() << "File Entry actually exists\n";
      llvm::outs() << "FileName is:  " << FileName << "\n";
      std::string Replacement;
      if (FileName == "SDL2/SDL.h") {
        Replacement = "SDL3/SDL.h";
      } else if (FileName == "SDL2/SDL_gamecontroller.h") {
        Replacement = "SDL3/SDL_gamepad.h";
      } else if (FileName.starts_with("SDL2/")) {
        Replacement = "SDL3/" + FileName.substr(5).str();
      } else if (FileName == "SDL.h") {
        Replacement = "SDL3/SDL.h";
      } else if (FileName.starts_with("SDL_")) {
        Replacement = "SDL3/" + FileName.str();
      }

      if (!Replacement.empty()) {
        llvm::outs() << "Replacement is not empty\n";
        std::string FormattedReplacement =
            isAngled ? ("<" + Replacement + ">") : ("\"" + Replacement + "\"");

        Check.diag(HashLoc, "replace with %0")
            << Replacement
            << FixItHint::CreateReplacement(FilenameRange,
                                            FormattedReplacement);
      }
    }
  }

private:
  ClangTidyCheck &Check;
  const SourceManager &SM;
};

// ---------------------------------------------------------------------------
// Rename/removal tables shared across checks (SDL_init.h scope)
// ---------------------------------------------------------------------------
static const char *FunctionRenames[][3] = {
    // SDL_endian.h
    {"SDL_SwapBE16", "SDL_Swap16BE", ""},
    {"SDL_SwapBE32", "SDL_Swap32BE", ""},
    {"SDL_SwapBE64", "SDL_Swap64BE", ""},
    {"SDL_SwapLE16", "SDL_Swap16LE", ""},
    {"SDL_SwapLE32", "SDL_Swap32LE", ""},
    {"SDL_SwapLE64", "SDL_Swap64LE", ""},
    // SDL_cpuinfo.h
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
    {"SDL_QueueAudio", ""},
    {"SDL_DequeueAudio", ""},
    {"SDL_ClearAudioQueue", ""},
    {"SDL_GetQueuedAudioSize", ""},
};

static const char *SymbolMigrations[][3] = {
    // SDL_audio.h format symbols
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

    // SDL_events.h symbols
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

// ---------------------------------------------------------------------------
// Helper to emit a function-rename diagnostic with a FixIt
// ---------------------------------------------------------------------------
template <typename NodeT>
static void EmitFuncRenameFixit(ClangTidyCheck &Check, const NodeT *Call,
                                const char *OldName, const char *NewName) {
  Check.diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
      << OldName << NewName
      << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                      NewName);
}

// ---------------------------------------------------------------------------
// SDL3AtomicCheck  (SDL_atomic.h)
// ---------------------------------------------------------------------------
static const char *AtomicFuncRenames[][2] = {
    {"SDL_AtomicAdd", "SDL_AddAtomicInt"},
    {"SDL_AtomicCAS", "SDL_CompareAndSwapAtomicInt"},
    {"SDL_AtomicCASPtr", "SDL_CompareAndSwapAtomicPointer"},
    {"SDL_AtomicGet", "SDL_GetAtomicInt"},
    {"SDL_AtomicGetPtr", "SDL_GetAtomicPointer"},
    {"SDL_AtomicLock", "SDL_LockSpinlock"},
    {"SDL_AtomicSet", "SDL_SetAtomicInt"},
    {"SDL_AtomicSetPtr", "SDL_SetAtomicPointer"},
    {"SDL_AtomicTryLock", "SDL_TryLockSpinlock"},
    {"SDL_AtomicUnlock", "SDL_UnlockSpinlock"},
};

class SDL3AtomicCheck : public ClangTidyCheck {
public:
  SDL3AtomicCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : AtomicFuncRenames) {
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);
    }

    Finder->addMatcher(
        varDecl(hasType(asString("SDL_atomic_t"))).bind("sdl_atomic_t_var"),
        this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : AtomicFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }

    if (const auto *Var = Result.Nodes.getNodeAs<VarDecl>("sdl_atomic_t_var")) {
      diag(Var->getLocation(),
           "SDL_atomic_t has been renamed to SDL_AtomicInt in SDL3")
          << FixItHint::CreateReplacement(
                 Var->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
                 "SDL_AtomicInt");
    }
  }
};

// ---------------------------------------------------------------------------
// SDL3AudioCheck  (SDL_audio.h)
// ---------------------------------------------------------------------------
static const char *AudioFuncRenames[][2] = {
    {"SDL_AudioStreamAvailable", "SDL_GetAudioStreamAvailable"},
    {"SDL_AudioStreamClear", "SDL_ClearAudioStream"},
    {"SDL_AudioStreamFlush", "SDL_FlushAudioStream"},
    {"SDL_AudioStreamGet", "SDL_GetAudioStreamData"},
    {"SDL_AudioStreamPut", "SDL_PutAudioStreamData"},
    {"SDL_FreeAudioStream", "SDL_DestroyAudioStream"},
    {"SDL_LoadWAV_RW", "SDL_LoadWAV_IO"},
};

static const char *AudioFormatMigrations[][3] = {
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
    {"AUDIO_U8", "SDL_AUDIO_U8", ""}};

class SDL3AudioCheck : public ClangTidyCheck {
public:
  SDL3AudioCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    Finder->addMatcher(FnCallMatcher("SDL_AudioInit", "sdl_audio_init"), this);
    Finder->addMatcher(FnCallMatcher("SDL_AudioQuit", "sdl_audio_quit"), this);
    Finder->addMatcher(FnCallMatcher("SDL_FreeWAV", "sdl_free_wav"), this);
    Finder->addMatcher(
        callExpr(callee(implicitCastExpr(
                     hasCastKind(CK_FunctionToPointerDecay),
                     hasSourceExpression(declRefExpr(
                         to(functionDecl(hasName("SDL_MixAudioFormat"))))))),
                 hasArgument(2, expr().bind("audio_format_arg")),
                 hasArgument(4, expr().bind("audio_volume")))
            .bind("sdl_mix_audio_format"),
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

    Finder->addMatcher(
        callExpr(callee(implicitCastExpr(
                     hasCastKind(CK_FunctionToPointerDecay),
                     hasSourceExpression(declRefExpr(
                         to(functionDecl(hasName("SDL_NewAudioStream"))))))),
                 hasArgument(0, expr().bind("src_format")),
                 hasArgument(1, expr().bind("src_channels")),
                 hasArgument(2, expr().bind("src_rate")),
                 hasArgument(3, expr().bind("dst_format")),
                 hasArgument(4, expr().bind("dst_channels")),
                 hasArgument(5, expr().bind("dst_rate")))
            .bind("sdl_new_audio_stream"),
        this);

    for (const auto &S : AudioFormatMigrations) {
      Finder->addMatcher(declRefExpr(to(namedDecl(hasName(S[0])))).bind(S[0]),
                         this);
    }

    for (const auto &R : AudioFuncRenames) {
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);
    }
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("sdl_audio_init")) {
      diag(Call->getBeginLoc(),
           "SDL_AudioInit() has been removed in SDL3. "
           "Use SDL_InitSubSystem(SDL_INIT_AUDIO) instead. "
           "To choose a specific driver, use the SDL_AUDIO_DRIVER hint")
          << FixItHint::CreateReplacement(Call->getSourceRange(),
                                          "SDL_InitSubSystem(SDL_INIT_AUDIO)");
      return;
    }

    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("sdl_audio_quit")) {
      diag(Call->getBeginLoc(), "SDL_AudioQuit() has been removed in SDL3. "
                                "Use SDL_QuitSubSystem(SDL_INIT_AUDIO) instead")
          << FixItHint::CreateReplacement(Call->getSourceRange(),
                                          "SDL_QuitSubSystem(SDL_INIT_AUDIO)");
      return;
    }

    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("sdl_free_wav")) {
      diag(Call->getBeginLoc(),
           "SDL_FreeWAV has been removed; replace with SDL_free")
          << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                          "SDL_free");
      return;
    }

    if (const auto *Call =
            Result.Nodes.getNodeAs<CallExpr>("get_num_audio_devices")) {
      const auto *DeviceType =
          Result.Nodes.getNodeAs<IntegerLiteral>("device_type");
      if (DeviceType) {
        std::string replacement = (DeviceType->getValue() == 0)
                                      ? "SDL_GetAudioPlaybackDevices"
                                      : "SDL_GetAudioRecordingDevices";
        diag(Call->getBeginLoc(),
             "SDL_GetNumAudioDevices() has been removed in SDL3; "
             "use %0(&num_devices) which returns an array of device IDs")
            << replacement;
      }
      return;
    }

    if (const auto *Call =
            Result.Nodes.getNodeAs<CallExpr>("sdl_pause_audio_device")) {
      const auto *PauseValue =
          Result.Nodes.getNodeAs<IntegerLiteral>("pause_value");
      const auto *DeviceArg = Result.Nodes.getNodeAs<Expr>("device_arg");
      if (PauseValue && DeviceArg) {
        std::string DeviceText =
            Lexer::getSourceText(
                CharSourceRange::getTokenRange(DeviceArg->getSourceRange()),
                *Result.SourceManager, Result.Context->getLangOpts())
                .str();
        if (PauseValue->getValue() == 0) {
          diag(Call->getBeginLoc(),
               "SDL_PauseAudioDevice() no longer takes a second argument; "
               "use SDL_ResumeAudioDevice() to unpause")
              << FixItHint::CreateReplacement(Call->getSourceRange(),
                                              "SDL_ResumeAudioDevice(" +
                                                  DeviceText + ")");
        } else {
          diag(Call->getBeginLoc(),
               "SDL_PauseAudioDevice() no longer takes a second argument; "
               "call SDL_PauseAudioDevice() with one argument to pause")
              << FixItHint::CreateReplacement(Call->getSourceRange(),
                                              "SDL_PauseAudioDevice(" +
                                                  DeviceText + ")");
        }
      }
      return;
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
        diag(Call->getBeginLoc(),
             "SDL_GetAudioDeviceStatus() has been removed; "
             "use SDL_AudioDevicePaused() which returns bool")
            << FixItHint::CreateReplacement(Call->getSourceRange(),
                                            "SDL_AudioDevicePaused(" +
                                                DeviceText + ")");
      }
      return;
    }

    for (const auto &S : AudioFormatMigrations) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(S[0])) {
        std::string Msg =
            std::string(S[0]) + " has been renamed to " + S[1] + " in SDL3";
        if (strlen(S[2]) > 0)
          Msg += " (" + std::string(S[2]) + ")";
        diag(DRE->getBeginLoc(), Msg)
            << FixItHint::CreateReplacement(DRE->getSourceRange(), S[1]);
        return;
      }
    }

    for (const auto &R : AudioFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }

    if (const auto *Call =
            Result.Nodes.getNodeAs<CallExpr>("sdl_mix_audio_format")) {
      const auto *FormatArg = Result.Nodes.getNodeAs<Expr>("audio_format_arg");

      std::string FormatText =
          Lexer::getSourceText(
              CharSourceRange::getTokenRange(FormatArg->getSourceRange()),
              *Result.SourceManager, Result.Context->getLangOpts())
              .str();

      const auto *VolumeArg = Result.Nodes.getNodeAs<Expr>("audio_volume");
      std::string VolumeText =
          Lexer::getSourceText(
              CharSourceRange::getTokenRange(VolumeArg->getSourceRange()),
              *Result.SourceManager, Result.Context->getLangOpts())
              .str();
      if (VolumeText == "SDL_MIX_MAXVOLUME") {
        VolumeText = "1.0f";
      } else {
        VolumeText = "(float)" + VolumeText + " / 128";
      }
      // Look up and replace audio format symbol
      for (const auto &S : AudioFormatMigrations) {
        if (FormatText == S[0]) {
          FormatText = S[1];
          break;
        }
      }

      std::string Replacement = "SDL_MixAudio";

      diag(Call->getBeginLoc(), "SDL_MixAudioFormat() has been removed in "
                                "SDL3. Use SDL_MixAudio() instead and change "
                                "the arguments appropriately")
          << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                          Replacement)
          << FixItHint::CreateReplacement(FormatArg->getSourceRange(),
                                          FormatText)
          << FixItHint::CreateReplacement(VolumeArg->getSourceRange(),
                                          VolumeText);
    }
    if (const auto *Call =
            Result.Nodes.getNodeAs<CallExpr>("sdl_new_audio_stream")) {
      // Extract argument text
      const auto *SrcFormat = Result.Nodes.getNodeAs<Expr>("src_format");
      const auto *SrcChannels = Result.Nodes.getNodeAs<Expr>("src_channels");
      const auto *SrcRate = Result.Nodes.getNodeAs<Expr>("src_rate");
      const auto *DstFormat = Result.Nodes.getNodeAs<Expr>("dst_format");
      const auto *DstChannels = Result.Nodes.getNodeAs<Expr>("dst_channels");
      const auto *DstRate = Result.Nodes.getNodeAs<Expr>("dst_rate");

      std::string SrcFormatText =
          Lexer::getSourceText(
              CharSourceRange::getTokenRange(SrcFormat->getSourceRange()),
              *Result.SourceManager, Result.Context->getLangOpts())
              .str();
      std::string SrcChannelsText =
          Lexer::getSourceText(
              CharSourceRange::getTokenRange(SrcChannels->getSourceRange()),
              *Result.SourceManager, Result.Context->getLangOpts())
              .str();
      std::string SrcRateText =
          Lexer::getSourceText(
              CharSourceRange::getTokenRange(SrcRate->getSourceRange()),
              *Result.SourceManager, Result.Context->getLangOpts())
              .str();
      std::string DstFormatText =
          Lexer::getSourceText(
              CharSourceRange::getTokenRange(DstFormat->getSourceRange()),
              *Result.SourceManager, Result.Context->getLangOpts())
              .str();
      std::string DstChannelsText =
          Lexer::getSourceText(
              CharSourceRange::getTokenRange(DstChannels->getSourceRange()),
              *Result.SourceManager, Result.Context->getLangOpts())
              .str();
      std::string DstRateText =
          Lexer::getSourceText(
              CharSourceRange::getTokenRange(DstRate->getSourceRange()),
              *Result.SourceManager, Result.Context->getLangOpts())
              .str();

      for (const auto &S : AudioFormatMigrations) {
        if (SrcFormatText == S[0]) {
          SrcFormatText = S[1];
        }
        if (DstFormatText == S[0]) {
          DstFormatText = S[1];
        }
      }
      // Find the statement containing this call
      auto Parents = Result.Context->getParents(*Call);
      const Stmt *ContainingStmt = nullptr;

      while (!Parents.empty()) {
        const auto &Parent = Parents[0];
        if (const auto *S = Parent.get<Stmt>()) {
          if (isa<DeclStmt>(S) || isa<Expr>(S)) {
            ContainingStmt = S;
            auto NextParents = Result.Context->getParents(Parent);
            if (!NextParents.empty() && NextParents[0].get<CompoundStmt>()) {
              break;
            }
          }
        }
        Parents = Result.Context->getParents(Parent);
      }

      if (ContainingStmt) {
        SourceLocation StmtBegin = ContainingStmt->getBeginLoc();

        std::string VarDecls = "SDL_AudioSpec srcspec = {" + SrcFormatText +
                               ", " + SrcChannelsText + ", " + SrcRateText +
                               "};\n"
                               "  SDL_AudioSpec dstspec = {" +
                               DstFormatText + ", " + DstChannelsText + ", " +
                               DstRateText +
                               "};\n"
                               "  ";

        std::string Replacement = "SDL_CreateAudioStream(&srcspec, &dstspec)";

        diag(Call->getBeginLoc(),
             "SDL_NewAudioStream() has been replaced in SDL3. "
             "Use SDL_CreateAudioStream() with SDL_AudioSpec structures")
            << FixItHint::CreateInsertion(StmtBegin, VarDecls)
            << FixItHint::CreateReplacement(Call->getSourceRange(),
                                            Replacement);
      }
    }
  }
};

// ---------------------------------------------------------------------------
// SDL3InitCheck  (SDL_init.h + error-checking patterns + endian + cpuinfo)
// ---------------------------------------------------------------------------
class SDL3InitCheck : public ClangTidyCheck {
public:
  SDL3InitCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    // SDL3 functions return bool; match old SDL2 error-check patterns
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

    for (const auto &R : FunctionRenames) {
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);
    }

    for (const auto &Rem : RemovedFunctions) {
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Rem[0])))).bind(Rem[0]), this);
    }

    for (const auto &S : SymbolMigrations) {
      Finder->addMatcher(declRefExpr(to(namedDecl(hasName(S[0])))).bind(S[0]),
                         this);
    }
  }

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    if (const auto *IfS =
            Result.Nodes.getNodeAs<IfStmt>("sdl_error_check_minus_one")) {
      const auto *Cond = IfS->getCond();
      if (const auto *BinOp = dyn_cast<BinaryOperator>(Cond)) {
        if (const auto *Call =
                dyn_cast<CallExpr>(BinOp->getLHS()->IgnoreParenImpCasts())) {
          std::string CallText =
              Lexer::getSourceText(
                  CharSourceRange::getTokenRange(Call->getSourceRange()),
                  *Result.SourceManager, Result.Context->getLangOpts())
                  .str();
          diag(IfS->getIfLoc(),
               "SDL3 functions that returned a negative error now return bool; "
               "remove '== -1' and negate to indicate failure")
              << FixItHint::CreateReplacement(Cond->getSourceRange(),
                                              "!" + CallText);
        }
      }
      return;
    }

    if (const auto *IfS =
            Result.Nodes.getNodeAs<IfStmt>("sdl_error_check_negative")) {
      const auto *Cond = IfS->getCond();
      if (const auto *BinOp = dyn_cast<BinaryOperator>(Cond)) {
        if (const auto *Call =
                dyn_cast<CallExpr>(BinOp->getLHS()->IgnoreParenImpCasts())) {
          std::string CallText =
              Lexer::getSourceText(
                  CharSourceRange::getTokenRange(Call->getSourceRange()),
                  *Result.SourceManager, Result.Context->getLangOpts())
                  .str();
          diag(IfS->getIfLoc(),
               "SDL3 functions that returned a negative error now return bool; "
               "remove '< 0' and negate to indicate failure")
              << FixItHint::CreateReplacement(Cond->getSourceRange(),
                                              "!" + CallText);
        }
      }
      return;
    }

    if (const auto *IfS =
            Result.Nodes.getNodeAs<IfStmt>("sdl_error_check_zero")) {
      const auto *Cond = IfS->getCond();
      if (const auto *BinOp = dyn_cast<BinaryOperator>(Cond)) {
        if (const auto *Call =
                dyn_cast<CallExpr>(BinOp->getLHS()->IgnoreParenImpCasts())) {
          std::string CallText =
              Lexer::getSourceText(
                  CharSourceRange::getTokenRange(Call->getSourceRange()),
                  *Result.SourceManager, Result.Context->getLangOpts())
                  .str();
          diag(IfS->getIfLoc(),
               "SDL3 functions that returned 0 for success now return bool; "
               "remove '== 0' for the success branch")
              << FixItHint::CreateReplacement(Cond->getSourceRange(), CallText);
        }
      }
      return;
    }

    if (const auto *IfS =
            Result.Nodes.getNodeAs<IfStmt>("sdl_error_check_negation")) {
      const auto *Cond = IfS->getCond();
      if (const auto *UnaryOp =
              dyn_cast<UnaryOperator>(Cond->IgnoreParenImpCasts())) {
        if (const auto *Call = dyn_cast<CallExpr>(
                UnaryOp->getSubExpr()->IgnoreParenImpCasts())) {
          std::string CallText =
              Lexer::getSourceText(
                  CharSourceRange::getTokenRange(Call->getSourceRange()),
                  *Result.SourceManager, Result.Context->getLangOpts())
                  .str();
          diag(IfS->getIfLoc(),
               "SDL3 functions that returned 0 for success now return bool; "
               "remove '!' for the success branch")
              << FixItHint::CreateReplacement(Cond->getSourceRange(), CallText);
        }
      }
      return;
    }

    for (const auto &R : FunctionRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        std::string Msg =
            std::string(R[0]) + "() has been renamed to " + R[1] + "() in SDL3";
        if (strlen(R[2]) > 0)
          Msg += " (" + std::string(R[2]) + ")";
        diag(Call->getBeginLoc(), Msg) << FixItHint::CreateReplacement(
            Call->getCallee()->getSourceRange(), R[1]);
        return;
      }
    }

    for (const auto &Rem : RemovedFunctions) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Rem[0])) {
        diag(Call->getBeginLoc(),
             "%0() has been removed in SDL3; see migration guide")
            << Rem[0];
        return;
      }
    }

    for (const auto &S : SymbolMigrations) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(S[0])) {
        std::string Msg =
            std::string(S[0]) + " has been renamed to " + S[1] + " in SDL3";
        if (strlen(S[2]) > 0)
          Msg += " (" + std::string(S[2]) + ")";
        diag(DRE->getBeginLoc(), Msg)
            << FixItHint::CreateReplacement(DRE->getSourceRange(), S[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3GamepadCheck  (SDL_gamecontroller.h -> SDL_gamepad.h)
// ===========================================================================
static const char *GamepadFuncRenames[][2] = {
    {"SDL_GameControllerAddMapping", "SDL_AddGamepadMapping"},
    {"SDL_GameControllerAddMappingsFromFile", "SDL_AddGamepadMappingsFromFile"},
    {"SDL_GameControllerAddMappingsFromRW", "SDL_AddGamepadMappingsFromIO"},
    {"SDL_GameControllerClose", "SDL_CloseGamepad"},
    {"SDL_GameControllerFromInstanceID", "SDL_GetGamepadFromID"},
    {"SDL_GameControllerFromPlayerIndex", "SDL_GetGamepadFromPlayerIndex"},
    {"SDL_GameControllerGetAppleSFSymbolsNameForAxis",
     "SDL_GetGamepadAppleSFSymbolsNameForAxis"},
    {"SDL_GameControllerGetAppleSFSymbolsNameForButton",
     "SDL_GetGamepadAppleSFSymbolsNameForButton"},
    {"SDL_GameControllerGetAttached", "SDL_GamepadConnected"},
    {"SDL_GameControllerGetAxis", "SDL_GetGamepadAxis"},
    {"SDL_GameControllerGetAxisFromString", "SDL_GetGamepadAxisFromString"},
    {"SDL_GameControllerGetButton", "SDL_GetGamepadButton"},
    {"SDL_GameControllerGetButtonFromString", "SDL_GetGamepadButtonFromString"},
    {"SDL_GameControllerGetFirmwareVersion", "SDL_GetGamepadFirmwareVersion"},
    {"SDL_GameControllerGetJoystick", "SDL_GetGamepadJoystick"},
    {"SDL_GameControllerGetNumTouchpadFingers",
     "SDL_GetNumGamepadTouchpadFingers"},
    {"SDL_GameControllerGetNumTouchpads", "SDL_GetNumGamepadTouchpads"},
    {"SDL_GameControllerGetPlayerIndex", "SDL_GetGamepadPlayerIndex"},
    {"SDL_GameControllerGetProduct", "SDL_GetGamepadProduct"},
    {"SDL_GameControllerGetProductVersion", "SDL_GetGamepadProductVersion"},
    {"SDL_GameControllerGetSensorData", "SDL_GetGamepadSensorData"},
    {"SDL_GameControllerGetSensorDataRate", "SDL_GetGamepadSensorDataRate"},
    {"SDL_GameControllerGetSerial", "SDL_GetGamepadSerial"},
    {"SDL_GameControllerGetSteamHandle", "SDL_GetGamepadSteamHandle"},
    {"SDL_GameControllerGetStringForAxis", "SDL_GetGamepadStringForAxis"},
    {"SDL_GameControllerGetStringForButton", "SDL_GetGamepadStringForButton"},
    {"SDL_GameControllerGetTouchpadFinger", "SDL_GetGamepadTouchpadFinger"},
    {"SDL_GameControllerGetType", "SDL_GetGamepadType"},
    {"SDL_GameControllerGetVendor", "SDL_GetGamepadVendor"},
    {"SDL_GameControllerHasAxis", "SDL_GamepadHasAxis"},
    {"SDL_GameControllerHasButton", "SDL_GamepadHasButton"},
    {"SDL_GameControllerHasSensor", "SDL_GamepadHasSensor"},
    {"SDL_GameControllerIsSensorEnabled", "SDL_GamepadSensorEnabled"},
    {"SDL_GameControllerMapping", "SDL_GetGamepadMapping"},
    {"SDL_GameControllerMappingForGUID", "SDL_GetGamepadMappingForGUID"},
    {"SDL_GameControllerName", "SDL_GetGamepadName"},
    {"SDL_GameControllerOpen", "SDL_OpenGamepad"},
    {"SDL_GameControllerPath", "SDL_GetGamepadPath"},
    {"SDL_GameControllerRumble", "SDL_RumbleGamepad"},
    {"SDL_GameControllerRumbleTriggers", "SDL_RumbleGamepadTriggers"},
    {"SDL_GameControllerSendEffect", "SDL_SendGamepadEffect"},
    {"SDL_GameControllerSetLED", "SDL_SetGamepadLED"},
    {"SDL_GameControllerSetPlayerIndex", "SDL_SetGamepadPlayerIndex"},
    {"SDL_GameControllerSetSensorEnabled", "SDL_SetGamepadSensorEnabled"},
    {"SDL_GameControllerUpdate", "SDL_UpdateGamepads"},
    {"SDL_IsGameController", "SDL_IsGamepad"},
};

static const char *GamepadRemovedFuncs[] = {
    "SDL_GameControllerEventState",
    "SDL_GameControllerGetBindForAxis",
    "SDL_GameControllerGetBindForButton",
    "SDL_GameControllerHasLED",
    "SDL_GameControllerHasRumble",
    "SDL_GameControllerHasRumbleTriggers",
    "SDL_GameControllerMappingForDeviceIndex",
    "SDL_GameControllerMappingForIndex",
    "SDL_GameControllerNameForIndex",
    "SDL_GameControllerNumMappings",
    "SDL_GameControllerPathForIndex",
    "SDL_GameControllerTypeForIndex",
    "SDL_GameControllerGetSensorDataWithTimestamp",
};

static const char *GamepadSymbolRenames[][2] = {
    {"SDL_CONTROLLER_AXIS_INVALID", "SDL_GAMEPAD_AXIS_INVALID"},
    {"SDL_CONTROLLER_AXIS_LEFTX", "SDL_GAMEPAD_AXIS_LEFTX"},
    {"SDL_CONTROLLER_AXIS_LEFTY", "SDL_GAMEPAD_AXIS_LEFTY"},
    {"SDL_CONTROLLER_AXIS_MAX", "SDL_GAMEPAD_AXIS_COUNT"},
    {"SDL_CONTROLLER_AXIS_RIGHTX", "SDL_GAMEPAD_AXIS_RIGHTX"},
    {"SDL_CONTROLLER_AXIS_RIGHTY", "SDL_GAMEPAD_AXIS_RIGHTY"},
    {"SDL_CONTROLLER_AXIS_TRIGGERLEFT", "SDL_GAMEPAD_AXIS_LEFT_TRIGGER"},
    {"SDL_CONTROLLER_AXIS_TRIGGERRIGHT", "SDL_GAMEPAD_AXIS_RIGHT_TRIGGER"},
    {"SDL_CONTROLLER_BINDTYPE_AXIS", "SDL_GAMEPAD_BINDTYPE_AXIS"},
    {"SDL_CONTROLLER_BINDTYPE_BUTTON", "SDL_GAMEPAD_BINDTYPE_BUTTON"},
    {"SDL_CONTROLLER_BINDTYPE_HAT", "SDL_GAMEPAD_BINDTYPE_HAT"},
    {"SDL_CONTROLLER_BINDTYPE_NONE", "SDL_GAMEPAD_BINDTYPE_NONE"},
    {"SDL_CONTROLLER_BUTTON_A", "SDL_GAMEPAD_BUTTON_SOUTH"},
    {"SDL_CONTROLLER_BUTTON_B", "SDL_GAMEPAD_BUTTON_EAST"},
    {"SDL_CONTROLLER_BUTTON_BACK", "SDL_GAMEPAD_BUTTON_BACK"},
    {"SDL_CONTROLLER_BUTTON_DPAD_DOWN", "SDL_GAMEPAD_BUTTON_DPAD_DOWN"},
    {"SDL_CONTROLLER_BUTTON_DPAD_LEFT", "SDL_GAMEPAD_BUTTON_DPAD_LEFT"},
    {"SDL_CONTROLLER_BUTTON_DPAD_RIGHT", "SDL_GAMEPAD_BUTTON_DPAD_RIGHT"},
    {"SDL_CONTROLLER_BUTTON_DPAD_UP", "SDL_GAMEPAD_BUTTON_DPAD_UP"},
    {"SDL_CONTROLLER_BUTTON_GUIDE", "SDL_GAMEPAD_BUTTON_GUIDE"},
    {"SDL_CONTROLLER_BUTTON_INVALID", "SDL_GAMEPAD_BUTTON_INVALID"},
    {"SDL_CONTROLLER_BUTTON_LEFTSHOULDER", "SDL_GAMEPAD_BUTTON_LEFT_SHOULDER"},
    {"SDL_CONTROLLER_BUTTON_LEFTSTICK", "SDL_GAMEPAD_BUTTON_LEFT_STICK"},
    {"SDL_CONTROLLER_BUTTON_MAX", "SDL_GAMEPAD_BUTTON_COUNT"},
    {"SDL_CONTROLLER_BUTTON_MISC1", "SDL_GAMEPAD_BUTTON_MISC1"},
    {"SDL_CONTROLLER_BUTTON_PADDLE1", "SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1"},
    {"SDL_CONTROLLER_BUTTON_PADDLE2", "SDL_GAMEPAD_BUTTON_LEFT_PADDLE1"},
    {"SDL_CONTROLLER_BUTTON_PADDLE3", "SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2"},
    {"SDL_CONTROLLER_BUTTON_PADDLE4", "SDL_GAMEPAD_BUTTON_LEFT_PADDLE2"},
    {"SDL_CONTROLLER_BUTTON_RIGHTSHOULDER",
     "SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER"},
    {"SDL_CONTROLLER_BUTTON_RIGHTSTICK", "SDL_GAMEPAD_BUTTON_RIGHT_STICK"},
    {"SDL_CONTROLLER_BUTTON_START", "SDL_GAMEPAD_BUTTON_START"},
    {"SDL_CONTROLLER_BUTTON_TOUCHPAD", "SDL_GAMEPAD_BUTTON_TOUCHPAD"},
    {"SDL_CONTROLLER_BUTTON_X", "SDL_GAMEPAD_BUTTON_WEST"},
    {"SDL_CONTROLLER_BUTTON_Y", "SDL_GAMEPAD_BUTTON_NORTH"},
    {"SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT",
     "SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT"},
    {"SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR",
     "SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR"},
    {"SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT",
     "SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT"},
    {"SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO",
     "SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO"},
    {"SDL_CONTROLLER_TYPE_PS3", "SDL_GAMEPAD_TYPE_PS3"},
    {"SDL_CONTROLLER_TYPE_PS4", "SDL_GAMEPAD_TYPE_PS4"},
    {"SDL_CONTROLLER_TYPE_PS5", "SDL_GAMEPAD_TYPE_PS5"},
    {"SDL_CONTROLLER_TYPE_UNKNOWN", "SDL_GAMEPAD_TYPE_STANDARD"},
    {"SDL_CONTROLLER_TYPE_XBOX360", "SDL_GAMEPAD_TYPE_XBOX360"},
    {"SDL_CONTROLLER_TYPE_XBOXONE", "SDL_GAMEPAD_TYPE_XBOXONE"},
};

class SDL3GamepadCheck : public ClangTidyCheck {
public:
  SDL3GamepadCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {

    Finder->addMatcher(varDecl(hasType(asString("SDL_GameController *")))
                           .bind("sdl_game_controller_var"),
                       this);
    for (const auto &R : GamepadFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);

    for (const auto &Removed : GamepadRemovedFuncs)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Removed)))).bind(Removed), this);

    for (const auto &S : GamepadSymbolRenames)
      Finder->addMatcher(declRefExpr(to(namedDecl(hasName(S[0])))).bind(S[0]),
                         this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    if (const auto *Var =
            Result.Nodes.getNodeAs<VarDecl>("sdl_game_controller_var")) {
      diag(Var->getLocation(),
           "SDL_GameController has been renamed to SDL_Gamepad in SDL3")
          << FixItHint::CreateReplacement(
                 Var->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
                 "SDL_Gamepad *");
    }
    for (const auto &R : GamepadFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
    for (const auto &Removed : GamepadRemovedFuncs) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Removed)) {
        diag(Call->getBeginLoc(), "%0() has been removed in SDL3") << Removed;
        return;
      }
    }
    for (const auto &S : GamepadSymbolRenames) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(S[0])) {
        diag(DRE->getBeginLoc(), "%0 has been renamed to %1 in SDL3")
            << S[0] << S[1]
            << FixItHint::CreateReplacement(DRE->getSourceRange(), S[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3JoystickCheck  (SDL_joystick.h)
// ===========================================================================
static const char *JoystickFuncRenames[][2] = {
    {"SDL_JoystickAttachVirtualEx", "SDL_AttachVirtualJoystick"},
    {"SDL_JoystickClose", "SDL_CloseJoystick"},
    {"SDL_JoystickDetachVirtual", "SDL_DetachVirtualJoystick"},
    {"SDL_JoystickFromInstanceID", "SDL_GetJoystickFromID"},
    {"SDL_JoystickFromPlayerIndex", "SDL_GetJoystickFromPlayerIndex"},
    {"SDL_JoystickGetAttached", "SDL_JoystickConnected"},
    {"SDL_JoystickGetAxis", "SDL_GetJoystickAxis"},
    {"SDL_JoystickGetAxisInitialState", "SDL_GetJoystickAxisInitialState"},
    {"SDL_JoystickGetBall", "SDL_GetJoystickBall"},
    {"SDL_JoystickGetButton", "SDL_GetJoystickButton"},
    {"SDL_JoystickGetFirmwareVersion", "SDL_GetJoystickFirmwareVersion"},
    {"SDL_JoystickGetGUID", "SDL_GetJoystickGUID"},
    {"SDL_JoystickGetGUIDFromString", "SDL_StringToGUID"},
    {"SDL_JoystickGetHat", "SDL_GetJoystickHat"},
    {"SDL_JoystickGetPlayerIndex", "SDL_GetJoystickPlayerIndex"},
    {"SDL_JoystickGetProduct", "SDL_GetJoystickProduct"},
    {"SDL_JoystickGetProductVersion", "SDL_GetJoystickProductVersion"},
    {"SDL_JoystickGetSerial", "SDL_GetJoystickSerial"},
    {"SDL_JoystickGetType", "SDL_GetJoystickType"},
    {"SDL_JoystickGetVendor", "SDL_GetJoystickVendor"},
    {"SDL_JoystickInstanceID", "SDL_GetJoystickID"},
    {"SDL_JoystickIsVirtual", "SDL_IsJoystickVirtual"},
    {"SDL_JoystickName", "SDL_GetJoystickName"},
    {"SDL_JoystickNumAxes", "SDL_GetNumJoystickAxes"},
    {"SDL_JoystickNumBalls", "SDL_GetNumJoystickBalls"},
    {"SDL_JoystickNumButtons", "SDL_GetNumJoystickButtons"},
    {"SDL_JoystickNumHats", "SDL_GetNumJoystickHats"},
    {"SDL_JoystickOpen", "SDL_OpenJoystick"},
    {"SDL_JoystickPath", "SDL_GetJoystickPath"},
    {"SDL_JoystickRumble", "SDL_RumbleJoystick"},
    {"SDL_JoystickRumbleTriggers", "SDL_RumbleJoystickTriggers"},
    {"SDL_JoystickSendEffect", "SDL_SendJoystickEffect"},
    {"SDL_JoystickSetLED", "SDL_SetJoystickLED"},
    {"SDL_JoystickSetPlayerIndex", "SDL_SetJoystickPlayerIndex"},
    {"SDL_JoystickSetVirtualAxis", "SDL_SetJoystickVirtualAxis"},
    {"SDL_JoystickSetVirtualButton", "SDL_SetJoystickVirtualButton"},
    {"SDL_JoystickSetVirtualHat", "SDL_SetJoystickVirtualHat"},
    {"SDL_JoystickUpdate", "SDL_UpdateJoysticks"},
};

static const char *JoystickRemovedFuncs[] = {
    "SDL_JoystickAttachVirtual",
    "SDL_JoystickCurrentPowerLevel",
    "SDL_JoystickEventState",
    "SDL_JoystickGetDeviceGUID",
    "SDL_JoystickGetDeviceInstanceID",
    "SDL_JoystickGetDevicePlayerIndex",
    "SDL_JoystickGetDeviceProduct",
    "SDL_JoystickGetDeviceProductVersion",
    "SDL_JoystickGetDeviceType",
    "SDL_JoystickGetDeviceVendor",
    "SDL_JoystickGetGUIDString",
    "SDL_JoystickHasLED",
    "SDL_JoystickHasRumble",
    "SDL_JoystickHasRumbleTriggers",
    "SDL_JoystickNameForIndex",
    "SDL_JoystickPathForIndex",
    "SDL_NumJoysticks",
};

static const char *JoystickSymbolRenames[][2] = {
    {"SDL_JOYSTICK_TYPE_GAMECONTROLLER", "SDL_JOYSTICK_TYPE_GAMEPAD"},
};

class SDL3JoystickCheck : public ClangTidyCheck {
public:
  SDL3JoystickCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : JoystickFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);

    for (const auto &Removed : JoystickRemovedFuncs)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Removed)))).bind(Removed), this);

    for (const auto &S : JoystickSymbolRenames)
      Finder->addMatcher(declRefExpr(to(namedDecl(hasName(S[0])))).bind(S[0]),
                         this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : JoystickFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
    for (const auto &Removed : JoystickRemovedFuncs) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Removed)) {
        diag(Call->getBeginLoc(), "%0() has been removed in SDL3") << Removed;
        return;
      }
    }
    for (const auto &S : JoystickSymbolRenames) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(S[0])) {
        diag(DRE->getBeginLoc(), "%0 has been renamed to %1 in SDL3")
            << S[0] << S[1]
            << FixItHint::CreateReplacement(DRE->getSourceRange(), S[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3HapticCheck  (SDL_haptic.h)
// ===========================================================================
static const char *HapticFuncRenames[][2] = {
    {"SDL_HapticClose", "SDL_CloseHaptic"},
    {"SDL_HapticDestroyEffect", "SDL_DestroyHapticEffect"},
    {"SDL_HapticGetEffectStatus", "SDL_GetHapticEffectStatus"},
    {"SDL_HapticNewEffect", "SDL_CreateHapticEffect"},
    {"SDL_HapticNumAxes", "SDL_GetNumHapticAxes"},
    {"SDL_HapticNumEffects", "SDL_GetMaxHapticEffects"},
    {"SDL_HapticNumEffectsPlaying", "SDL_GetMaxHapticEffectsPlaying"},
    {"SDL_HapticOpen", "SDL_OpenHaptic"},
    {"SDL_HapticOpenFromJoystick", "SDL_OpenHapticFromJoystick"},
    {"SDL_HapticOpenFromMouse", "SDL_OpenHapticFromMouse"},
    {"SDL_HapticPause", "SDL_PauseHaptic"},
    {"SDL_HapticQuery", "SDL_GetHapticFeatures"},
    {"SDL_HapticRumbleInit", "SDL_InitHapticRumble"},
    {"SDL_HapticRumblePlay", "SDL_PlayHapticRumble"},
    {"SDL_HapticRumbleStop", "SDL_StopHapticRumble"},
    {"SDL_HapticRunEffect", "SDL_RunHapticEffect"},
    {"SDL_HapticSetAutocenter", "SDL_SetHapticAutocenter"},
    {"SDL_HapticSetGain", "SDL_SetHapticGain"},
    {"SDL_HapticStopAll", "SDL_StopHapticEffects"},
    {"SDL_HapticStopEffect", "SDL_StopHapticEffect"},
    {"SDL_HapticUnpause", "SDL_ResumeHaptic"},
    {"SDL_HapticUpdateEffect", "SDL_UpdateHapticEffect"},
    {"SDL_JoystickIsHaptic", "SDL_IsJoystickHaptic"},
    {"SDL_MouseIsHaptic", "SDL_IsMouseHaptic"},
};

static const char *HapticRemovedFuncs[] = {
    "SDL_HapticIndex",
    "SDL_HapticName",
    "SDL_HapticOpened",
    "SDL_NumHaptics",
};

class SDL3HapticCheck : public ClangTidyCheck {
public:
  SDL3HapticCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : HapticFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);

    for (const auto &Removed : HapticRemovedFuncs)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Removed)))).bind(Removed), this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : HapticFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
    for (const auto &Removed : HapticRemovedFuncs) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Removed)) {
        diag(Call->getBeginLoc(), "%0() has been removed in SDL3") << Removed;
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3MouseCheck  (SDL_mouse.h)
// ===========================================================================
static const char *MouseFuncRenames[][2] = {
    {"SDL_FreeCursor", "SDL_DestroyCursor"},
};

static const char *MouseRemovedFuncs[] = {
    "SDL_SetRelativeMouseMode",
    "SDL_GetRelativeMouseMode",
};

static const char *MouseSymbolRenames[][2] = {
    {"SDL_BUTTON", "SDL_BUTTON_MASK"},
    {"SDL_NUM_SYSTEM_CURSORS", "SDL_SYSTEM_CURSOR_COUNT"},
    {"SDL_SYSTEM_CURSOR_ARROW", "SDL_SYSTEM_CURSOR_DEFAULT"},
    {"SDL_SYSTEM_CURSOR_HAND", "SDL_SYSTEM_CURSOR_POINTER"},
    {"SDL_SYSTEM_CURSOR_IBEAM", "SDL_SYSTEM_CURSOR_TEXT"},
    {"SDL_SYSTEM_CURSOR_NO", "SDL_SYSTEM_CURSOR_NOT_ALLOWED"},
    {"SDL_SYSTEM_CURSOR_SIZEALL", "SDL_SYSTEM_CURSOR_MOVE"},
    {"SDL_SYSTEM_CURSOR_SIZENESW", "SDL_SYSTEM_CURSOR_NESW_RESIZE"},
    {"SDL_SYSTEM_CURSOR_SIZENS", "SDL_SYSTEM_CURSOR_NS_RESIZE"},
    {"SDL_SYSTEM_CURSOR_SIZENWSE", "SDL_SYSTEM_CURSOR_NWSE_RESIZE"},
    {"SDL_SYSTEM_CURSOR_SIZEWE", "SDL_SYSTEM_CURSOR_EW_RESIZE"},
    {"SDL_SYSTEM_CURSOR_WAITARROW", "SDL_SYSTEM_CURSOR_PROGRESS"},
};

class SDL3MouseCheck : public ClangTidyCheck {
public:
  SDL3MouseCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : MouseFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);

    for (const auto &Removed : MouseRemovedFuncs)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Removed)))).bind(Removed), this);

    for (const auto &S : MouseSymbolRenames)
      Finder->addMatcher(declRefExpr(to(namedDecl(hasName(S[0])))).bind(S[0]),
                         this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : MouseFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
    for (const auto &Removed : MouseRemovedFuncs) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Removed)) {
        if (StringRef(Removed) == "SDL_SetRelativeMouseMode") {
          diag(Call->getBeginLoc(),
               "SDL_SetRelativeMouseMode() has been removed; "
               "use SDL_SetWindowRelativeMouseMode() instead");
        } else if (StringRef(Removed) == "SDL_GetRelativeMouseMode") {
          diag(Call->getBeginLoc(),
               "SDL_GetRelativeMouseMode() has been removed; "
               "use SDL_GetWindowRelativeMouseMode() instead");
        } else {
          diag(Call->getBeginLoc(), "%0() has been removed in SDL3") << Removed;
        }
        return;
      }
    }
    for (const auto &S : MouseSymbolRenames) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(S[0])) {
        diag(DRE->getBeginLoc(), "%0 has been renamed to %1 in SDL3")
            << S[0] << S[1]
            << FixItHint::CreateReplacement(DRE->getSourceRange(), S[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3RenderCheck  (SDL_render.h)
// ===========================================================================
static const char *RenderFuncRenames[][2] = {
    {"SDL_GetRendererOutputSize", "SDL_GetCurrentRenderOutputSize"},
    {"SDL_RenderCopy", "SDL_RenderTexture"},
    {"SDL_RenderCopyEx", "SDL_RenderTextureRotated"},
    {"SDL_RenderCopyExF", "SDL_RenderTextureRotated"},
    {"SDL_RenderCopyF", "SDL_RenderTexture"},
    {"SDL_RenderDrawLine", "SDL_RenderLine"},
    {"SDL_RenderDrawLineF", "SDL_RenderLine"},
    {"SDL_RenderDrawLines", "SDL_RenderLines"},
    {"SDL_RenderDrawLinesF", "SDL_RenderLines"},
    {"SDL_RenderDrawPoint", "SDL_RenderPoint"},
    {"SDL_RenderDrawPointF", "SDL_RenderPoint"},
    {"SDL_RenderDrawPoints", "SDL_RenderPoints"},
    {"SDL_RenderDrawPointsF", "SDL_RenderPoints"},
    {"SDL_RenderDrawRect", "SDL_RenderRect"},
    {"SDL_RenderDrawRectF", "SDL_RenderRect"},
    {"SDL_RenderDrawRects", "SDL_RenderRects"},
    {"SDL_RenderDrawRectsF", "SDL_RenderRects"},
    {"SDL_RenderFillRectF", "SDL_RenderFillRect"},
    {"SDL_RenderFillRectsF", "SDL_RenderFillRects"},
    {"SDL_RenderFlush", "SDL_FlushRenderer"},
    {"SDL_RenderGetClipRect", "SDL_GetRenderClipRect"},
    {"SDL_RenderGetIntegerScale", "SDL_GetRenderIntegerScale"},
    {"SDL_RenderGetLogicalSize", "SDL_GetRenderLogicalPresentation"},
    {"SDL_RenderGetMetalCommandEncoder", "SDL_GetRenderMetalCommandEncoder"},
    {"SDL_RenderGetMetalLayer", "SDL_GetRenderMetalLayer"},
    {"SDL_RenderGetScale", "SDL_GetRenderScale"},
    {"SDL_RenderGetViewport", "SDL_GetRenderViewport"},
    {"SDL_RenderGetWindow", "SDL_GetRenderWindow"},
    {"SDL_RenderIsClipEnabled", "SDL_RenderClipEnabled"},
    {"SDL_RenderLogicalToWindow", "SDL_RenderCoordinatesToWindow"},
    {"SDL_RenderSetClipRect", "SDL_SetRenderClipRect"},
    {"SDL_RenderSetLogicalSize", "SDL_SetRenderLogicalPresentation"},
    {"SDL_RenderSetScale", "SDL_SetRenderScale"},
    {"SDL_RenderSetVSync", "SDL_SetRenderVSync"},
    {"SDL_RenderSetViewport", "SDL_SetRenderViewport"},
    {"SDL_RenderWindowToLogical", "SDL_RenderCoordinatesFromWindow"},
};

static const char *RenderRemovedFuncs[] = {
    "SDL_GL_BindTexture",        "SDL_GL_UnbindTexture",
    "SDL_GetTextureUserData",    "SDL_RenderSetIntegerScale",
    "SDL_RenderTargetSupported", "SDL_SetTextureUserData",
    "SDL_GetRenderDriverInfo",
};

static const char *RenderSymbolRenames[][2] = {
    {"SDL_ScaleModeLinear", "SDL_SCALEMODE_LINEAR"},
    {"SDL_ScaleModeNearest", "SDL_SCALEMODE_NEAREST"},
};

class SDL3RenderCheck : public ClangTidyCheck {
public:
  SDL3RenderCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : RenderFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);

    for (const auto &Removed : RenderRemovedFuncs)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Removed)))).bind(Removed), this);

    for (const auto &S : RenderSymbolRenames)
      Finder->addMatcher(declRefExpr(to(namedDecl(hasName(S[0])))).bind(S[0]),
                         this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : RenderFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
    for (const auto &Removed : RenderRemovedFuncs) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Removed)) {
        diag(Call->getBeginLoc(), "%0() has been removed in SDL3") << Removed;
        return;
      }
    }
    for (const auto &S : RenderSymbolRenames) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(S[0])) {
        diag(DRE->getBeginLoc(), "%0 has been renamed to %1 in SDL3")
            << S[0] << S[1]
            << FixItHint::CreateReplacement(DRE->getSourceRange(), S[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3MutexCheck  (SDL_mutex.h)
// ===========================================================================
static const char *MutexFuncRenames[][2] = {
    {"SDL_CondBroadcast", "SDL_BroadcastCondition"},
    {"SDL_CondSignal", "SDL_SignalCondition"},
    {"SDL_CondWait", "SDL_WaitCondition"},
    {"SDL_CondWaitTimeout", "SDL_WaitConditionTimeout"},
    {"SDL_CreateCond", "SDL_CreateCondition"},
    {"SDL_DestroyCond", "SDL_DestroyCondition"},
    {"SDL_SemPost", "SDL_SignalSemaphore"},
    {"SDL_SemTryWait", "SDL_TryWaitSemaphore"},
    {"SDL_SemValue", "SDL_GetSemaphoreValue"},
    {"SDL_SemWait", "SDL_WaitSemaphore"},
    {"SDL_SemWaitTimeout", "SDL_WaitSemaphoreTimeout"},
};

static const char *MutexTypeMigrations[][2] = {
    {"SDL_mutex *", "SDL_Mutex *"},
    {"SDL_cond *", "SDL_Condition *"},
    {"SDL_sem *", "SDL_Semaphore *"}};

class SDL3MutexCheck : public ClangTidyCheck {
public:
  SDL3MutexCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {

    for (const auto &T : MutexTypeMigrations) {
      Finder->addMatcher(varDecl(hasType(asString(T[0]))).bind(T[0]), this);
    }

    for (const auto &R : MutexFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : MutexFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }

    for (const auto &T : MutexTypeMigrations) {
      if (const auto *Var = Result.Nodes.getNodeAs<VarDecl>(T[0])) {
        diag(Var->getLocation(),
             std::string(T[0]) + " has been renamed to " + T[1] + " in SDL3")
            << FixItHint::CreateReplacement(
                   Var->getTypeSourceInfo()->getTypeLoc().getSourceRange(),
                   T[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3RectCheck  (SDL_rect.h)
// ===========================================================================
static const char *RectFuncRenames[][2] = {
    {"SDL_EncloseFPoints", "SDL_GetRectEnclosingPointsFloat"},
    {"SDL_EnclosePoints", "SDL_GetRectEnclosingPoints"},
    {"SDL_FRectEmpty", "SDL_RectEmptyFloat"},
    {"SDL_FRectEquals", "SDL_RectsEqualFloat"},
    {"SDL_FRectEqualsEpsilon", "SDL_RectsEqualEpsilon"},
    {"SDL_HasIntersection", "SDL_HasRectIntersection"},
    {"SDL_HasIntersectionF", "SDL_HasRectIntersectionFloat"},
    {"SDL_IntersectFRect", "SDL_GetRectIntersectionFloat"},
    {"SDL_IntersectFRectAndLine", "SDL_GetRectAndLineIntersectionFloat"},
    {"SDL_IntersectRect", "SDL_GetRectIntersection"},
    {"SDL_IntersectRectAndLine", "SDL_GetRectAndLineIntersection"},
    {"SDL_PointInFRect", "SDL_PointInRectFloat"},
    {"SDL_RectEquals", "SDL_RectsEqual"},
    {"SDL_UnionFRect", "SDL_GetRectUnionFloat"},
    {"SDL_UnionRect", "SDL_GetRectUnion"},
};

class SDL3RectCheck : public ClangTidyCheck {
public:
  SDL3RectCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : RectFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : RectFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3SurfaceCheck  (SDL_surface.h)
// ===========================================================================
static const char *SurfaceFuncRenames[][2] = {
    {"SDL_BlitScaled", "SDL_BlitSurfaceScaled"},
    {"SDL_ConvertSurfaceFormat", "SDL_ConvertSurface"},
    {"SDL_FillRect", "SDL_FillSurfaceRect"},
    {"SDL_FillRects", "SDL_FillSurfaceRects"},
    {"SDL_FreeSurface", "SDL_DestroySurface"},
    {"SDL_GetClipRect", "SDL_GetSurfaceClipRect"},
    {"SDL_GetColorKey", "SDL_GetSurfaceColorKey"},
    {"SDL_HasColorKey", "SDL_SurfaceHasColorKey"},
    {"SDL_HasSurfaceRLE", "SDL_SurfaceHasRLE"},
    {"SDL_LoadBMP_RW", "SDL_LoadBMP_IO"},
    {"SDL_LowerBlit", "SDL_BlitSurfaceUnchecked"},
    {"SDL_LowerBlitScaled", "SDL_BlitSurfaceUncheckedScaled"},
    {"SDL_SaveBMP_RW", "SDL_SaveBMP_IO"},
    {"SDL_SetClipRect", "SDL_SetSurfaceClipRect"},
    {"SDL_SetColorKey", "SDL_SetSurfaceColorKey"},
    {"SDL_UpperBlit", "SDL_BlitSurface"},
    {"SDL_UpperBlitScaled", "SDL_BlitSurfaceScaled"},
};

static const char *SurfaceRemovedFuncs[] = {
    "SDL_FreeFormat",
    "SDL_GetYUVConversionMode",
    "SDL_GetYUVConversionModeForResolution",
    "SDL_SetYUVConversionMode",
    "SDL_SoftStretch",
    "SDL_SoftStretchLinear",
};

class SDL3SurfaceCheck : public ClangTidyCheck {
public:
  SDL3SurfaceCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : SurfaceFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);

    for (const auto &Removed : SurfaceRemovedFuncs)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Removed)))).bind(Removed), this);

    // SDL_CreateRGBSurface() and SDL_CreateRGBSurfaceWithFormat() ->
    // SDL_CreateSurface()
    Finder->addMatcher(
        callExpr(callee(functionDecl(hasName("SDL_CreateRGBSurface"))))
            .bind("sdl_create_rgb_surface"),
        this);
    Finder->addMatcher(callExpr(callee(functionDecl(
                                    hasName("SDL_CreateRGBSurfaceWithFormat"))))
                           .bind("sdl_create_rgb_surface_fmt"),
                       this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : SurfaceFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
    for (const auto &Removed : SurfaceRemovedFuncs) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Removed)) {
        diag(Call->getBeginLoc(), "%0() has been removed in SDL3") << Removed;
        return;
      }
    }
    if (Result.Nodes.getNodeAs<CallExpr>("sdl_create_rgb_surface")) {
      const auto *Call =
          Result.Nodes.getNodeAs<CallExpr>("sdl_create_rgb_surface");
      diag(Call->getBeginLoc(),
           "SDL_CreateRGBSurface() has been replaced by SDL_CreateSurface() "
           "in SDL3; use SDL_GetPixelFormatForMasks() to convert masks to a "
           "pixel format");
      return;
    }
    if (Result.Nodes.getNodeAs<CallExpr>("sdl_create_rgb_surface_fmt")) {
      const auto *Call =
          Result.Nodes.getNodeAs<CallExpr>("sdl_create_rgb_surface_fmt");
      diag(Call->getBeginLoc(),
           "SDL_CreateRGBSurfaceWithFormat() has been replaced by "
           "SDL_CreateSurface() in SDL3")
          << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                          "SDL_CreateSurface");
      return;
    }
  }
};

// ===========================================================================
// SDL3IOStreamCheck  (SDL_rwops.h -> SDL_iostream.h)
// ===========================================================================
static const char *IOStreamFuncRenames[][2] = {
    {"SDL_RWFromConstMem", "SDL_IOFromConstMem"},
    {"SDL_RWFromFile", "SDL_IOFromFile"},
    {"SDL_RWFromMem", "SDL_IOFromMem"},
    {"SDL_RWclose", "SDL_CloseIO"},
    {"SDL_RWread", "SDL_ReadIO"},
    {"SDL_RWseek", "SDL_SeekIO"},
    {"SDL_RWsize", "SDL_GetIOSize"},
    {"SDL_RWtell", "SDL_TellIO"},
    {"SDL_RWwrite", "SDL_WriteIO"},
    {"SDL_ReadBE16", "SDL_ReadU16BE"},
    {"SDL_ReadBE32", "SDL_ReadU32BE"},
    {"SDL_ReadBE64", "SDL_ReadU64BE"},
    {"SDL_ReadLE16", "SDL_ReadU16LE"},
    {"SDL_ReadLE32", "SDL_ReadU32LE"},
    {"SDL_ReadLE64", "SDL_ReadU64LE"},
    {"SDL_WriteBE16", "SDL_WriteU16BE"},
    {"SDL_WriteBE32", "SDL_WriteU32BE"},
    {"SDL_WriteBE64", "SDL_WriteU64BE"},
    {"SDL_WriteLE16", "SDL_WriteU16LE"},
    {"SDL_WriteLE32", "SDL_WriteU32LE"},
    {"SDL_WriteLE64", "SDL_WriteU64LE"},
};

static const char *IOStreamRemovedFuncs[] = {
    "SDL_AllocRW",
    "SDL_FreeRW",
    "SDL_RWFromFP",
};

static const char *IOStreamSymbolRenames[][2] = {
    {"RW_SEEK_CUR", "SDL_IO_SEEK_CUR"},
    {"RW_SEEK_END", "SDL_IO_SEEK_END"},
    {"RW_SEEK_SET", "SDL_IO_SEEK_SET"},
};

class SDL3IOStreamCheck : public ClangTidyCheck {
public:
  SDL3IOStreamCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : IOStreamFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);

    for (const auto &Removed : IOStreamRemovedFuncs)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Removed)))).bind(Removed), this);

    for (const auto &S : IOStreamSymbolRenames)
      Finder->addMatcher(declRefExpr(to(namedDecl(hasName(S[0])))).bind(S[0]),
                         this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : IOStreamFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
    for (const auto &Removed : IOStreamRemovedFuncs) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Removed)) {
        if (StringRef(Removed) == "SDL_RWFromFP") {
          diag(Call->getBeginLoc(),
               "SDL_RWFromFP() has been removed; implement a custom "
               "SDL_IOStream using SDL_OpenIO() instead");
        } else {
          diag(Call->getBeginLoc(), "%0() has been removed in SDL3") << Removed;
        }
        return;
      }
    }
    for (const auto &S : IOStreamSymbolRenames) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(S[0])) {
        diag(DRE->getBeginLoc(), "%0 has been renamed to %1 in SDL3")
            << S[0] << S[1]
            << FixItHint::CreateReplacement(DRE->getSourceRange(), S[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3LogCheck  (SDL_log.h)
// ===========================================================================
static const char *LogFuncRenames[][2] = {
    {"SDL_LogGetOutputFunction", "SDL_GetLogOutputFunction"},
    {"SDL_LogGetPriority", "SDL_GetLogPriority"},
    {"SDL_LogResetPriorities", "SDL_ResetLogPriorities"},
    {"SDL_LogSetAllPriority", "SDL_SetLogPriorities"},
    {"SDL_LogSetOutputFunction", "SDL_SetLogOutputFunction"},
    {"SDL_LogSetPriority", "SDL_SetLogPriority"},
};

static const char *LogSymbolRenames[][2] = {
    {"SDL_NUM_LOG_PRIORITIES", "SDL_LOG_PRIORITY_COUNT"},
};

class SDL3LogCheck : public ClangTidyCheck {
public:
  SDL3LogCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : LogFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);

    for (const auto &S : LogSymbolRenames)
      Finder->addMatcher(declRefExpr(to(namedDecl(hasName(S[0])))).bind(S[0]),
                         this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : LogFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
    for (const auto &S : LogSymbolRenames) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(S[0])) {
        diag(DRE->getBeginLoc(), "%0 has been renamed to %1 in SDL3")
            << S[0] << S[1]
            << FixItHint::CreateReplacement(DRE->getSourceRange(), S[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3PixelsCheck  (SDL_pixels.h)
// ===========================================================================
static const char *PixelsFuncRenames[][2] = {
    {"SDL_AllocFormat", "SDL_GetPixelFormatDetails"},
    {"SDL_AllocPalette", "SDL_CreatePalette"},
    {"SDL_FreePalette", "SDL_DestroyPalette"},
    {"SDL_MasksToPixelFormatEnum", "SDL_GetPixelFormatForMasks"},
    {"SDL_PixelFormatEnumToMasks", "SDL_GetMasksForPixelFormat"},
};

static const char *PixelsRemovedFuncs[] = {
    "SDL_FreeFormat",
    "SDL_SetPixelFormatPalette",
    "SDL_CalculateGammaRamp",
};

static const char *PixelsSymbolRenames[][2] = {
    {"SDL_PIXELFORMAT_BGR444", "SDL_PIXELFORMAT_XBGR4444"},
    {"SDL_PIXELFORMAT_BGR555", "SDL_PIXELFORMAT_XBGR1555"},
    {"SDL_PIXELFORMAT_BGR888", "SDL_PIXELFORMAT_XBGR8888"},
    {"SDL_PIXELFORMAT_RGB444", "SDL_PIXELFORMAT_XRGB4444"},
    {"SDL_PIXELFORMAT_RGB555", "SDL_PIXELFORMAT_XRGB1555"},
    {"SDL_PIXELFORMAT_RGB888", "SDL_PIXELFORMAT_XRGB8888"},
};

class SDL3PixelsCheck : public ClangTidyCheck {
public:
  SDL3PixelsCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override {
    PP->addPPCallbacks(::std::make_unique<SDLIncludeCallback>(*this, SM));
  }

  void registerMatchers(ast_matchers::MatchFinder *Finder) override {
    for (const auto &R : PixelsFuncRenames)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(R[0])))).bind(R[0]), this);

    for (const auto &Removed : PixelsRemovedFuncs)
      Finder->addMatcher(
          callExpr(callee(functionDecl(hasName(Removed)))).bind(Removed), this);

    for (const auto &S : PixelsSymbolRenames)
      Finder->addMatcher(declRefExpr(to(namedDecl(hasName(S[0])))).bind(S[0]),
                         this);
  }

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override {
    for (const auto &R : PixelsFuncRenames) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(R[0])) {
        diag(Call->getBeginLoc(), "%0() has been renamed to %1() in SDL3")
            << R[0] << R[1]
            << FixItHint::CreateReplacement(Call->getCallee()->getSourceRange(),
                                            R[1]);
        return;
      }
    }
    for (const auto &Removed : PixelsRemovedFuncs) {
      if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>(Removed)) {
        diag(Call->getBeginLoc(), "%0() has been removed in SDL3") << Removed;
        return;
      }
    }
    for (const auto &S : PixelsSymbolRenames) {
      if (const auto *DRE = Result.Nodes.getNodeAs<DeclRefExpr>(S[0])) {
        diag(DRE->getBeginLoc(), "%0 has been renamed to %1 in SDL3")
            << S[0] << S[1]
            << FixItHint::CreateReplacement(DRE->getSourceRange(), S[1]);
        return;
      }
    }
  }
};

// ===========================================================================
// SDL3MigrationModule  - registers all checks
// ===========================================================================
class SDL3MigrationModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    // SDL_init.h + error-checking patterns + endian + cpuinfo + events
    CheckFactories.registerCheck<SDL3InitCheck>("sdl3-migration-init");
    // SDL_audio.h
    CheckFactories.registerCheck<SDL3AudioCheck>("sdl3-migration-audio");
    // SDL_atomic.h
    CheckFactories.registerCheck<SDL3AtomicCheck>("sdl3-migration-atomic");
    // SDL_gamecontroller.h -> SDL_gamepad.h
    CheckFactories.registerCheck<SDL3GamepadCheck>("sdl3-migration-gamepad");
    // SDL_joystick.h
    CheckFactories.registerCheck<SDL3JoystickCheck>("sdl3-migration-joystick");
    // SDL_haptic.h
    CheckFactories.registerCheck<SDL3HapticCheck>("sdl3-migration-haptic");
    // SDL_mouse.h
    CheckFactories.registerCheck<SDL3MouseCheck>("sdl3-migration-mouse");
    // SDL_render.h
    CheckFactories.registerCheck<SDL3RenderCheck>("sdl3-migration-render");
    // SDL_mutex.h
    CheckFactories.registerCheck<SDL3MutexCheck>("sdl3-migration-mutex");
    // SDL_rect.h
    CheckFactories.registerCheck<SDL3RectCheck>("sdl3-migration-rect");
    // SDL_surface.h
    CheckFactories.registerCheck<SDL3SurfaceCheck>("sdl3-migration-surface");
    // SDL_rwops.h / SDL_iostream.h
    CheckFactories.registerCheck<SDL3IOStreamCheck>("sdl3-migration-iostream");
    // SDL_log.h
    CheckFactories.registerCheck<SDL3LogCheck>("sdl3-migration-log");
    // SDL_pixels.h
    CheckFactories.registerCheck<SDL3PixelsCheck>("sdl3-migration-pixels");
  }
};

// ---------------------------------------------------------------------------
// Plugin registration
// ---------------------------------------------------------------------------
namespace clang {
namespace tidy {

static ClangTidyModuleRegistry::Add<SDL3MigrationModule>
    X("sdl3-migration-module", "Adds SDL3 migration checks.");

volatile int SDL3MigrationModuleAnchorSource = 0;

} // namespace tidy
} // namespace clang

extern "C" {
ClangTidyModule *createClangTidyModule() { return new SDL3MigrationModule(); }
}
