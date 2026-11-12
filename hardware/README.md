# Controller enclosure

Two original prototype meshes are included in [stl/](stl). Units are millimetres. Files already use print orientation.

| File | Source revision | Approximate bounds | Orientation |
| --- | --- | --- | --- |
| `openpromptlet-top-v4.2-print.stl` | v4.2 four-encoder Codex-sticker top | 111.05 × 68 × 19.39 mm | Face down, as supplied |
| `openpromptlet-bottom-v5-print.stl` | v5 modular top-module bottom shell | 111.05 × 68 × 20 mm | Floor on bed, as supplied |

[manifest.json](stl/manifest.json) records original filenames and SHA256 checksums. The v5 design was explicitly made to retain this unchanged v4.2 top. Only the controller is included; older combined bases, keypad frames and alternate magnet sizes are omitted to avoid mixing assemblies.

## Print and assemble

1. Inspect both meshes in your slicer at 100% scale. Start with a conventional PLA prototype profile such as 0.20 mm layers and three walls; these are starting suggestions, not a validated print recipe.
2. Print in the supplied orientation. The original v5 shell was designed for support-free printing. Inspect the top's posts and recesses in your own slicer before choosing supports.
3. With power disconnected, dry-fit the empty shell and top. Check the tongues and locating keys. Remove burrs or adjust a local copy rather than forcing the parts.
4. Dry-fit the Genesis board and screen, then check USB plug clearance and cable routing before fastening. Keep the boards insulated from conductive hardware and avoid cable strain.
5. Close only after testing the controls in the [setup guide](../docs/setup.md). If using the three optional 6 × 2 mm lid-magnet pairs, check mating polarity before gluing. Do not install the future keypad-join magnets for this standalone build.

## Known mechanical limits

These are published work-in-progress design files, not a fully qualified fabrication kit. A photographed controller exists, but the exact two-part assembly has not received a documented repeatable fit test in this release.

The top originated around an **AMOLED-1.43 screen bracket**. The working electronics use **LCD-1.46**, so that old bracket is intentionally not shipped as a compatible part. An LCD-1.46 mount/adapter and its screw lengths still need to be documented. Test the electronics outside the case while resolving the mount; do not substitute the old bracket or force the LCD into place.

Genesis clearances depend on the actual spacers and underside connectors. The v5 CAD allows a calculated 2.2 mm clearance under its assumed worst-case stack; this is design evidence, not a physical measurement of every build. Lid magnet retention and final fastener sizes are also open items.

No battery installation, shared power wiring or charging arrangement is specified. Power each board separately over USB for the included bench setup.
