// Raven NLE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "imgui.h"

#include "widgets.h"
#include "imguihelper.h"
#include "imgui_plot.h"
#include "imguifilesystem.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#ifdef HELLOIMGUI_USE_SDL_OPENGL3
#include <SDL.h>
#endif

#include <iostream>

void DrawButtons(ImVec2 buttonSize);

#include "app.h"
#include "timeline.h"

const char *app_name = "Raven";

AppState appState;

ImFont *gTechFont = nullptr;
ImFont *gIconFont = nullptr;

// Log a message to the terminal
void Log(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  fprintf(stderr, "LOG: ");
  fprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

// Display a message in the GUI (and to the terminal)
void Message(const char* format, ...)
{
  va_list args;
  va_start(args, format);
  vsnprintf(appState.message, sizeof(appState.message), format, args);
  va_end(args);
  Log(appState.message);
}

// Files in the application fonts/ folder are supposed to be embedded
// automatically (on iOS/Android/Emscripten), but that's not wired up.
void LoadFonts()
{
  ImGuiIO& io = ImGui::GetIO();

  // TODO: Use ImGuiFontStudio to bundle these fonts into the executable?
#ifdef EMSCRIPTEN
  Log("Skipping font loading on EMSCRIPTEN platform.");
  gTechFont = io.Fonts->AddFontDefault();
  gIconFont = gTechFont;
#else
  gTechFont = io.Fonts->AddFontFromFileTTF("fonts/ShareTechMono-Regular.ttf", 20.0f);
  static const ImWchar icon_fa_ranges[] = { 0xF000, 0xF18B, 0 };
  gIconFont = io.Fonts->AddFontFromFileTTF("fonts/fontawesome-webfont.ttf", 16.0f, NULL, icon_fa_ranges);
#endif
}

void ApplyAppStyle()
{
  // ImGuiStyle& style = ImGui::GetStyle();

  ImGui::StyleColorsDark();
    
/*
  style.Alpha = 1.0;
  style.WindowRounding = 3;
  style.GrabRounding = 1;
  style.GrabMinSize = 20;
  style.FrameRounding = 3;
  style.WindowBorderSize = 0;
  style.ChildBorderSize = 0;
  style.FrameBorderSize = 1;

  // Based on this theme by enemymouse:
  // https://github.com/ocornut/imgui/issues/539#issuecomment-204412632
  // https://gist.github.com/enemymouse/c8aa24e247a1d7b9fc33d45091cbb8f0
  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text]                   = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_TextDisabled]           = ImVec4(0.00f, 0.40f, 0.41f, 1.00f);
  colors[ImGuiCol_WindowBg]               = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
  colors[ImGuiCol_Border]                 = ImVec4(0.00f, 1.00f, 1.00f, 0.65f);
  colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_FrameBg]                = ImVec4(0.44f, 0.80f, 0.80f, 0.18f);
  colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.44f, 0.80f, 0.80f, 0.27f);
  colors[ImGuiCol_FrameBgActive]          = ImVec4(0.44f, 0.81f, 0.86f, 0.66f);
  colors[ImGuiCol_TitleBg]                = ImVec4(0.14f, 0.18f, 0.21f, 0.78f);
  colors[ImGuiCol_TitleBgActive]          = ImVec4(0.00f, 0.54f, 0.55f, 0.78f);
  colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.78f);
  colors[ImGuiCol_MenuBarBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
  colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.22f, 0.29f, 0.30f, 0.71f);
  colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.00f, 1.00f, 1.00f, 0.44f);
  colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
  colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.80f, 0.99f, 0.99f, 1.00f);
  colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 1.00f, 1.00f, 0.68f);
  colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 1.00f, 1.00f, 0.36f);
  colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.80f, 0.99f, 0.99f, 1.00f);
  colors[ImGuiCol_Button]                 = ImVec4(0.00f, 0.65f, 0.65f, 0.46f);
  colors[ImGuiCol_ButtonHovered]          = ImVec4(0.01f, 1.00f, 1.00f, 0.43f);
  colors[ImGuiCol_ButtonActive]           = ImVec4(0.80f, 0.99f, 0.99f, 1.00f);
  colors[ImGuiCol_Header]                 = ImVec4(0.00f, 1.00f, 1.00f, 0.33f);
  colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 1.00f, 1.00f, 0.42f);
  colors[ImGuiCol_HeaderActive]           = ImVec4(1.00f, 1.00f, 1.00f, 0.54f);
  colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
  colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
  colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
  colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
  colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
  colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.80f, 0.99f, 0.99f, 1.00f);
  colors[ImGuiCol_Tab]                    = ImVec4(0.12f, 0.31f, 0.31f, 1.00f);
  colors[ImGuiCol_TabHovered]             = ImVec4(0.80f, 0.99f, 0.99f, 1.00f);
  colors[ImGuiCol_TabActive]              = ImVec4(0.00f, 0.62f, 0.62f, 1.00f);
  colors[ImGuiCol_TabUnfocused]           = ImVec4(0.08f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.43f, 0.43f, 1.00f);
  colors[ImGuiCol_DockingPreview]         = ImVec4(0.80f, 0.99f, 0.99f, 1.00f);
  colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_PlotLines]              = ImVec4(0.80f, 0.99f, 0.99f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_PlotHistogram]          = ImVec4(0.80f, 0.99f, 0.99f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.00f, 1.00f, 1.00f, 0.22f);
  colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
  colors[ImGuiCol_NavHighlight]           = ImVec4(0.94f, 0.98f, 0.26f, 1.00f);
  colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.04f, 0.10f, 0.09f, 0.51f);
  */
}

std::string otio_error_string(otio::ErrorStatus const& error_status)
{
    return otio::ErrorStatus::outcome_to_string(error_status.outcome) + ": " +
        error_status.details;
}

void LoadFile(const char* path)
{
  std::string input(path);
  otio::ErrorStatus error_status;
  auto timeline = dynamic_cast<otio::Timeline*>(otio::Timeline::from_json_file(input, &error_status));
  if (!timeline || otio::is_error(error_status)) {
    Message("Error loading %s: %s", path, otio_error_string(error_status).c_str());
    return;
  }
  appState.timeline = timeline;
  strncpy(appState.file_path, path, sizeof(appState.file_path));
  Message("Loaded %s", timeline->name().c_str());
}

void MainInit(int argc, char** argv)
{
  ApplyAppStyle();
  LoadFonts();
  
  if (argc > 1) {
    LoadFile(argv[1]);
  }
}

void MainCleanup()
{
}

// Make a button using the fancy icon font
bool IconButton(const char* label, const ImVec2 size=ImVec2(0,0))
{
  ImGui::PushFont(gIconFont);
  bool result = ImGui::Button(label, size);
  ImGui::PopFont();
  return result;
}

ImU32 ImLerpColors(ImU32 col_a, ImU32 col_b, float t)
{
    int r = ImLerp((int)(col_a >> IM_COL32_R_SHIFT) & 0xFF, (int)(col_b >> IM_COL32_R_SHIFT) & 0xFF, t);
    int g = ImLerp((int)(col_a >> IM_COL32_G_SHIFT) & 0xFF, (int)(col_b >> IM_COL32_G_SHIFT) & 0xFF, t);
    int b = ImLerp((int)(col_a >> IM_COL32_B_SHIFT) & 0xFF, (int)(col_b >> IM_COL32_B_SHIFT) & 0xFF, t);
    int a = ImLerp((int)(col_a >> IM_COL32_A_SHIFT) & 0xFF, (int)(col_b >> IM_COL32_A_SHIFT) & 0xFF, t);
    return IM_COL32(r, g, b, a);
}

void AppUpdate()
{
}

static char buffer[256];
const char* TimecodeString(float t) {

  auto str = otio::RationalTime::from_seconds(t).to_timecode();
  
  // // float fraction = t - floor(t);
  // t = floor(t);
  // int seconds = fmodf(t, 60.0);
  // int minutes = fmodf(t/60.0, 60.0);
  // int hours = floor(t/3600.0);

  // snprintf(
  //   buffer, sizeof(buffer),
  //   "%d:%02d:%02d",
  //   hours, minutes, seconds); //, (int)(fraction*100.0));

  snprintf(buffer, sizeof(buffer), "%s", str.c_str());
  
  return buffer;
}

void MainGui()
{
  AppUpdate();

  ImGuiIO& io = ImGui::GetIO();
  ImVec2 displaySize = io.DisplaySize;
  if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
    ImGui::SetNextWindowSize(displaySize, ImGuiCond_FirstUseEver);
  }else{
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(displaySize);
  }

  const char *window_id = "###MainWindow";
  char window_title[1024];
  char filename[ImGuiFs::MAX_FILENAME_BYTES] = {""};
  ImGuiFs::PathGetFileName(appState.file_path, filename);
  if (strlen(filename)) {
    snprintf(window_title, sizeof(window_title), "%s - %s%s", app_name, filename, window_id);
  }else{
    snprintf(window_title, sizeof(window_title), "%s%s", app_name, window_id);
  }

  ImGui::Begin(
      window_title,
      &appState.show_main_window,
      ImGuiWindowFlags_NoCollapse |
      // ImGuiWindowFlags_NoDocking |
      // ImGuiWindowFlags_AlwaysAutoResize |
      0
      );

  if (!appState.show_main_window) {
    MainCleanup();
    exit(0);
  }

  ImVec2 button_size = ImVec2(
    ImGui::GetTextLineHeightWithSpacing(),
    ImGui::GetTextLineHeightWithSpacing()
    );

  ImGui::SameLine();
  DrawButtons(button_size);

  // ImGui::SameLine(ImGui::GetContentRegionAvailWidth() - button_size.x + style.ItemSpacing.x);

  ImVec2 contentSize = ImGui::GetContentRegionAvail();
  if (contentSize.y < 500) contentSize.y = 500;

  // float splitter_size = 2.0f;
  // float w = contentSize.x - splitter_size - style.WindowPadding.x * 2;
  // float h = contentSize.y - style.WindowPadding.y * 2;
  // static float sz1 = 0;
  // static float sz2 = 0;
  // if (sz1 + sz2 != w) {
  //   float delta = (sz1 + sz2) - w;
  //   sz1 -= delta / 2;
  //   sz2 -= delta / 2;
  // }
  // Splitter(true, splitter_size, &sz1, &sz2, 8, 8, h, 8);
  // ImGui::BeginChild("1", ImVec2(sz1, h), true);

  // DrawAudioPanel();

  // ImGui::EndChild();
  // ImGui::SameLine();
  // ImGui::BeginChild("2", ImVec2(sz2, h), true);

  ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()*2)); // Leave room for 1 line below us
  
  if (ImGui::BeginTabBar("MyTabBar", ImGuiTabBarFlags_None))
  {
    if (ImGui::BeginTabItem("Timeline"))
    {
      DrawTimeline(appState.timeline);

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Selection"))
    {
      char buf[10000];
      snprintf(buf, sizeof(buf), "%s", appState.selected_text.c_str());
      ImGui::InputTextMultiline("Selection", buf, sizeof(buf), ImVec2(-FLT_MIN, -FLT_MIN), 0);

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Settings"))
    {
      ImGui::ShowStyleEditor();

      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  
  ImGui::EndChild();

  ImGui::Separator();

  if (DrawTransportControls(appState.timeline)) {
      appState.scroll_to_playhead = true;
  }

  // Status message at the very bottom
  ImGui::Text("%s", appState.message);

  ImGui::End();
  
  if (appState.show_demo_window) {
    ImGui::ShowDemoWindow();
  }
}


void DrawButtons(ImVec2 button_size)
{
  // ImGuiStyle& style = ImGui::GetStyle();

  const bool browseButtonPressed = IconButton("\uF07C##Load", button_size);                          // we need a trigger boolean variable
  static ImGuiFs::Dialog dlg;
  // ImGui::SetNextWindowPos(ImVec2(300,300));
  const char* chosenPath = dlg.chooseFileDialog(
    browseButtonPressed,
    dlg.getLastDirectory(),
    ".otio",
    "Load OTIO File"
  );
  if (strlen(chosenPath)>0) {
    LoadFile(chosenPath);
  }
  ImGui::SameLine();

  // if (IconButton("\uF074##NodeGraph", button_size)) {
  //   appState.show_node_graph = !appState.show_node_graph;
  // }
  // ImGui::SameLine();

  // if (IconButton("\uF0AE##Style", button_size)) {
  //   appState.show_style_editor = !appState.show_style_editor;
  // }
  // ImGui::SameLine();

  if (IconButton("\uF013#Demo", button_size)) {
    appState.show_demo_window = !appState.show_demo_window;
  }
  
  ImGui::SameLine();
  if (ImGui::Checkbox("Snap to Frame", &appState.snap_to_frame)) {
    if (appState.snap_to_frame) {
        appState.playhead = otio::RationalTime::from_frames(appState.playhead.to_frames(), appState.playhead.rate());
    }
  }
}
