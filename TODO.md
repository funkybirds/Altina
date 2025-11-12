# AltinaEngine TODO

## Commit message format

We enforce a simple conventional-like commit message format. The rules:

- Commit subject (first line) must follow: `<type>(optional-scope): <subject>`
- Allowed types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`, `build`, `ci`
- Subject line should be non-empty and at most 72 characters.

Example:

```
feat(renderer): add initial Vulkan renderer
```

Installation:

1. Run the included script to enable hooks for this repo:

```powershell
.\Scripts\InstallGitHooks.ps1
```

2. Hooks live in `./.githooks` and include `commit-msg` and `commit-msg.ps1` to validate messages locally.

If you prefer CI enforcement as well, we can add a GitHub Actions job to lint commit messages on pull requests.

