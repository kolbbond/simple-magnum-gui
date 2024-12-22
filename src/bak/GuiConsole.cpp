// conversion of the demo app console to a class

#include "GuiConsole.hh"

// Helper to wire demo markers located in code to an interactive browser
typedef void (*ImGuiConsoleMarkerCallback)(const char *file, int line,
                                           const char *section,
                                           void *user_data);
extern ImGuiConsoleMarkerCallback GImGuiConsoleMarkerCallback;
extern void *GImGuiConsoleMarkerCallbackUserData;
ImGuiConsoleMarkerCallback GImGuiConsoleMarkerCallback = NULL;
void *GImGuiConsoleMarkerCallbackUserData = NULL;
#define IMGUI_CONSOLE_MARKER(section)                                          \
  do {                                                                         \
    if (GImGuiConsoleMarkerCallback != NULL)                                   \
      GImGuiConsoleMarkerCallback(__FILE__, __LINE__, section,                 \
                                  GImGuiConsoleMarkerCallbackUserData);        \
  } while (0)

GuiConsole::GuiConsole() {
  IMGUI_CONSOLE_MARKER("GuiConsole");
  ClearLog();
  memset(InputBuf_, 0, sizeof(InputBuf_));
  HistoryPos_ = -1;

  // "CLASSIFY" is here to provide the test case where "C"+[tab] completes to
  // "CL" and display multiple matches.
  Commands_.push_back("HELP");
  Commands_.push_back("HISTORY");
  Commands_.push_back("CLEAR");
  Commands_.push_back("CLASSIFY");
  Commands_.push_back("lua");
  Commands_.push_back("matlab");

  AutoScroll_ = true;
  ScrollToBottom_ = false;
  AddLog("welcome to example gui console");
}

// destructor
GuiConsole::~GuiConsole() {
  ClearLog();
  for (int i = 0; i < History_.Size; i++)
    ImGui::MemFree(History_[i]);
}

void GuiConsole::ClearLog() {
  for (int i = 0; i < Items_.Size; i++)
    ImGui::MemFree(Items_[i]);
  Items_.clear();
}

// void GuiConsole::AddLog(const char *fmt, ...) IM_FMTARGS(2) {
void GuiConsole::AddLog(const char *fmt, ...) {
  // FIXME-OPT
  char buf[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
  buf[IM_ARRAYSIZE(buf) - 1] = 0;
  va_end(args);
  Items_.push_back(Strdup(buf));
}

void GuiConsole::Draw(const char *title, bool *p_open) {
  ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(title, p_open)) {
    ImGui::End();
    return;
  }

  // As a specific feature guaranteed by the library, after calling Begin()
  // the last Item represent the title bar. So e.g. IsItemHovered() will
  // return true when hovering the title bar. Here we create a context menu
  // only available from the title bar.
  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Close Console"))
      *p_open = false;
    ImGui::EndPopup();
  }

  ImGui::TextWrapped(
      "This example implements a console with basic coloring, completion "
      "(TAB key) and history (Up/Down keys). A more elaborate "
      "implementation may want to store entries along with extra data such "
      "as timestamp, emitter, etc.");
  ImGui::TextWrapped("Enter 'HELP' for help.");

  // TODO: display items starting from the bottom

  if (ImGui::SmallButton("Add Debug Text")) {
    AddLog("%d some text", Items_.Size);
    AddLog("some more text");
    AddLog("display very important message here!");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Add Debug Error")) {
    AddLog("[error] something went wrong");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton("Clear")) {
    ClearLog();
  }
  ImGui::SameLine();
  bool copy_to_clipboard = ImGui::SmallButton("Copy");
  // static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t =
  // ImGui::GetTime(); AddLog("Spam %f", t); }

  ImGui::Separator();

  // Options menu
  if (ImGui::BeginPopup("Options")) {
    ImGui::Checkbox("Auto-scroll", &AutoScroll_);
    ImGui::EndPopup();
  }

  // Options, Filter
  if (ImGui::Button("Options"))
    ImGui::OpenPopup("Options");
  ImGui::SameLine();
  Filter_.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
  ImGui::Separator();

  // Reserve enough left-over height for 1 separator + 1 input text
  const float footer_height_to_reserve =
      ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
  if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve),
                        ImGuiChildFlags_None,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    if (ImGui::BeginPopupContextWindow()) {
      if (ImGui::Selectable("Clear"))
        ClearLog();
      ImGui::EndPopup();
    }

    // Display every line as a separate entry so we can change their color or
    // add custom widgets. If you only want raw text you can use
    // ImGui::TextUnformatted(log.begin(), log.end()); NB- if you have
    // thousands of entries this approach may be too inefficient and may
    // require user-side clipping to only process visible items. The clipper
    // will automatically measure the height of your first item and then
    // "seek" to display only items in the visible area.
    // To use the clipper we can replace your standard loop:
    //      for (int i = 0; i < Items.Size; i++)
    //   With:
    //      ImGuiListClipper clipper;
    //      clipper.Begin(Items.Size);
    //      while (clipper.Step())
    //         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
    // - That your items are evenly spaced (same height)
    // - That you have cheap random access to your elements (you can access
    // them given their index,
    //   without processing all the ones before)
    // You cannot this code as-is if a filter is active because it breaks the
    // 'cheap random-access' property. We would need random-access on the
    // post-filtered list. A typical application wanting coarse clipping and
    // filtering may want to pre-compute an array of indices or offsets of
    // items that passed the filtering test, recomputing this array when user
    // changes the filter, and appending newly elements as they are inserted.
    // This is left as a task to the user until we can manage to improve this
    // example code! If your items are of variable height:
    // - Split them into same height items would be simpler and facilitate
    // random-seeking into your list.
    // - Consider using manual call to IsRectVisible() and skipping extraneous
    // decoration from your items.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(4, 1)); // Tighten spacing
    if (copy_to_clipboard)
      ImGui::LogToClipboard();
    for (const char *item : Items_) {
      if (!Filter_.PassFilter(item))
        continue;

      // Normally you would store more information in your item than just a
      // string. (e.g. make Items[] an array of structure, store color/type
      // etc.)
      ImVec4 color;
      bool has_color = false;
      if (strstr(item, "[error]")) {
        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        has_color = true;
      } else if (strncmp(item, "# ", 2) == 0) {
        color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f);
        has_color = true;
      }
      if (has_color)
        ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::TextUnformatted(item);
      if (has_color)
        ImGui::PopStyleColor();
    }
    if (copy_to_clipboard)
      ImGui::LogFinish();

    // Keep up at the bottom of the scroll region if we were already at the
    // bottom at the beginning of the frame. Using a scrollbar or mouse-wheel
    // will take away from the bottom edge.
    if (ScrollToBottom_ ||
        (AutoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
      ImGui::SetScrollHereY(1.0f);
    ScrollToBottom_ = false;

    ImGui::PopStyleVar();
  }
  ImGui::EndChild();
  ImGui::Separator();

  // Command-line
  bool reclaim_focus = false;
  ImGuiInputTextFlags input_text_flags =
      ImGuiInputTextFlags_EnterReturnsTrue |
      ImGuiInputTextFlags_EscapeClearsAll |
      ImGuiInputTextFlags_CallbackCompletion |
      ImGuiInputTextFlags_CallbackHistory;
  if (ImGui::InputText("Input", InputBuf_, IM_ARRAYSIZE(InputBuf_),
                       input_text_flags, &TextEditCallbackStub, (void *)this)) {
    char *s = InputBuf_;
    Strtrim(s);
    if (s[0])
      ExecCommand(s);
    strcpy(s, "");
    reclaim_focus = true;
  }

  // Auto-focus on window apparition
  ImGui::SetItemDefaultFocus();
  if (reclaim_focus)
    ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

  ImGui::End();
}

