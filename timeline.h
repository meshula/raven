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
constexpr inline TimelineNode DocumentNodeId() { return (TimelineNode){1}; }
constexpr inline TimelineNode RootNodeId() { return (TimelineNode){2}; }

class TimelineProvider {
protected:
    std::map<TimelineNode, std::vector<TimelineNode>, cmp_TimelineNode> _syncStarts;
    std::map<TimelineNode, std::vector<TimelineNode>, cmp_TimelineNode> _seqStarts;
    void clearMaps() { _syncStarts.clear(); }

public:
    virtual ~TimelineProvider() = default;

    TimelineNode HasSequentialSibling(TimelineNode) const;
    TimelineNode HasSynchronousSibling(TimelineNode) const;
    TimelineNode HasParent(TimelineNode) const;
    
    virtual std::vector<std::string> NodeKindNames() const = 0;
    virtual const std::string NodeKindName(TimelineNode) const = 0;
    virtual const std::string NodeSecondaryKindName(TimelineNode) const = 0;
    virtual otio::TimeRange TimeRange(TimelineNode) const = 0;
    virtual otio::RationalTime StartTime(TimelineNode) const = 0;
    virtual otio::RationalTime Duration(TimelineNode) const = 0;
    virtual TimelineNode Document() const = 0;
    virtual TimelineNode RootNode() const = 0;
    virtual otio::TimeRange TimelineTimeRange() const = 0;
    
    std::vector<TimelineNode> SyncStarts(TimelineNode n) {
        auto it = _syncStarts.find(n);
        if (it == _syncStarts.end())
            return {};
        return it->second;  // returns a copy
    }
    std::vector<TimelineNode> SeqStarts(TimelineNode n) {
        auto it = _seqStarts.find(n);
        if (it == _seqStarts.end())
            return {};
        return it->second;  // returns a copy
    }
};

class OTIOProvider : public TimelineProvider {
    otio::SerializableObject::Retainer<otio::Timeline> _timeline;
    std::map<TimelineNode, 
             otio::SerializableObject::Retainer<otio::Composable>,
             cmp_TimelineNode> nodeMap;
    std::map<TimelineNode, 
             TimelineNode,
             cmp_TimelineNode> parentMap;
    uint64_t nextId = 0;
    std::string nullName;
    
public:
    OTIOProvider() {
        nullName = "<null>";
    }
    virtual ~OTIOProvider() = default;
    
    otio::TimeRange TimelineTimeRange() const override {
        return otio::TimeRange(
            _timeline->global_start_time().value_or(otio::RationalTime()),
            _timeline->duration());
    }
    
    void SetTimeline(otio::SerializableObject::Retainer<otio::Timeline> t) {
        _timeline = t;
        nodeMap.clear();
        clearMaps();
        if (t.value == nullptr)
            return;
        
        // add the root
        nodeMap[DocumentNodeId()] = otio::dynamic_retainer_cast<otio::Composable>(t);
        _syncStarts[DocumentNodeId()] = std::vector<TimelineNode>();

        // encode the tracks of the timeline's stack as sync starts on the root.
        otio::Stack* stack = t->tracks();
        nodeMap[RootNodeId()] = otio::dynamic_retainer_cast<otio::Composable>(t);
        _syncStarts[RootNodeId()] = std::vector<TimelineNode>();
        auto start_it = _syncStarts.find(RootNodeId());
        nextId = 3;
        std::vector<otio::SerializableObject::Retainer<otio::Composable>> const& tracks = stack->children();
        for (auto trackItem : tracks) {
            otio::SerializableObject::Retainer<otio::Composable> track = otio::dynamic_retainer_cast<otio::Composable>(trackItem); // Composable to Item
            nodeMap[(TimelineNode){nextId}] = track;
            auto trackNode = (TimelineNode){nextId};
            start_it->second.push_back(trackNode); // register the synchronous start
            _seqStarts[trackNode] = std::vector<TimelineNode>();
            auto seq_it = _seqStarts.find(trackNode);
            parentMap[(TimelineNode){nextId}] = DocumentNodeId(); // register the parent
            ++nextId;
            
            otio::SerializableObject::Retainer<otio::Track> otrack = otio::dynamic_retainer_cast<otio::Track>(trackItem); // Composable to Item
            for (const auto& child : otrack->children()) {
                if (const auto& item = dynamic_cast<otio::Composable*>(child.value)) {
                    parentMap[(TimelineNode){nextId}] = trackNode;
                    nodeMap[(TimelineNode){nextId}] = item;
                    seq_it->second.push_back((TimelineNode){nextId}); // register the sequential starts
                    ++nextId;
                }
            }
        }
    }
    
    std::vector<std::string> NodeKindNames() const override {
        return {};
    }
    const std::string NodeKindName(TimelineNode) const override {
        return nullName;
    }
    const std::string NodeSecondaryKindName(TimelineNode n) const override {
        auto it = nodeMap.find(n);
        if (it == nodeMap.end())
            return nullName;
        auto track = otio::dynamic_retainer_cast<otio::Track>(it->second);
        if (track.value == nullptr)
            return nullName;
        return track->kind();
    }
    
    otio::TimeRange TimeRange(TimelineNode) const override {
        return otio::TimeRange();
    }
    otio::RationalTime StartTime(TimelineNode n) const override {
        auto it = nodeMap.find(n);
        if (it == nodeMap.end()) {
            return otio::RationalTime();
        }
        auto item = dynamic_cast<otio::Item*>(it->second.value);
        if (!item) {
            return otio::RationalTime();
        }
        return item->trimmed_range().start_time();
    }
    otio::RationalTime Duration(TimelineNode n) const override {
        auto it = nodeMap.find(n);
        if (it == nodeMap.end()) {
            return otio::RationalTime();
        }
        return it->second->duration();
    }
    TimelineNode Document() const override {
        auto it = nodeMap.find(DocumentNodeId());
        if (it == nodeMap.end()) {
            return TimelineNodeNull();
        }
        return it->first;
    }
    TimelineNode RootNode() const override {
        auto it = nodeMap.find(RootNodeId());
        if (it == nodeMap.end()) {
            return TimelineNodeNull();
        }
        return it->first;
    }

    otio::SerializableObject::Retainer<otio::Timeline> OtioTimeilne() {
        return _timeline;
    }

    otio::SerializableObject::Retainer<otio::Composable> OtioItemFromNode(TimelineNode n) {
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
