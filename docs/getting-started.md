# Getting started

Ten minutes from nothing to a named, protected, green repository and a breakpoint hit in your own app.
You need `git`, a GitHub account, and the [GitHub CLI](https://cli.github.com) logged in with `gh auth login`.

## 1. Create the repository

When you start a project, create it from the template, wait for Init to name it, then fetch the rename:

```bash
gh repo create order_book --public --clone --template bmal/cpp_project_template && cd order_book && until id=$(gh run list --json databaseId,workflowName -q 'map(select(.workflowName == "Init"))[0].databaseId') && [ -n "$id" ]; do sleep 2; done && gh run watch "$id" && git pull
```

Here `order_book` is your name. Init names the project after the repository and starts CI.

When the repository name is not the project name you want, rename by hand and push:

```bash
scripts/bootstrap.sh --format-tools && scripts/init-project.sh order_book && git add -A && git commit -m "Name the project" && git push
```

When you want a weekend project with only `core`, one app, and one passing test, drop the samples:

```bash
scripts/bootstrap.sh --format-tools && scripts/init-project.sh order_book --strip-samples && git add -A && git commit -m "Drop the samples" && git push
```

## 2. Build and test

When a machine has never built this project, install the toolchain and git hook, then build and test `dev`:

```bash
scripts/bootstrap.sh && make dev
```

You should see `100% tests passed`. The first run builds dependencies and takes a few minutes.

## 3. Protect the repository

When you want `main` to take only pull requests that pass CI, run once as the repository's admin:

```bash
make setup-repo
```

## 4. Hit a breakpoint

When you want to debug, open the folder in VS Code:

```bash
code .
```

1. Click **Yes, I trust the authors**, then **Install** on the recommended extensions prompt.
2. Pick **Debug build with tests**, which is `dev`, when CMake Tools asks for a configure preset.
3. Run **CMake: Set Launch/Debug Target** and pick `order_book_cli`.
4. Open `apps/order_book_cli/main.cpp` and click left of the line in `main` that prints `version_banner()`.
5. Press F5. The debugger stops on that line with the locals shown. Next, see the [how-to guides](how-to/README.md).