int GuiConsole::start_lua() {
  // start the lua interpreter
  lua_ = LuaWrapper::create();

  return 1;
}

void GuiConsole::forward_command(const char *command) {
  // forward the command to the interpreter

  // check both are not set
  assert(!(flag_lua_console_ == 1 && flag_matlab_console_ == 1));

  // get interpreter
  if (flag_lua_console_) {

    lua_->dostring(command);

  } else if (flag_matlab_console_) {

  } else {
    printf("why are we forwarding commands?\n");
  }
}

void GuiConsole::ExecCommand(const char *command_line) {
  AddLog("# %s\n", command_line);

  // if interpreter flag set then ...
  // we need to override this or ...

  // Insert into history. First find match and delete it so it can be pushed
  // to the back. This isn't trying to be smart or optimal.
  HistoryPos_ = -1;
  for (int i = History_.Size - 1; i >= 0; i--)
    if (Stricmp(History_[i], command_line) == 0) {
      ImGui::MemFree(History_[i]);
      History_.erase(History_.begin() + i);
      break;
    }
  History_.push_back(Strdup(command_line));

  // intercept
  if (flag_lua_console_) {
    forward_command(command_line);

  } else {

    // Process command
    if (Stricmp(command_line, "CLEAR") == 0) {
      ClearLog();

    } else if (Stricmp(command_line, "HELP") == 0) {
      AddLog("Commands:");
      for (int i = 0; i < Commands_.Size; i++)
        AddLog("- %s", Commands_[i]);

    } else if (Stricmp(command_line, "HISTORY") == 0) {
      int first = History_.Size - 10;
      for (int i = first > 0 ? first : 0; i < History_.Size; i++) {
        AddLog("%3d: %s\n", i, History_[i]);
      }

      // lua interpreter start
    } else if (Stricmp(command_line, "lua") == 0) {
      AddLog("Start Lua interpreter\n");
      // need to check if compiled with lua...
      // @hey how do we add header definitions ...
      flag_lua_console_ = true;

      // check if success
      if (!start_lua()) {
        AddLog("lua failed\n");
        flag_lua_console_ = false;
      }

      // matlab interpreter start
    } else if (Stricmp(command_line, "matlab") == 0) {
      AddLog("Start Matlab interpreter\n");
      // need to check if compiled with matlab ...
      flag_matlab_console_ = true;

      // otherwise
    } else {
      AddLog("Unknown command: '%s'\n", command_line);
    }
  }

  // On command input, we scroll to bottom even if AutoScroll==false
  ScrollToBottom_ = true;
}

