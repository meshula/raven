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
constexpr inline TimelineNode RootNodeId() { return (TimelineNode){1}; }

class TimelineProvider {
protected:
    std::string nullName;
    std::map<TimelineNode, std::vector<TimelineNode>, cmp_TimelineNode> _syncStarts;
    std::map<TimelineNode, std::vector<TimelineNode>, cmp_TimelineNode> _seqStarts;
    std::map<TimelineNode, otio::TimeRange,           cmp_TimelineNode> _times;
    std::map<TimelineNode, std::string,               cmp_TimelineNode> _names;
    void clearMaps() {
        _syncStarts.clear();
        _seqStarts.clear();
        _times.clear();
        _names.clear();
    }

public:
    TimelineProvider() {
        nullName = "<null>";
    }
    virtual ~TimelineProvider() = default;

    TimelineNode HasSequentialSibling(TimelineNode) const;
    TimelineNode HasSynchronousSibling(TimelineNode) const;
    TimelineNode HasParent(TimelineNode) const;
    
    virtual std::vector<std::string> NodeKindNames() const = 0;
    virtual const std::string        NodeKindName(TimelineNode) const = 0;
    virtual const std::string        NodeSecondaryKindName(TimelineNode) const = 0;
    virtual TimelineNode             RootNode() const = 0;
    virtual otio::TimeRange          TimelineTimeRange() const = 0;

    const std::string& Name(TimelineNode n) const {
        auto it = _names.find(n);
        if (it == _names.end())
            return nullName;
        return it->second;
    }
    std::vector<TimelineNode> SyncStarts(TimelineNode n) const {
        auto it = _syncStarts.find(n);
        if (it == _syncStarts.end())
            return {};
        return it->second;  // returns a copy
    }
    std::vector<TimelineNode> SeqStarts(TimelineNode n) const {
        auto it = _seqStarts.find(n);
        if (it == _seqStarts.end())
            return {};
        return it->second;  // returns a copy
    }
    otio::TimeRange TimeRange(TimelineNode n) const {
        auto it = _times.find(n);
        if (it == _times.end())
            return otio::TimeRange();
        return it->second;
    }
    otio::RationalTime StartTime(TimelineNode n) const {
        auto it = _times.find(n);
        if (it == _times.end())
            return otio::RationalTime();
        return it->second.start_time();
    }
    otio::RationalTime Duration(TimelineNode n) const {
        auto it = _times.find(n);
        if (it == _times.end())
            return otio::RationalTime();
        return it->second.duration();
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
    std::map<otio::Composable*, TimelineNode> _reverse;
    uint64_t nextId = 0;
    
    // Transform this range map from the context item's coodinate space
    // into the top-level timeline's coordinate space. This compensates for
    // any source_range offsets in intermediate levels of nesting in the
    // composition.
    void TransformToContextCoordinateSpace(
            std::map<otio::Composable*, otio::TimeRange>& range_map,
            otio::Item* context) {
        auto zero = otio::RationalTime();
        TimelineNode tracksNode = RootNode();
        auto topItem = OtioFromNode(tracksNode);
        auto top = dynamic_cast<otio::Item*>(topItem.value);
        if (top) {
            auto offset = context->transformed_time(zero, top);
            for (auto& pair : range_map) {
                auto& range = pair.second;
                range = otio::TimeRange(range.start_time() + offset, range.duration());
            }
        }
    }

public:
    OTIOProvider() = default;
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
        otio::Stack* stack = t->tracks();
        nodeMap[RootNodeId()] = otio::dynamic_retainer_cast<otio::Composable>(t);
        _syncStarts[RootNodeId()] = std::vector<TimelineNode>();

        // encode the tracks of the timeline's stack as sync starts on the root.
        auto start_it = _syncStarts.find(RootNodeId());
        nextId = 3;
        std::vector<otio::SerializableObject::Retainer<otio::Composable>> const& tracks = stack->children();
        for (auto trackItem : tracks) {
            otio::SerializableObject::Retainer<otio::Composable> track = otio::dynamic_retainer_cast<otio::Composable>(trackItem); // Composable to Item
            nodeMap[(TimelineNode){nextId}] = track;
            auto trackNode = (TimelineNode){nextId};
            _reverse[trackItem.value] = trackNode;
            start_it->second.push_back(trackNode); // register the synchronous start
            _seqStarts[trackNode] = std::vector<TimelineNode>();
            _names[trackNode] = track->name();
            auto seq_it = _seqStarts.find(trackNode);
            ++nextId;
            
            otio::SerializableObject::Retainer<otio::Track> otrack = otio::dynamic_retainer_cast<otio::Track>(trackItem); // Composable to Item
            for (const auto& child : otrack->children()) {
                if (const auto& item = dynamic_cast<otio::Composable*>(child.value)) {
                    TimelineNode itemNode = (TimelineNode){nextId};
                    parentMap[itemNode] = trackNode;
                    nodeMap[itemNode] = item;
                    _reverse[item] = itemNode;
                    _names[itemNode] = item->name();
                    seq_it->second.push_back(itemNode); // register the sequential starts
                    ++nextId;
                }
            }
            
            // compute and cache the times for all the children
            auto times = otrack->range_of_all_children();
            TransformToContextCoordinateSpace(times, otrack);
            for (auto time : times) {
                auto it = _reverse.find(time.first);
                if (it != _reverse.end()) {
                    _times[it->second] = time.second;
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

    otio::SerializableObject::Retainer<otio::Composable> OtioFromNode(TimelineNode n) {
        auto it = nodeMap.find(n);
        if (it == nodeMap.end()) {
            return {};
        }
        return it->second;
    }
    
    TimelineNode NodeFromOtio(otio::Composable* i) {
        auto it = _reverse.find(i);
        if (it == _reverse.end()) {
            return TimelineNodeNull();
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
    float time_scale,
    float width,
    float height);

#endif
