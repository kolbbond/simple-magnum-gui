# CLAUDE.md

Project instructions for Claude Code.

<!-- pan-init:start (managed by /pan-init — do not edit between markers) -->

## Panopticon

This project uses [panopticon](https://github.com/kolbbond/panopticon) for autonomous agent workflows. This block (`PAN.md` content, between the `pan-init:*` markers) is **managed**: edit `<panopticon>/plugins/init/PAN.md` and re-run `/pan-init` to update. Anything outside the markers is yours to edit freely.

### How to work with this agent

- **Goal-in, result-out.** State the goal; the agent drives it through a pipeline (research → analysis → implementation → verification) and reports the result. You don't have to approve every intermediate step — review the result.
- **Auto mode** means the agent proceeds without asking on routine, low-risk decisions. Course-correct at any point; the agent treats new messages as normal input, not interruptions.
- **The agent will pause and ask** before destructive or shared-state actions (force-push, dropping data, posting to external services, deleting branches). It will not ask for routine local edits, builds, or tests.
- **Subagents and worktrees.** Long or parallel work is dispatched to subagents, often in isolated git worktrees under `.git/worktrees/`. Each subagent gets a fresh context window and reports back; you see the summary, not their full transcript.
- **The agent's job is to be helpful, not performatively agreeable.** If a request is ambiguous, contradictory, or seems wrong, the agent should push back with reasoning rather than silently doing the wrong thing.

### What the agent verifies before claiming done

- Type-checker (`tsc --noEmit`, `mypy`, etc.) — fix all errors, do not suppress
- Linters if configured (`eslint`, `ruff`, etc.)
- Test suite for the touched area
- Build commands when relevant — prefer `bin/pan-ops doctor/build/test/bench` if available, since it is allowlisted for the autonomous loop
- If no checker is configured, the agent says so explicitly rather than asserting "done"

### Git and GitHub

- **No co-authorship.** Never add a `Co-Authored-By` trailer to commits.
- **Commit subjects: topic-prefixed and laconic.** Lead with the branch or topic, then a terse description — `fluid: add helium temperature-dependent materials h(T)`. Skip a body unless the change genuinely needs one.
- **PR titles: initials-prefixed, same laconic style.** `dkb / add thermal fluid`. Keep the body short or omit it. Open a PR only when asked.

### Where output lives

- **Analyses**: `docs/analyses/<YYYY-MM-DD>-<topic>/` — one directory per run, with `session-log.md` or `cycle-log.md` plus artifacts
- **Reports**: `bin/pan-report` (or `/pan-report`) produces `report.md` + `report.pdf` (LaTeX → pandoc fallback → markdown-only)
- **Plots**: matplotlib style at `<panopticon>/plugins/plot/panopticon.mplstyle`; drop a `pan-plot.mplstyle` in repo root for project-local overrides
- **Project-local memory**: `./.pan/memory/<project>/` via `/pan-memory` — classified, versionable, separate from global auto-memory
- **References / knowledge base**: plain markdown + CSV under `references/<domain>/`, grep-friendly, with YAML front-matter (`title`, `source`, `date_saved`, `tags`)

### Slash commands

- `/pan <question>` — auto-route to the appropriate domain plugin
- `/pan-check` — quick read-only status snapshot (git, processes, recent analyses, scheduled tasks)
- `/pan-pr [#|--mine]` — PR status, CI checks, reviews, unresolved threads via `gh`
- `/pan-report [topic]` — lab-report-style PDF + markdown summary of the session (or any `docs/analyses/<dir>` post-hoc)
- `/pan-plot` — plotting style guide + matplotlib templates
- `/pan-memory <content>` — save a classified memory to `./.pan/memory/`
- `/pan-init` — refresh this managed block from the canonical `PAN.md`
- `/pan-cycle --goal "<goal>"` (CLI) — run an autonomous improvement cycle
- General-purpose: `/refactor`, `/debug`, `/research`, `/sweep`, `/build`, `/test-gap`, `/review-code`, `/parallel-code`, `/cycle`, `/improve-loop`

### Things to avoid

- **Don't edit inside the `pan-init:start` / `pan-init:end` markers.** Re-running `/pan-init` overwrites that span. Put project-specific notes *outside* the markers — anywhere above or below — and they survive refreshes untouched.
- **Don't rely on the agent's conversation memory across sessions.** Use `/pan-memory` for cross-session facts and add durable project rules outside the managed block in this file.
- **Don't ask the agent to skip hooks** (`--no-verify`, `--no-gpg-sign`) unless you explicitly want that. The agent will refuse by default and surface the underlying failure instead.
- **Don't bypass `bin/pan-ops`** when its verbs apply (`doctor`, `build`, `test`, `bench`, `commit`, `notify`, `log-append`). Raw shell calls bypass the allowlist and surface as permission prompts to the human.

<!-- pan-init:end -->

<!-- pan-init:start:cxx (managed by /pan-init — do not edit between markers) -->

## C++ style

All C++ must conform to the project's `.clang-format` and `.clang-tidy` in the
repo root. These configs are the source of truth — when a diagnostic suggests a
formatting fix, run the tool rather than hand-editing.

### clang-format
- Run `clang-format -i <file>` on every modified C++ file before committing.
- House settings (informational — the repo's `.clang-format` wins): LLVM base,
  4-space indent, 140-column limit, no tabs, compact namespaces, short
  blocks/functions/ifs allowed on one line, pointer left-aligned (`int* p`),
  includes not sorted.

### clang-tidy
- Run `clang-tidy <file>` on modified files.
- Enabled groups: `clang-analyzer-*`, `bugprone-*`, `performance-*`,
  `modernize-*`, `readability-*`, `portability-*`.
- Prefer `modernize-use-override`; avoid trailing return types.
- **Do NOT use `auto`** for definitions. Always use explicit types. The only
  exception is iterators (and genuinely unspellable STL types).
- **Avoid `using`** — no `using namespace` directives (never in headers) and no
  `using`-declarations that pull names into scope. Spell out qualified names
  (`std::vector`, `std::string`) so every identifier's origin is explicit.
- Cognitive-complexity threshold: 100.

### Comments
- **Laconic.** Comment the *why*, not the *what* — the code already says what it
  does. A short clause beats a sentence; a sentence beats a paragraph.
- No comments that restate the next line, no banner blocks, no commented-out
  code. Match the comment density of the surrounding file.

<!-- pan-init:end:cxx -->