// In C++11 you'd be better off using lambdas for this sort of forwarding
// callbacks
int GuiConsole::TextEditCallbackStub(ImGuiInputTextCallbackData *data) {
  GuiConsole *console = (GuiConsole *)data->UserData;
  return console->TextEditCallback(data);
}

int GuiConsole::TextEditCallback(ImGuiInputTextCallbackData *data) {
  // AddLog("cursor: %d, selection: %d-%d", data->CursorPos,
  // data->SelectionStart, data->SelectionEnd);
  switch (data->EventFlag) {
  case ImGuiInputTextFlags_CallbackCompletion: {
    // Example of TEXT COMPLETION

    // Locate beginning of current word
    const char *word_end = data->Buf + data->CursorPos;
    const char *word_start = word_end;
    while (word_start > data->Buf) {
      const char c = word_start[-1];
      if (c == ' ' || c == '\t' || c == ',' || c == ';')
        break;
      word_start--;
    }

    // Build a list of candidates
    ImVector<const char *> candidates;
    for (int i = 0; i < Commands_.Size; i++)
      if (Strnicmp(Commands_[i], word_start, (int)(word_end - word_start)) == 0)
        candidates.push_back(Commands_[i]);

    if (candidates.Size == 0) {
      // No match
      AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start),
             word_start);
    } else if (candidates.Size == 1) {
      // Single match. Delete the beginning of the word and replace it
      // entirely so we've got nice casing.
      data->DeleteChars((int)(word_start - data->Buf),
                        (int)(word_end - word_start));
      data->InsertChars(data->CursorPos, candidates[0]);
      data->InsertChars(data->CursorPos, " ");
    } else {
      // Multiple matches. Complete as much as we can..
      // So inputing "C"+Tab will complete to "CL" then display "CLEAR" and
      // "CLASSIFY" as matches.
      int match_len = (int)(word_end - word_start);
      for (;;) {
        int c = 0;
        bool all_candidates_matches = true;
        for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
          if (i == 0)
            c = toupper(candidates[i][match_len]);
          else if (c == 0 || c != toupper(candidates[i][match_len]))
            all_candidates_matches = false;
        if (!all_candidates_matches)
          break;
        match_len++;
      }

      if (match_len > 0) {
        data->DeleteChars((int)(word_start - data->Buf),
                          (int)(word_end - word_start));
        data->InsertChars(data->CursorPos, candidates[0],
                          candidates[0] + match_len);
      }

      // List matches
      AddLog("Possible matches:\n");
      for (int i = 0; i < candidates.Size; i++)
        AddLog("- %s\n", candidates[i]);
    }

    break;
  }
  case ImGuiInputTextFlags_CallbackHistory: {
    // Example of HISTORY
    const int prev_history_pos = HistoryPos_;
    if (data->EventKey == ImGuiKey_UpArrow) {
      if (HistoryPos_ == -1)
        HistoryPos_ = History_.Size - 1;
      else if (HistoryPos_ > 0)
        HistoryPos_--;
    } else if (data->EventKey == ImGuiKey_DownArrow) {
      if (HistoryPos_ != -1)
        if (++HistoryPos_ >= History_.Size)
          HistoryPos_ = -1;
    }

    // A better implementation would preserve the data on the current input
    // line along with cursor position.
    if (prev_history_pos != HistoryPos_) {
      const char *history_str = (HistoryPos_ >= 0) ? History_[HistoryPos_] : "";
      data->DeleteChars(0, data->BufTextLen);
      data->InsertChars(0, history_str);
    }
  }
  } // switch end

  return 0;
}

void GuiConsole::ShowGuiConsole(bool *p_open) {
  static GuiConsole console;
  console.Draw("Console", p_open);
}

ShGuiConsolePr GuiConsole::create() {
  // factory
  return std::make_shared<GuiConsole>();
}

// STATICS

// Portable helpers
int GuiConsole::Stricmp(const char *s1, const char *s2) {
  int d;
  while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) {
    s1++;
    s2++;
  }
  return d;
}
int GuiConsole::Strnicmp(const char *s1, const char *s2, int n) {
  int d = 0;
  while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) {
    s1++;
    s2++;
    n--;
  }
  return d;
}

char *GuiConsole::Strdup(const char *s) {
  IM_ASSERT(s);
  size_t len = strlen(s) + 1;
  void *buf = ImGui::MemAlloc(len);
  IM_ASSERT(buf);
  return (char *)memcpy(buf, (const void *)s, len);
}

void GuiConsole::Strtrim(char *s) {
  char *str_end = s + strlen(s);
  while (str_end > s && str_end[-1] == ' ')
    str_end--;
  *str_end = 0;
}
