# Generate the API Documentation

N-Type uses Doxygen comments in its C++ headers and sources. The repository's
`Doxyfile` scans the client, server, protocol, engine, and shared code and produces
a browsable HTML reference. Graphviz is used for class and dependency diagrams.

Run every command below from the repository root.

## Linux

On Debian, Ubuntu, or Pop!_OS, install Doxygen and Graphviz:

```sh
sudo apt update
sudo apt install doxygen graphviz
```

Generate the documentation:

```sh
doxygen Doxyfile
```

Open it in the default browser:

```sh
xdg-open docs/api/html/index.html
```

If `xdg-open` is unavailable, open `docs/api/html/index.html` manually in a browser.

## Windows

Open PowerShell in the repository root. With Windows Package Manager (`winget`),
install Doxygen and Graphviz:

```powershell
winget install --id Doxygen.Doxygen --exact
winget install --id Graphviz.Graphviz --exact
```

Close and reopen PowerShell after installation so the updated `PATH` is loaded,
then verify both programs:

```powershell
doxygen --version
dot -V
```

Generate and open the documentation:

```powershell
doxygen Doxyfile
Start-Process .\docs\api\html\index.html
```

If `doxygen` or `dot` is not found, install them from their official Windows
installers and add their `bin` directories to `PATH`, then reopen PowerShell.

## Output and cleanup

The entry page is always:

```text
docs/api/html/index.html
```

Generated files under `docs/api/` are ignored by Git. To regenerate from scratch,
delete that directory and run `doxygen Doxyfile` again:

```sh
rm -rf docs/api
doxygen Doxyfile
```

In Windows PowerShell:

```powershell
Remove-Item -Recurse -Force .\docs\api
doxygen Doxyfile
```

Do not commit generated HTML. Commit source comments, `Doxyfile`, and this guide.

## Common problems

- **No diagrams:** install Graphviz and confirm `dot -V` works.
- **A class or function is missing:** ensure its file is below one of the `INPUT`
  directories in `Doxyfile` and uses a supported `.hpp` or `.cpp` extension.
- **A warning reports malformed documentation:** check `@param`, `@return`, code
  blocks, and comment delimiters around the location printed by Doxygen.
- **Stale pages remain:** remove `docs/api/` before generating again.
