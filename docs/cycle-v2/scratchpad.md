# Cycle 2 Scratchpad

## Domain/source node

The graph likely needs an explicit way to start a pipeline in a chosen domain, but the
right representation is still open. A dedicated domain/source node could produce time,
spectral magnitude, or spectral phase directly from the same mesh/curve operand model.
That would avoid names like "trilinear wave surface" implying a fixed signal domain.

Open question: should this be a separate source node that adapts a mesh operand into a
domain signal, or should mesh generators expose domain modes directly in their schema?
