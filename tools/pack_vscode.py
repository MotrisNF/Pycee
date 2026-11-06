#!/usr/bin/env python3
"""
Packs editors/vscode into build/pycee-<version>.vsix, the file VS Code
installs ("Extensions: Install from VSIX...", or
code --install-extension build/pycee-0.1.0.vsix). A .vsix is a zip with a
manifest; this writes it without vsce (no npm, no network). The grammar is
generated first, so it always matches include/resolver/natives.h.

    python3 tools/pack_vscode.py
"""

import json
import os
import subprocess
import sys
import zipfile
from xml.sax.saxutils import escape

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "editors", "vscode")

subprocess.run([sys.executable, os.path.join(ROOT, "tools", "gen_vscode_grammar.py")],
               check=True)

with open(os.path.join(SRC, "package.json")) as f:
    pkg = json.load(f)

manifest = f"""<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011" xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
  <Metadata>
    <Identity Language="en-US" Id="{pkg['name']}" Version="{pkg['version']}" Publisher="{pkg['publisher']}" />
    <DisplayName>{escape(pkg['displayName'])}</DisplayName>
    <Description xml:space="preserve">{escape(pkg['description'])}</Description>
    <Categories>{escape(','.join(pkg['categories']))}</Categories>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="{escape(pkg['engines']['vscode'])}" />
      <Property Id="Microsoft.VisualStudio.Code.ExtensionKind" Value="workspace" />
    </Properties>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code" />
  </Installation>
  <Dependencies />
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
  </Assets>
</PackageManifest>
"""

content_types = """<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension=".json" ContentType="application/json" />
  <Default Extension=".js" ContentType="application/javascript" />
  <Default Extension=".md" ContentType="text/markdown" />
  <Default Extension=".vsixmanifest" ContentType="text/xml" />
</Types>
"""

os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
out = os.path.join(ROOT, "build", f"{pkg['name']}-{pkg['version']}.vsix")
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", content_types)
    z.writestr("extension.vsixmanifest", manifest)
    for folder, _, names in os.walk(SRC):
        for name in sorted(names):
            full = os.path.join(folder, name)
            z.write(full, "extension/" + os.path.relpath(full, SRC))
print("wrote", os.path.relpath(out, ROOT))
