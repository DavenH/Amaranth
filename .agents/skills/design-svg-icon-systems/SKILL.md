---
name: design-svg-icon-systems
description: Design, redesign, and integrate cohesive semantic SVG icon sets for application interfaces. Use when Codex needs to create or improve multiple repo-native vector icons, unify inconsistent icons, replace miniature UI previews with dedicated symbols, make sidebar or toolbar icons legible at production size, or visually review and iterate an existing SVG icon family.
---

# Design SVG Icon Systems

Create icons as a visual language: semantically distinct, stylistically related, and proven legible in their real interface. Prefer native SVG and repository rendering tools over generated bitmap art.

## 1. Inspect Before Designing

Read repository instructions and inspect:

- every icon or item in scope, including icons the user already likes;
- the production renderer, actual bounds, background, hover/selected states, and scaling behavior;
- the product's existing palette, stroke language, corner radii, and density;
- the authoritative registry or model that defines the complete item set;
- mature full-size visualizations that may inform semantics without being reused as tiny thumbnails.

Render the current UI before editing. Record what succeeds and what fails at the actual displayed size. Preserve strong concepts while unifying their geometry and color treatment.

Do not assume that shrinking a detailed preview produces an icon. If the existing path renders a miniature graph, editor, or visualization, create a dedicated compact icon path and leave the authoritative full-size renderer unchanged.

## 2. Make a Semantic Icon Brief

Before drawing, make a compact working matrix for the complete family:

| Item | Essential meaning | Primary silhouette | Confusable siblings | Accent role |
| --- | --- | --- | --- | --- |
| Example | What it does, not merely its name | One recognizable gesture | Icons it must differ from | Domain or signal color |

Choose one strong metaphor per icon. Express behavior when possible: inputs merging, a signal splitting, echoes receding, a curve being shaped, or one representation transforming into another.

Apply these rules:

- Make sibling icons differ first by silhouette and topology, not only by color.
- Use direction, repetition, decay, branching, or convergence to communicate operation.
- Avoid labels and tiny text. Use mathematical glyphs only when they are the established concept.
- Remove decorative detail that does not improve recognition.
- Design the family together so repeated concepts use repeated shapes.
- Cover every authoritative item; do not silently omit awkward or generic cases.

## 3. Establish a Visual Grammar

Define the system before polishing individual files. Adapt values to the host UI, but keep them consistent across the family:

- one square `viewBox`, commonly `0 0 48 48`;
- transparent backgrounds;
- rounded line caps and joins;
- a small stroke-weight vocabulary, typically one primary and one secondary weight;
- consistent optical margins and a shared visual center;
- muted structural lines plus restrained semantic accents;
- a limited palette sampled from the application;
- simple primitives and short paths that remain clear when rasterized.

Use color to reinforce meaning, never as the only distinction. Keep critical geometry fully opaque. Faint strokes, transparency, gradients, filters, masks, and sub-pixel details often disappear at sidebar size.

Prefer a few deliberate shapes over literal miniature diagrams. Similar visual weight matters more than identical bounding boxes: compensate optically for circles, diagonals, sparse icons, and dense icons.

## 4. Draw the Complete First Pass

Write clean, editable SVG source. Keep related files structurally consistent so the family is easy to review and maintain.

For each icon:

1. Draw the primary semantic gesture.
2. Add only the context needed to disambiguate it.
3. Apply the shared palette and stroke rules.
4. Check that the silhouette survives monochrome inspection.
5. Compare it directly with its nearest siblings.

Complete the whole set before deeply polishing one icon. System-level inconsistencies become visible only when the family is viewed together.

## 5. Integrate Efficiently

Follow the repository's established resource pipeline. When architecture permits:

- embed or package SVG source rather than checked-in raster derivatives;
- map icons through the authoritative type registry instead of duplicating a large type switch;
- parse each SVG once and cache the drawable;
- keep palette/sidebar icon rendering separate from full-size content previews;
- add a coverage test proving every registered type resolves to a parseable icon.

Do not copy mature rendering or domain behavior into an icon adapter. The icon should communicate the concept, not reimplement it.

## 6. Run the Visual Review Loop

Run at least three deliberate review passes. Use PNG renders or screenshots for every pass; source inspection alone is insufficient.

### Pass A: Semantic recognition

Render all icons together at a large review scale and at exact production size. Ask:

- Can the operation be guessed without its label?
- Are sibling icons unmistakably different?
- Is the dominant gesture immediate?
- Does any symbol imply the wrong direction or behavior?

Fix metaphors and topology before styling details.

### Pass B: Optical legibility

Capture the real interface with its true background, padding, labels, and hover state. Inspect each group or context separately. Ask:

- Which strokes, dots, or gaps vanish at native size?
- Is any icon too faint, busy, small, or visually heavy?
- Are repeated elements countable when their count matters?
- Do baselines, centers, and apparent scale align?

Replace low-opacity critical elements with explicit colors. Enlarge meaningful dots and gaps. Simplify crossings and dense grids.

### Pass C: Family consistency

Review the complete contact sheet and actual UI again. Ask:

- Do the icons share stroke, radius, margin, and color logic?
- Are strong pre-existing concepts now part of the same family?
- Does one icon look imported from another visual language?
- Are category accents informative without becoming noisy?

Make targeted corrections, rerender, and repeat until no specific weakness remains. Do not stop merely because the SVGs are valid or attractive when enlarged; the production-size render is the source of truth.

## 7. Validate and Hand Off

Before completion:

- validate every SVG as XML;
- build the actual target that embeds and renders the assets;
- run the icon coverage test and relevant UI tests;
- inspect diffs for accidental generated files or unrelated changes;
- run repository style and whitespace checks;
- save representative final screenshots or contact sheets;
- summarize the semantic improvements and any architectural change.

Keep original vector sources in the repository. Treat rendered PNGs as review artifacts unless the product explicitly consumes them.

## Quality Bar

The set is finished only when every icon:

- has one clear meaning at its real displayed size;
- differs structurally from adjacent or related icons;
- belongs to the same visual system;
- remains legible on all relevant UI states;
- is connected to the authoritative item set and covered against omission;
- has been reviewed as a rasterized result inside the actual product.
