# AltinaEngine ProjectGenerator

CLI tool to generate a new default demo project under `Demo/<Name>/`.

## Usage

```
AltinaEngineProjectGenerator.exe new-demo --root <RepoRoot> --name <DemoName>
```

Common options:
- `--managed <true|false>`: generate a managed script project + `.script` asset (default: true).
- `--update-cmake <true|false>`: append `add_subdirectory(Demo/<Name>)` to repo root `CMakeLists.txt`.
- `--force`: overwrite existing `Demo/<Name>` directory.

