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
    bool drawPanZoomer;

    // to be abstracted
    otio::SerializableObject* selected_object; // maybe NULL
};

void DrawTimeline(TimelineProviderHarness* timeline);
bool DrawTransportControls(TimelineProviderHarness* timeline);
void DrawTimecodeRuler(
    const void* ptr_id,
    otio::RationalTime start,
    otio::RationalTime end,
    float frame_rate,
    float time_scalar,
    float scale,
    float width,
    float height);

#endif
