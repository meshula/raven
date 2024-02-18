//
//  timeline_provider.hpp
//  raven
//
//  Created by Nick Porcino on 2/17/24.
//

#ifndef timeline_provider_h
#define timeline_provider_h

#include <opentime/timeRange.h>

namespace raven {

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

inline bool operator!= (const TimelineNode& a, const TimelineNode& b) {
    return !(a == b);
}

constexpr inline TimelineNode TimelineNodeNull() { return { 0 }; }
constexpr inline TimelineNode RootNodeId() { return (TimelineNode){1}; }

class TimelineProvider {
public:
    enum NodeKind {
        Track, General, Gap, Transition
    };
    
    using TimeRange = opentime::OPENTIME_VERSION::TimeRange;
    using RationalTime = opentime::OPENTIME_VERSION::RationalTime;

protected:
    std::string nullName;
    std::map<TimelineNode, std::vector<TimelineNode>, cmp_TimelineNode> _syncStarts;
    std::map<TimelineNode, std::vector<TimelineNode>, cmp_TimelineNode> _seqStarts;
    std::map<TimelineNode, TimeRange,                 cmp_TimelineNode> _times;
    std::map<TimelineNode, std::string,               cmp_TimelineNode> _names;
    std::map<TimelineNode, std::string,               cmp_TimelineNode> _trackKinds;
    std::map<TimelineNode, NodeKind,                  cmp_TimelineNode> _kinds;
    void clearMaps() {
        _syncStarts.clear();
        _seqStarts.clear();
        _times.clear();
        _names.clear();
        _trackKinds.clear();
        _kinds.clear();
    }

public:
    explicit TimelineProvider() {
        nullName = "<null>";
    }
    virtual ~TimelineProvider() = default;

    TimelineNode HasSequentialSibling(TimelineNode) const;
    TimelineNode HasSynchronousSibling(TimelineNode) const;
    TimelineNode HasParent(TimelineNode) const;
    
    virtual std::vector<std::string> NodeKindNames() const = 0;
    virtual TimelineNode             RootNode() const = 0;
    virtual TimeRange                TimelineTimeRange() const = 0;
    virtual uint64_t                 StationaryId(TimelineNode) const = 0;

    const std::string& Name(TimelineNode n) const {
        auto it = _names.find(n);
        if (it == _names.end())
            return nullName;
        return it->second;
    }
    NodeKind Kind(TimelineNode n) const {
        auto it = _kinds.find(n);
        if (it == _kinds.end())
            return NodeKind::General;
        return it->second;
    }
    const std::string& TrackKind(TimelineNode n) const {
        auto it = _trackKinds.find(n);
        if (it == _trackKinds.end())
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
    TimeRange NodeTimeRange(TimelineNode n) const {
        auto it = _times.find(n);
        if (it == _times.end())
            return TimeRange();
        return it->second;
    }
    RationalTime StartTime(TimelineNode n) const {
        auto it = _times.find(n);
        if (it == _times.end())
            return RationalTime();
        return it->second.start_time();
    }
    RationalTime Duration(TimelineNode n) const {
        auto it = _times.find(n);
        if (it == _times.end())
            return RationalTime();
        return it->second.duration();
    }
};

/*
 The ProviderHarness holds the timeline state; it's got a provider,
 and runtime state, such as playhead position.
 */

class TimelineProviderHarness {
    using TimeRange = opentime::OPENTIME_VERSION::TimeRange;
    using RationalTime = opentime::OPENTIME_VERSION::RationalTime;
    std::unique_ptr<TimelineProvider> provider;
    TimeRange playhead_limit;           // min/max limit for moving the playhead, auto-calculated

public:
    TimelineProviderHarness() = default;
    ~TimelineProviderHarness() = default;

    template<typename T>
    T* Provider() const { return dynamic_cast<T*>(provider.get()); }

    TimelineProvider* Provider() const { return provider.get(); }

    void SetProvider(std::unique_ptr<TimelineProvider>&& p) {
        provider = std::move(p);
    }

    TimeRange PlayheadLimit() const { return playhead_limit; }

    void Seek(double seconds) {
        double lower_limit = playhead_limit.start_time().to_seconds();
        double upper_limit = playhead_limit.end_time_exclusive().to_seconds();
        seconds = fmax(lower_limit, fmin(upper_limit, seconds));
        playhead = RationalTime::from_seconds(seconds, playhead.rate());
    }

    void SetPlayheadLimitFromProvider() {
        if (provider)
            playhead_limit = provider->TimelineTimeRange();
    }

    TimelineNode selected_object;
    RationalTime playhead;
    bool scroll_to_playhead = false;    // internal flag, only true until next frame
    bool  drawPanZoomer = true;
    float timeline_width = 100.0f;
    float zebra_factor = 0.1;           // opacity of the per-frame zebra stripes
    bool  snap_to_frames = true;        // user preference to snap the playhead, times,
    float scale = 100.0f;               // zoom scale, measured in pixels per second
    float track_height = 30.0f;         // current track height (pixels)
};

}

#endif /* timeline_provider_h */
