# Cycle 2 panel trackpad navigation

Status: implemented.

The shared `ZoomPanel` owns viewport bounds, scrollbar clamping, and viewport notifications. Cycle 2's `PanelInputHostComponent` owns routing of pointer input to the legacy panel interactor. The interactor's vertical wheel behavior remains authoritative for zoom, including its modifiers and tool handling.

Route horizontal wheel movement from Cycle 2 hosts to `ZoomPanel`'s horizontal scrollbar range. Route vertical movement to the interactor. A diagonal gesture may do both. A pure horizontal gesture must not trigger zoom or tool changes. Panning is constant work per wheel event and never changes the document graph or mesh. Cycle 1's direct interactor path remains untouched.

Completion: a panel event regression proves horizontal panning preserves zoom width, vertical scrolling changes zoom width, and horizontal movement respects range limits. Remove any temporary routing or duplicate viewport clamping introduced during implementation.

Verification: focused Trimesh panel event coverage includes the 2D spectral viewport and the 3D viewport after zooming; the existing scrollbar owns clamping and notifications. No temporary routing or duplicate clamping remains.
