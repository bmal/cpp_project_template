# Pull template updates

Your repository does not share history with the template, so an update is the template's diff between two commits, renamed.
The commands use `order_book` for your project; the placeholders are split with `''` so a rename never rewrites them.

When you want the template's latest `main`, fetch it:

```bash
git fetch https://github.com/bmal/cpp_project_template.git main
```

When this is your first update, the base is the template commit your repository started from:

```bash
git log -1 --format=%h --before="$(git log --reverse --format=%cI | head -n 1)" FETCH_HEAD
```

After that, the base is the commit named in your last update's subject:

```bash
git log -1 --grep='^Pull template' --format=%s
```

When you have the base, apply the template's changes since it with your names:

```bash
git diff <base> FETCH_HEAD | perl -pe 's/myp''roj/order_book/g; s/MyP''roj/OrderBook/g; s/MYP''ROJ/ORDER_BOOK/g' | git apply --reject
```

You should see one line per file. A hunk that does not fit goes to `<file>.rej`; apply it by hand, or delete it.
Changes to `README.md` and `.github/workflows/init.yaml` never fit, because Init replaced one and deleted the other.
When the build and tests pass, commit with the template commit in the subject:

```bash
make dev && find . -name '*.rej' -delete && git add -A && git commit -m "Pull template $(git rev-parse --short FETCH_HEAD)"
```
