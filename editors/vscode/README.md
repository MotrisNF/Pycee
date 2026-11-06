# pycee for VS Code

- **Highlighting** of `.pce` files: keywords, strings (f-strings with their `{...}` fields, raw and triple-quoted ones, escapes), numbers, comments, declarations (`func`, `obj`, `var`...), the global natives, the error types, modules and method calls. `==`, `!=` and `!`, which pycee does not have, show as invalid.
- **Editing**: `Ctrl+/` comments with `#`, brackets and quotes close themselves, `{` indents the next line.
- **Errors**: when a `.pce` file is opened or saved, `pycee --check` runs and the error it reports is underlined (an error inside an imported module is shown in that module and in the importing file). The command "pycee: Check this file" runs it at any moment.

## Install

```
python3 tools/pack_vscode.py          # writes build/pycee-0.1.0.vsix
code --install-extension build/pycee-0.1.0.vsix
```

or, in VS Code, "Extensions: Install from VSIX..." and choose that file. Then "Developer: Reload Window".

The interpreter is found at the root of the workspace (`pycee-release`, else `pycee`), else in the `PATH`; the setting `pycee.executable` chooses another one. `pycee.checkOnSave` turns the checking off.

## Changing it

The grammar is generated from `include/resolver/natives.h`: after adding a native, an error type or a module, run `python3 tools/pack_vscode.py` again and reinstall. The rest (`package.json`, `language-configuration.json`, `extension.js`) is edited by hand.

Still to come (after phase 12): completion, go to definition and documentation on hover, with a language server that uses pycee's own resolver.
