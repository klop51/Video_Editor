# fx Module
Responsibility: Effect and transition nodes used by the render graph.

Current: Placeholder base `Effect` interface.

Planned Additions:
- Parameter system (typed values + animation handles).
- Built-in effect set: transform, opacity, crop, LUT, blur, sharpen.
- Transition base (crossfade first).
- Serialization hooks for parameter persistence.

Dependency Direction:
`fx` depends on `render` for node integration; must not depend on UI.
