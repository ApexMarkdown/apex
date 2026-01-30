# Debugging Apex

## Using the debugger in VS Code

To step through apex with the debugger:

1. **Build with debug symbols** (default when using `make` with Debug build type):
   ```bash
   cd /path/to/apex
   make clean && make
   ```
   Or with CMake directly: `cmake -DCMAKE_BUILD_TYPE=Debug .. && make`

2. **Install a C/C++ debugger extension** (if not already):
   - **CodeLLDB** (recommended on macOS): Extension ID `vadimcn.vscode-lldb`
   - Or **C/C++** (Microsoft): Extension ID `ms-vscode.cpptools`

3. **Use the launch configurations** in `.vscode/launch.json`:
   - **Run and Debug** (sidebar or `Cmd+Shift+D`), then pick a configuration and press F5 or the green play button.
   - **Apex CLI (ref-image+IAL fixture)** – runs `build/apex` with `tests/fixtures/debug_ref_image_ial.md` (the ref image + IAL link case).
   - **Apex CLI (current file as input)** – runs apex with whatever file is currently active in the editor.
   - **Apex test_runner (multimarkdown_image_attributes)** – runs the image-attributes test suite under the debugger.

4. **Set breakpoints** by clicking in the gutter next to line numbers in:
   - `src/apex.c` – e.g. in `apex_markdown_to_html` (image_attrs block, IAL block, or after rendering).
   - `src/extensions/ial.c` – e.g. in `apex_preprocess_image_attributes` (expansion loop, ref-def handling) or in `process_span_ial_in_container` (IAL application to links).
   - `src/html_renderer.c` – e.g. in `apex_render_html_with_attributes` (attribute injection).

5. **Step through**: F10 (step over), F11 (step into), Shift+F11 (step out). Inspect variables in the **Variables** panel or hover.

## Trace logging (no debugger)

To see how the ref-image + IAL markup moves through the pipeline without stepping:

```bash
export APEX_DEBUG_PIPELINE=1
./build/apex tests/fixtures/debug_ref_image_ial.md
```

Or with piped input (paste your markdown, then Ctrl+D):

```bash
export APEX_DEBUG_PIPELINE=1
./build/apex -
```

Log lines (on stderr) show:

- **preprocess_image_attributes in** – markdown before image-attribute preprocessing.
- **expand ref [refname]** – when a reference-style image is expanded to inline.
- **preprocess_image_attributes out** – markdown after image preprocessing (ref def removed, image expanded).
- **after image_attrs** – markdown after image-attribute step in the main pipeline.
- **after ial_preprocess** – markdown after IAL preprocessing.
- **markdown to parse** – exact string fed to the cmark parser.
- **IAL applied to link** – when span IAL is applied to a link node (with attrs snippet).
- **rendered html len** – length of HTML after render; if short, the first 500 chars are printed.

Use this to see where content disappears (e.g. empty “markdown to parse” or “rendered html len=0”).

## CLI from terminal with lldb

For a quick run under lldb without the VS Code UI:

```bash
lldb build/apex
(lldb) run tests/fixtures/debug_ref_image_ial.md
# When it hits a breakpoint or you interrupt (Ctrl+C):
(lldb) bt
(lldb) frame variable
(lldb) quit
```

See also `debug_test.sh` for running the test runner under lldb.
