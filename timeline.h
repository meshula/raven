// Timeline widget
#ifndef RAVEN_TIMELINE_WIDGET_H
#define RAVEN_TIMELINE_WIDGET_H
#include <opentimelineio/gap.h>
#include <opentimelineio/serializableObject.h>
#include <opentimelineio/timeline.h>
#include <opentimelineio/transition.h>
#include "timeline_provider.hpp"
namespace otio = opentimelineio::OPENTIMELINEIO_VERSION;

#include <map>
#include <string>
#include <vector>

namespace raven {
class OTIOProvider : public TimelineProvider {
    otio::SerializableObject::Retainer<otio::Timeline> _timeline;
    std::map<TimelineNode, otio::SerializableObject::Retainer<otio::Composable>,
                                            cmp_TimelineNode> nodeMap;
    std::map<TimelineNode, TimelineNode,
                                            cmp_TimelineNode> parentMap;
    std::map<otio::SerializableObject*, TimelineNode> _reverse;
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
            _trackKinds[trackNode] = dynamic_cast<otio::Track*>(track.value)->kind();
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
                    
                    if (dynamic_cast<otio::Gap*>(item) != nullptr) {
                        _kinds[itemNode] = NodeKind::Gap;
                    }
                    else if (dynamic_cast<otio::Transition*>(item) != nullptr) {
                        _kinds[itemNode] = NodeKind::Transition;
                    }
                    else {
                        _kinds[itemNode] = NodeKind::General;
                    }
                    
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
    
    TimelineNode RootNode() const override {
        auto it = nodeMap.find(RootNodeId());
        if (it == nodeMap.end()) {
            return TimelineNodeNull();
        }
        return it->first;
    }
    
    otio::SerializableObject::Retainer<otio::Timeline> OtioTimeline() {
        return _timeline;
    }
    
    otio::SerializableObject::Retainer<otio::Composable> OtioFromNode(TimelineNode n) {
        auto it = nodeMap.find(n);
        if (it == nodeMap.end()) {
            return {};
        }
        return it->second;
    }
    
    TimelineNode NodeFromOtio(otio::SerializableObject* i) {
        auto it = _reverse.find(i);
        if (it == _reverse.end()) {
            return TimelineNodeNull();
        }
        return it->second;
    }
    
    uint64_t StationaryId(TimelineNode n) const override {
        auto it = nodeMap.find(n);
        if (it == nodeMap.end()) {
            return {};
        }
        return static_cast<uint64_t>(static_cast<uintptr_t>(it->second));
    }
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

} // raven



#endif
