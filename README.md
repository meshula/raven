# Raven NLE

![screenshot](screenshot.png)

## Building

	% make
	% ./raven

See `.github/workflows/build.yaml` for more details.

## Thanks

Made with the excellent [Dear ImGui](https://github.com/ocornut/imgui), [SoLoud](http://sol.gfxile.net/soloud/), and [dr_mp3](https://github.com/mackron/dr_libs/blob/master/dr_mp3.h).

## To Do

- Double-click to expand/collapse nested compositions
- Double-click a Clip to expand/collapse it's media reference
- Show time-warped playhead inside media reference or nested composition
- Performance optimization
  - avoid rendering items out of scroll region
  - avoid rendering items smaller than a tiny sliver
- Dockable side-by-side inspector
- Look at ImGui document-based demo code
- Edit JSON to replace selected object
- Tree view of JSON structure with collapse/expand anything
  - Maybe also edit numbers?
  - How fancy can we get with introspection of otio::SerializableObject?
- Arrow keys to navigate by selection
- Fit zoom when document opens
- Inspector panel with timing information (ala otiotool --inspect)
- Per-schema inspector GUI
  - SerializableObjectWithMetadata (or any child):
    - name
    - metadata
  - Items:
    - enable/disable
  - Clips:
    - adjust source_range
    - adjust available_range of media reference
  - Transitions:
  	- adjust in/out offsets
  - Gaps:
    - adjust duration
  - Markers:
    - adjust marked range
    - color
    - name
    - comment
  - Compositions:
    - adjust source_range
  - Effects:
    - name
    - effect_type
  - LinearTimeWarp:
    - time_scale
