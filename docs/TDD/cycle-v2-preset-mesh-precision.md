# Cycle V2 preset mesh precision

Status: implemented.

## Contract and owner

`GraphSerializer::meshScalarToJSON` is the authoritative writer for mesh vertex
numbers. It must emit the shortest decimal that round-trips to the stored
`float`, while retaining `.0` for integral floating-point values. The writer
still visits each vertex once. Parameter numbers continue through their
existing serializer path and retain their higher precision.

The serializer is a cohesive 1,322-line file (1,323 before this change): it
owns graph JSON shape, migration entry points, and canonical writing. This
change replaces one local number formatter without adding a policy site or
dependency outside standard C++ `to_chars`; no extraction is warranted for
this narrow adjustment.

Checked-in presets that were saved with fixed nine fractional digits are
normalized lexically. For every changed mesh vertex field, the old and new
text decode to the same binary32 value. Node positions, graph topology,
parameters, and other preset content are preserved. Existing local preset
edits remain in the working tree.

## Verification

Before cleanup, the preset diff against `origin/master` contained 20,750
insertions and 21,051 deletions. About 32,000 changed numeric fields decoded
to the same binary32 value as master. The precision-only cleanup changed
70,279 numeric tokens in 74 presets, with an automated parsed-graph check that
all non-vertex values stayed identical and all vertex binary32 values stayed
identical. The resulting diff contains 4,527 insertions and 4,827 deletions;
the remaining changes include authored layouts, topology, and a new preset.

`Graph JSON*` passes 126 assertions in 17 cases. The representative
`astral-3.cyclegraph` loads and saves with all 424 mesh vertex lines unchanged.
The focused float test now uses a populated graph fixture; its previous demo
graph had an empty mesh and crashed before reaching the precision assertion.
