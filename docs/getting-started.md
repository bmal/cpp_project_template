# Getting started

Ten minutes from nothing to a named, protected, green repository and a breakpoint hit in your own app.
You need `git`, a GitHub account, and the [GitHub CLI](https://cli.github.com) logged in with `gh auth login`.

## 1. Create the repository

When you start a project, create it from the template and clone it; `order_book` is your name:

```bash
gh repo create order_book --public --clone --template bmal/cpp_project_template
```

The first push runs the Init workflow, which names the project after the repository and commits.
When you want to see it finish, watch it; `found no in progress runs` means it already has:

```bash
cd order_book && gh run watch
```

Then fetch the renamed tree:

```bash
git pull
```

When the repository name is not the project name you want, rename by hand and push:

```bash
scripts/init-project.sh order_book && git add -A && git commit -m "Name the project" && git push
```

When you want a weekend project with only `core`, one app, and one passing test, drop the samples too:

```bash
scripts/init-project.sh order_book --strip-samples && git add -A && git commit -m "Drop the samples" && git push
```

## 2. Install the toolchain

When a machine has never built this project, install the compilers, CMake, vcpkg, and the git hook:

```bash
scripts/bootstrap.sh
```

## 3. Build and test

When you want the debug build and its unit and integration tests:

```bash
make dev
```

You should see `100% tests passed`. The first run builds dependencies and takes a few minutes.
When you want to run the sample app, which echoes `key=value` fields and exits 1 on a malformed line:

```bash
echo "a=1 b=2" | build/dev/bin/order_book_cli
```

## 4. Protect the repository

When you want `main` to take only pull requests that pass CI, run once as the repository's admin:

```bash
make setup-repo
```

It prints one line per change. From now on, push a branch and open a pull request; the `status` check gates the merge.

## 5. Hit a breakpoint

When you want to debug, open the folder in VS Code:

```bash
code .
```

1. Accept **Install** on the recommended extensions prompt.
2. Pick `dev` under **Configure** in the CMake panel.
3. Run **CMake: Set Launch/Debug Target** and pick `order_book_cli`.
4. Open `apps/order_book_cli/main.cpp` and click left of the line in `main` that prints `version_banner()`.
5. Press F5.

You should see the debugger stop on that line with the locals shown. Next, see the [how-to guides](how-to/README.md).
