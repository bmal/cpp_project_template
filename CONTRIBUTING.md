# Contributing

## Suppressing a clang-tidy finding

Fix the finding when you can. When the code is right and the check is wrong for this line, put the check name and a one-sentence reason on the line above it, as below; wrap a block in `NOLINTBEGIN(check-name): reason` and `NOLINTEND(check-name): reason`. A bare `NOLINT` with no check name or no reason is not accepted in review. To turn a check off everywhere, change `.clang-tidy` and record why in [docs/decisions.md](docs/decisions.md).

```cpp
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): bytes in, characters out.
const std::string_view line(reinterpret_cast<const char*>(data), size);
```
