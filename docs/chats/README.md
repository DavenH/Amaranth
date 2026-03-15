# Chat Playbook

This folder stores concise conversation summaries that help future agents act
more autonomously in ways that match the user's preferences.

## Goal

Capture non-trivial user responses in a reusable form:

- the situation or ambiguity the agent faced
- how the user resolved it
- any stable preference, naming rule, or decision heuristic

The emphasis is semantic, not syntactic. Prefer summaries of the form:

- `State`: what was uncertain, incomplete, or being decided
- `User Preference`: what the user chose or clarified
- `Guideline`: how a future agent should act in similar cases

## When To Update

Update this folder:

- before any conversation compaction
- when the user explicitly asks to update the archive/playbook
- after a session that establishes a durable preference, workflow, or naming rule

Do not update it for trivial confirmations or one-off ephemeral choices unless
they reveal a broader pattern.

Canonical user trigger:

- `update playbook`

## File Layout

- One markdown file per notable session
- Filename format: Unix epoch seconds, e.g. `1773535683.md`

Each session file should include:

1. `Date`
2. `Context`
3. `Decisions`
4. `Generalized Rules`
5. `Open Questions` if any remain

## Writing Rules

- Keep entries short and high signal
- Generalize from the specific conversation when the pattern is durable
- Distinguish stable policy from temporary debugging concessions
- Prefer imperative guidance for future agents
- Mention files or subsystems only when they matter to the rule

## Preferred Content Types

- ambiguity-resolution heuristics from the user
- naming preferences and migration direction
- refactor sequencing preferences
- tolerance for temporary debug code versus permanent architecture
- validation expectations
- what counts as "done" for the user

## Avoid

- raw transcripts
- low-value chronology
- private chain-of-thought style notes
- speculative personality claims unsupported by repeated behavior

## Maintenance

When adding a new session file:

- preserve older files; this is an archive, not a rolling overwrite
- avoid duplicating an existing rule unless the new session materially sharpens it
- if a newer session supersedes an older preference, say so explicitly in the new file
