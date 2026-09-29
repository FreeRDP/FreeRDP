# Native Bridge Skeleton

This folder hosts the Harmony native bridge that will eventually wrap FreeRDP.

## Next wiring steps

1. Add Harmony N-API and native window dependencies.
2. Link `libfreerdp`, `winpr` and selected `channels` targets.
3. Route `BeginPaint/EndPaint` output into a Harmony surface.
4. Map touch, keyboard and clipboard events back into `freerdp_input_send_*` calls.
