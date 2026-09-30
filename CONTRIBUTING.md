# Contributing

Code follows the [conventions](docs/reference/conventions.md): naming, layout, file headers, and how to suppress a clang-tidy finding.
A choice that changes one of the [decisions](docs/decisions.md) is recorded there, with why and what was rejected.

## Write docs

| Rule | Example |
| --- | --- |
| Task headings are imperative | `## Add a module`, not `## Adding modules` |
| One command per block | A second command gets its own block and its own "when you want this" line |
| Every command has one line above it saying when you want it | "When you want the findings CI would report:" |
| Sentences stay under twenty words | Split at "and" or "which" |
| Tables over paragraphs | Options, labels, and choices go in a table |
| Say nothing a preset name or a `make help` line already says | Link the [reference](docs/reference/README.md) instead |
| A how-to stays under forty lines, the README under sixty | `scripts/selftest.sh docs_limits` checks both |

## Check a template change

After changing `cmake/`, presets, scripts, or install rules, run the template's lifecycle cases:

```bash
scripts/selftest.sh
```

Each case prints `ok <case>`, or `skip <case>: <reason>`; `scripts/selftest.sh --help` lists them.
After changing the `Makefile` or `CMakePresets.json`, regenerate the reference tables:

```bash
make docs
```
