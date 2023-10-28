// Timeline widget
#ifndef RAVEN_TIMELINE_WIDGET_H
#define RAVEN_TIMELINE_WIDGET_H
#include <opentimelineio/serializableObject.h>
#include <opentimelineio/timeline.h>
namespace otio = opentimelineio::OPENTIMELINEIO_VERSION;

#include <map>
#include <string>
#include <vector>

struct TimelineNode {
    uint64_t id;
};

struct cmp_TimelineNode {
    bool operator()(const TimelineNode& a, const TimelineNode& b) const {
        return a.id < b.id;
    }
};

inline bool operator== (const TimelineNode& a, const TimelineNode& b) {
    return a.id == b.id;
}


constexpr inline TimelineNode TimelineNodeNull() { return { 0 }; }

class TimelineProvider {
public:
    otio::SerializableObject::Retainer<otio::Timeline> _timeline;

    virtual ~TimelineProvider() = default;
    
    TimelineNode HasSequentialSibling(TimelineNode) const;
    TimelineNode HasSynchronousSibling(TimelineNode) const;
    TimelineNode HasParent(TimelineNode) const;
    
    virtual std::vector<std::string> NodeKindNames() const = 0;
    virtual const std::string& NodeKindName(TimelineNode) const = 0;
    virtual otio::TimeRange TimeRange(TimelineNode) const = 0;
    virtual otio::RationalTime StartTime(TimelineNode) const = 0;
    virtual otio::RationalTime Duration(TimelineNode) const = 0;
    virtual TimelineNode RootNode() const = 0;
};

class OTIOProvider : public TimelineProvider {
    std::map<TimelineNode, otio::SerializableObject::Retainer<otio::Item>,
             cmp_TimelineNode> nodeMap;
    uint64_t nextId = 0;
    std::string nullName;
public:
    OTIOProvider() = default;
    virtual ~OTIOProvider() = default;
    
    void SetTimeline(otio::SerializableObject::Retainer<otio::Timeline> t) {
        _timeline = t;
        nodeMap.clear();
        nextId = 2;
        nodeMap[(TimelineNode){1}] = otio::dynamic_retainer_cast<otio::Item>(t);
        nullName = "<null>";
    }
    
    std::vector<std::string> NodeKindNames() const override {
        return {};
    }
    const std::string& NodeKindName(TimelineNode) const override {
        return nullName;
    }
    otio::TimeRange TimeRange(TimelineNode) const override {
        return otio::TimeRange();
    }
    otio::RationalTime StartTime(TimelineNode) const override {
        auto it = nodeMap.find({1});
        if (it == nodeMap.end()) {
            return otio::RationalTime();
        }
        return it->second->trimmed_range().start_time();
    }
    otio::RationalTime Duration(TimelineNode) const override {
        auto it = nodeMap.find({1});
        if (it == nodeMap.end()) {
            return otio::RationalTime();
        }
        return it->second->duration();
    }
    TimelineNode RootNode() const override {
        auto it = nodeMap.find({1});
        if (it == nodeMap.end()) {
            return TimelineNodeNull();
        }
        return it->first;
    }

    otio::SerializableObject::Retainer<otio::Item> OtioItemFromNode(TimelineNode n) {
        auto it = nodeMap.find(n);
        if (it == nodeMap.end()) {
            return {};
        }
        return it->second;
    }
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
