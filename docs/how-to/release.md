# Cut a release

A tag `v<version>` whose version equals `project(VERSION)` in `CMakeLists.txt` becomes a GitHub release.
`.github/workflows/release.yaml` builds and tests `release`, runs CPack, and attaches `MyProj-<version>-Linux.tar.gz`.

When you want to check a tag before you push it:

```bash
scripts/check-release-tag.sh v1.2.0
```

It prints the version, or names both versions when they differ, which is also how the workflow fails.

When you want to publish a version, bump `project(VERSION)` on `main` first, then tag that commit:

```bash
git tag v1.2.0 && git push origin v1.2.0
```

When you want a release candidate, add a suffix; GitHub marks it as a pre-release:

```bash
git tag v1.2.0-rc.1 && git push origin v1.2.0-rc.1
```

`release` builds for the runner's CPU, so an archive runs only where the CPU has the same instructions.
