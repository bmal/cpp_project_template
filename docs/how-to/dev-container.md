# Work in the Linux dev container

The root `Dockerfile` builds a Linux image with every tool `scripts/bootstrap.sh` installs.
`.devcontainer/devcontainer.json` opens the repository in it with the recommended extensions.

## Open the repository in the container

When you want Linux, GCC, or MemorySanitizer on a Mac, open the command palette and run:

```text
Dev Containers: Reopen in Container
```

The first open builds the image, then configures and builds `dev`.
The Linux targets of `make help` then work in the VS Code terminal.

Container builds live in a Docker volume mounted at `build/`, apart from the host's builds.
A second volume keeps the vcpkg binary cache and the MemorySanitizer libc++ across rebuilds.

## Build the image without VS Code

When you want to reproduce a Linux failure from the command line, build the image:

```bash
docker build -t myproj-dev .
```

When you want the `dev` workflow on Linux, run it inside the image:

```bash
docker run --rm -v "$PWD:/workspaces/myproj" -v /workspaces/myproj/build -w /workspaces/myproj myproj-dev make dev
```

## Change a tool version

When you change a pin at the top of `scripts/bootstrap.sh`, check the Dockerfile still installs nothing itself:

```bash
scripts/selftest.sh container_files
```
