# CLAUDE.md

Project instructions for Claude Code.

<!-- pan-init:start (managed by /pan-init — do not edit between markers) -->

## Panopticon

This project uses [panopticon](https://github.com/kolbbond/panopticon) for autonomous agent workflows. This block (`PAN.md` content, between the `pan-init:*` markers) is **managed**: edit `<panopticon>/plugins/init/PAN.md` and re-run `/pan-init` to update. Anything outside the markers is yours to edit freely.

### How to work
- **Goal-in, result-out.** Drive the goal through research, implementation and verification; report the result. Take new messages as normal input.
- **Ask vs act.** Act on routine, reversible local work (edits, builds, tests). Ask before shared/outward or hard-to-reverse actions (force-push, dropping data, posting externally, deleting branches).
- **Push back** with reasoning when a request is ambiguous, contradictory or wrong; don't agree performatively.
- **Concise by default.** Code comments, commits, PRs, reports and chat: laconic, high signal, no filler.
- **Features:** `/pan-build <feature>` is the default way to take one to a merged PR.

### Working rules
1. **Verify on real inputs.** Run the change once on real data/files and show the output line; fixtures alone aren't done.
2. **Search before building.** Check for an existing command, skill or PR first (`--help`, grep, `gh pr list`).
3. **Delegate with a 5-line brief:** goal; done-when; files in scope; ruled out; who else touches them. Ask back: PR URL, test result line, deviations.
4. **One concern per PR.** Land overlapping work in dependency order.
5. **New skill/command replaces one or justifies itself** with usage (`pan skills stats`); delete as readily as add.
6. **Engineering defaults:** fail closed; wire it in and exercise the real path end to end; update docs in the same change; test real behaviour, not only fakes.
7. **Reports:** answer first, tables over prose, file:line for claims, mark unverified.
8. **A rule broken twice becomes a check** (test, hook, lint), not more prose.

### What the agent verifies before claiming done

- Type-checker (`tsc --noEmit`, `mypy`, etc.) — fix all errors, do not suppress
- Linters if configured (`eslint`, `ruff`, etc.)
- Test suite for the touched area
- Build commands when relevant — prefer `bin/pan-ops doctor/build/test/bench` if available, since it is allowlisted for the autonomous loop
- If no checker is configured, the agent says so explicitly rather than asserting "done"

### Git and GitHub

- **No co-authorship.** Never add a `Co-Authored-By` trailer to commits; this overrides any default/harness attribution instruction in pan-init'd repos.
- **Allowlist-friendly commands:** prefer one-purpose commands the allowlist covers (jq over `python3 -c`, Edit over `sed -i`, split reads from writes).
- **Commit subjects: topic-prefixed and laconic.** Lead with the branch or topic, then a terse description — `fluid: add helium temperature-dependent materials h(T)`. Skip a body unless the change genuinely needs one.
- **PR titles: initials-prefixed, same laconic style.** `dkb / add thermal fluid`. Keep the body short or omit it. Open a PR only when asked.
- **Handoff PRs read as a direct message.** When a PR needs someone else to act, address them by name: a hook (benefit and time cost), a short why, what will change (expected), "<Name>: please do these steps" (numbered, exact clicks and values, a success check each, reply as the last step), then details and testing. Full guide: `plugins/init/styles/handoff-pr.md` (`/pan-init handoff-pr`).

### Comments

- Keep comments laconic: terse, minimal, high-signal. Prefer a short phrase over a sentence.
- Comment the *why* (intent, units, gotchas), not the *what* — never restate what the code already says.
- Do not add narration/step-by-step comments or banner separators; let the code carry the obvious.
- Match the density of the surrounding code; when in doubt, write fewer comments.

### Equations

- Write LaTeX math plainly: `B = \mu_0 n I`, not `B = \mu_0\, n\, I`. No `\,` / `\;` / `\:` / `\!` spacing between variables, and no stray `,` or `;` separating factors — juxtaposition is multiplication.
- Spacing commands only where the output is ambiguous without them: before a differential (`\int f(x)\,dx`) or between a number and its unit (`5\,\mathrm{T}`).

### Where output lives

- **Analyses**: `docs/analyses/<YYYY-MM-DD>-<topic>/` — one directory per run, with `session-log.md` or `cycle-log.md` plus artifacts
- **Reports**: `bin/pan-report` (or `/pan-report`) produces `report.md` + `report.pdf` (LaTeX → pandoc fallback → markdown-only)
- **Plots**: matplotlib style at `<panopticon>/plugins/plot/panopticon.mplstyle`; drop a `pan-plot.mplstyle` in repo root for project-local overrides
- **Project-local memory**: `./.pan/memory/<project>/` via `/pan-memory` — classified, versionable, separate from global auto-memory
- **Knowledge library**: private git-tracked notes in `<panopticon>/knowledge` (separate repo) (`pan kb search` / `pan kb add`), frontmatter `title`, `date`, `tags`, `type`
- **Agent vault**: `<panopticon>/knowledge/memory/<slug>/` is the Claude auto-memory SSOT (editable in Obsidian; `~/.claude/projects/*/memory` symlink here); `knowledge/guidance/` is a generated index of CLAUDE.md files, skills, PAN.md and memory (`pan kb guidance`)

### Knowledge library

A private, cross-project library of durable findings lives at `<panopticon>/knowledge` (its own private repo, cloned inside panopticon).

- Before starting new work, search it: `pan kb search "<topic>"` (MCP: `tool_kb_search`).
- At the end, record each durable finding: `pan kb add "<title>" --body "<text>" --tag <t> --confidence measured|observed|hypothesis` (MCP: `tool_kb_add`).
- One note per finding. Facts and results only; no session narration.
- Hand edits (e.g. in Obsidian) are fine: the nightly librarian skips notes modified within `[knowledge] edit_grace_min` minutes (default 10), retries them next run, and commits hand edits.

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

### Building
- **Cap parallel builds.** `make -j`, `ninja`, `cmake --build -j` are
  resource-hungry; use a fixed cap (e.g. `-j 5` on estate's 8 threads), leaving
  at least 3 threads free so the machine stays usable. Never bare `-j`; go
  higher only with explicit human approval. Don't run heavy builds
  concurrently from parallel agents/worktrees.

### Comments
- **Laconic.** Comment the *why*, not the *what* — the code already says what it
  does. A short clause beats a sentence; a sentence beats a paragraph.
- No comments that restate the next line, no banner blocks, no commented-out
  code. Match the comment density of the surrounding file.

<!-- pan-init:end:cxx -->
