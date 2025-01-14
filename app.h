

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "imgui.h"
#include "imgui_internal.h"

#include "timeline.h"

#include <opentimelineio/timeline.h>
namespace otio = opentimelineio::OPENTIMELINEIO_VERSION;

enum AppThemeCol_ {
    AppThemeCol_Background,
    AppThemeCol_Label,
    AppThemeCol_TickMajor,
    AppThemeCol_TickMinor,
    AppThemeCol_GapHovered,
    AppThemeCol_GapSelected,
    AppThemeCol_Item,
    AppThemeCol_ItemHovered,
    AppThemeCol_ItemSelected,
    AppThemeCol_Transition,
    AppThemeCol_TransitionLine,
    AppThemeCol_TransitionHovered,
    AppThemeCol_TransitionSelected,
    AppThemeCol_Effect,
    AppThemeCol_EffectHovered,
    AppThemeCol_EffectSelected,
    AppThemeCol_Playhead,
    AppThemeCol_PlayheadLine,
    AppThemeCol_PlayheadHovered,
    AppThemeCol_PlayheadSelected,
    AppThemeCol_MarkerHovered,
    AppThemeCol_MarkerSelected,
    AppThemeCol_Track,
    AppThemeCol_TrackHovered,
    AppThemeCol_TrackSelected,
    AppThemeCol_COUNT
};

extern const char* AppThemeColor_Names[];

#ifdef DEFINE_APP_THEME_NAMES
const char* AppThemeColor_Names[] = {
    "Background",
    "Label",
    "Tick Major",
    "Tick Minor",
    "Gap Hovered",
    "Gap Selected",
    "Item",
    "Item Hovered",
    "Item Selected",
    "Transition",
    "Transition Line",
    "Transition Hovered",
    "Transition Selected",
    "Effect",
    "Effect Hovered",
    "Effect Selected",
    "Playhead",
    "Playhead Line",
    "Playhead Hovered",
    "Playhead Selected",
    "Marker Hovered",
    "Marker Selected",
    "Track",
    "Track Hovered",
    "Track Selected",
    "Invalid"
};
#endif

struct AppTheme {
    ImU32 colors[AppThemeCol_COUNT];
};

// Struct that holds the application's state
struct AppState {
    // What file did we load?
    std::string file_path;

    // This holds the main timeline object.
    // Pretty much everything drills into this one entry point.
    raven::TimelineProviderHarness timelinePH;

    // Timeline display settings
    float default_track_height = 30.0f; // (pixels)

    bool display_timecode = true;
    bool display_frames = false;
    bool display_seconds = false;
    bool display_rate = false;
    opentime::IsDropFrameRate drop_frame_mode = opentime::InferFromRate;

    // Selection.
    otio::SerializableObject*
        selected_context; // often NULL, parent to the selected object for OTIO
    // objects which don't track their parent
    std::string selected_text; // displayed in the JSON inspector
    char message[1024]; // single-line message displayed in main window

    // Toggles for Dear ImGui windows
    bool show_main_window = true;
    bool show_style_editor = false;
    bool show_demo_window = false;
    bool show_metrics = false;
    bool show_implot_demo_window = false;
};

extern AppState appState;
extern AppTheme appTheme;
extern ImFont* gFont;

void Log(const char* format, ...);
void Message(const char* format, ...);
std::string Format(const char* format, ...);

std::string otio_error_string(otio::ErrorStatus const& error_status);

void SelectObject(
    otio::SerializableObject* object,
    otio::SerializableObject* context = NULL);
void SeekPlayhead(double seconds);
void SnapPlayhead();
void DetectPlayheadLimits();
void FitZoomWholeTimeline();
std::string FormattedStringFromTime(otio::RationalTime time, bool allow_rate = true);
std::string TimecodeStringFromTime(otio::RationalTime);
std::string FramesStringFromTime(otio::RationalTime);
std::string SecondsStringFromTime(otio::RationalTime);
void UpdateJSONInspector();
