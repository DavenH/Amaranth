# Amaranth Installer

The installer module owns the shared installer UI, EULA, manifest schema, saved
field handling, and artifact copy logic. Product modules own their manifests:

- `cycle/installer.json`
- `oscillo/installer.json`

The installer can be launched with a manifest path:

```sh
open build/standalone-debug/installer/Installer.app --args cycle/installer.json
open build/standalone-debug/installer/Installer.app --args oscillo/installer.json
```

The default manifest is Cycle.

## Packaging

Use `scripts/package_installer_zip.py` to create the zip consumed by a manifest.
The script searches each `--artifact-root` for the artifact `source` paths named
in the manifest and writes those same paths into the zip.

```sh
scripts/package_installer_zip.py \
    --manifest cycle/installer.json \
    --artifact-root build/standalone-release/cycle \
    --artifact-root build/plugin-release/cycle \
    --artifact-root cycle \
    --output build/packages/Cycle.zip

scripts/package_installer_zip.py \
    --manifest oscillo/installer.json \
    --artifact-root build/standalone-release/oscillo \
    --output build/packages/Oscillo.zip
```

CMake also exposes product-owned package targets:

```sh
cmake --build --preset standalone-release --target Oscillo_installer_zip --parallel 10
cmake --build --preset standalone-release --target Cycle_installer_zip --parallel 10
```

`Cycle_installer_zip` expects both standalone and plugin release artifacts to
exist. Build `standalone-release` and `plugin-release` first for a complete
Cycle package.
