// GuiConsole.hh

#ifndef GuiConsole_hh
#define GuiConsole_hh

#include "ImGuiWrapper.hh"
#include "ImPlotWrapper.hh"
#include "LuaWrapper.hh"
#include "WindowWrapper.hh"

typedef std::shared_ptr<class GuiConsole> ShGuiConsolePr;

class GuiConsole {

  // properties
protected:
  // these come from ImGui demos
  char InputBuf_[256];
  ImVector<char *> Items_;
  ImVector<const char *> Commands_;
  ImVector<char *> History_;
  int HistoryPos_; // -1: new line, 0..History.Size-1 browsing history.
  ImGuiTextFilter Filter_;
  bool AutoScroll_;
  bool ScrollToBottom_;

  // interpreter engines
  ShLuaWrapperPr lua_;

  bool flag_lua_console_ = false;
  bool flag_matlab_console_ = false;

public:
  GuiConsole();
  ~GuiConsole();

  int start_lua();
  void forward_command(const char *command);

  // FROM IMGUI DEMO
  // general
  void ClearLog();
  // void AddLog(const char *fmt, ...) IM_FMTARGS(2);
  void AddLog(const char *fmt, ...);
  void Draw(const char *, bool *);
  void ExecCommand(const char *);

  // factory
  static ShGuiConsolePr create();

  static int Stricmp(const char *, const char *);
  static void Strtrim(char *s);
  static char *Strdup(const char *s);
  static int Strnicmp(const char *s1, const char *s2, int n);

  static void ShowGuiConsole(bool *);
  static int TextEditCallbackStub(ImGuiInputTextCallbackData *data);

  int TextEditCallback(ImGuiInputTextCallbackData *data);
};

#endif
