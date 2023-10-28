// Timeline widget
#ifndef RAVEN_TIMELINE_WIDGET_H
#define RAVEN_TIMELINE_WIDGET_H
#include <opentimelineio/timeline.h>
namespace otio = opentimelineio::OPENTIMELINEIO_VERSION;

class TimelineProvider {
public:
    virtual ~TimelineProvider() = default;
    otio::SerializableObject::Retainer<otio::Timeline> timeline;
};

class OTIOProvider : public TimelineProvider {
public:
};

/*
 The ProviderHarness holds the timeline state; it's got a provider,
 and runtime state, such as playhead position.
 */

class TimelineProviderHarness {
public:
    TimelineProviderHarness() = default;
    ~TimelineProviderHarness() = default;
    
    std::unique_ptr<TimelineProvider> provider;
    
    otio::RationalTime playhead;
    otio::TimeRange playhead_limit; // min/max limit for moving the playhead, auto-calculated
    bool drawPanZoomer = true;
    float zebra_factor = 0.1;   // opacity of the per-frame zebra stripes

    bool scroll_to_playhead = false; // internal flag, only true until next frame

    bool snap_to_frames = true; // user preference to snap the playhead, times,
                                // ranges, etc. to frames
    float scale = 100.0f;       // zoom scale, measured in pixels per second
    float track_height = 30.0f; // current track height (pixels)
    float timeline_width = 100.0f; // automatically calculated (pixels)

    // to be abstracted
    otio::SerializableObject* selected_object; // maybe NULL
};

void DrawTimeline(TimelineProviderHarness* timeline);
bool DrawTransportControls(TimelineProviderHarness* timeline);
void DrawTimecodeRuler(
    TimelineProviderHarness* timeline,
    const void* ptr_id,
    otio::RationalTime start,
    otio::RationalTime end,
    float frame_rate,
    float time_scalar,
    float scale,
    float width,
    float height);

#endif
