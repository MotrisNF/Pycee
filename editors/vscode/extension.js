// pycee for VS Code: runs `pycee --check` on a .pce file when it is opened
// or saved, and underlines the error it reports (pycee stops at the first
// error, so there is at most one). No dependencies: only the vscode API
// and Node's own modules.

const vscode = require("vscode");
const cp = require("child_process");
const fs = require("fs");
const path = require("path");

let diagnostics;

// The interpreter: the setting, else pycee-release or pycee at the root of
// a workspace folder, else "pycee" in the PATH.
function executable(doc) {
    const setting = vscode.workspace.getConfiguration("pycee").get("executable");
    if (setting) {
        return setting;
    }
    const folder = vscode.workspace.getWorkspaceFolder(doc.uri);
    const roots = folder ? [folder.uri.fsPath] : [];
    roots.push(path.dirname(doc.uri.fsPath));
    for (const root of roots) {
        for (const name of ["pycee-release", "pycee"]) {
            const candidate = path.join(root, name);
            if (fs.existsSync(candidate)) {
                return candidate;
            }
        }
    }
    return "pycee";
}

// "NameError: name 'x' is not defined (/a/b.pce, line 3)", then the line of
// code and a line of ^ under the error. Both lines start with four spaces;
// the ^ line has one character per byte of the source before the error.
const HEADER = /^([A-Za-z]+)(?:: (.*))? \((.*), line (\d+)\)$/;

function parse(output) {
    const lines = output.split("\n");
    for (let i = 0; i < lines.length; i++) {
        const m = HEADER.exec(lines[i]);
        if (!m) {
            continue;
        }
        const caret = lines[i + 2] || "";
        const start = caret.indexOf("^");
        return {
            type: m[1],
            message: m[2] || "",
            file: m[3],
            line: parseInt(m[4], 10) - 1,
            byteCol: start >= 4 ? start - 4 : 0,
            byteLen: start >= 0 ? caret.length - start : 1,
        };
    }
    return null;
}

// pycee counts bytes; VS Code counts UTF-16 units: convert on the line.
function range(text, line, byteCol, byteLen) {
    const bytes = Buffer.from(text, "utf8");
    const startCol = bytes.subarray(0, byteCol).toString("utf8").length;
    const endCol = bytes.subarray(0, byteCol + byteLen).toString("utf8").length;
    return new vscode.Range(line, startCol, line, Math.max(endCol, startCol + 1));
}

function lineText(file, line) {
    const open = vscode.workspace.textDocuments.find((d) => d.uri.fsPath === file);
    if (open) {
        return line < open.lineCount ? open.lineAt(line).text : "";
    }
    try {
        return fs.readFileSync(file, "utf8").split("\n")[line] || "";
    } catch (e) {
        return "";
    }
}

function check(doc) {
    if (doc.languageId !== "pycee" || doc.uri.scheme !== "file") {
        return;
    }
    const file = doc.uri.fsPath;
    const exe = executable(doc);
    cp.execFile(exe, ["--check", file], { cwd: path.dirname(file), timeout: 10000 },
        (error, stdout, stderr) => {
            diagnostics.delete(doc.uri);
            if (error && error.code === "ENOENT") {
                vscode.window.showWarningMessage(
                    `pycee: cannot run "${exe}". Build it (make release) or set pycee.executable.`);
                return;
            }
            const found = parse(stdout + stderr);
            if (!found) {
                return;
            }
            const where = range(lineText(found.file, found.line), found.line,
                found.byteCol, found.byteLen);
            const text = found.message ? `${found.type}: ${found.message}` : found.type;
            const d = new vscode.Diagnostic(where, text, vscode.DiagnosticSeverity.Error);
            d.source = "pycee";
            if (path.resolve(found.file) === path.resolve(file)) {
                diagnostics.set(doc.uri, [d]);
                return;
            }
            // The error is in a module this file imports: shown in both.
            const other = vscode.Uri.file(path.resolve(path.dirname(file), found.file));
            diagnostics.set(other, [d]);
            const here = new vscode.Diagnostic(new vscode.Range(0, 0, 0, 1),
                `error in ${path.basename(found.file)}, line ${found.line + 1}: ${text}`,
                vscode.DiagnosticSeverity.Error);
            here.source = "pycee";
            here.relatedInformation = [new vscode.DiagnosticRelatedInformation(
                new vscode.Location(other, where), text)];
            diagnostics.set(doc.uri, [here]);
        });
}

function enabled() {
    return vscode.workspace.getConfiguration("pycee").get("checkOnSave");
}

function activate(context) {
    diagnostics = vscode.languages.createDiagnosticCollection("pycee");
    context.subscriptions.push(diagnostics);
    context.subscriptions.push(vscode.workspace.onDidOpenTextDocument((d) => enabled() && check(d)));
    context.subscriptions.push(vscode.workspace.onDidSaveTextDocument((d) => enabled() && check(d)));
    context.subscriptions.push(vscode.workspace.onDidCloseTextDocument((d) => diagnostics.delete(d.uri)));
    context.subscriptions.push(vscode.commands.registerCommand("pycee.check", () => {
        const editor = vscode.window.activeTextEditor;
        if (editor) {
            check(editor.document);
        }
    }));
    if (enabled()) {
        vscode.workspace.textDocuments.forEach(check);
    }
}

function deactivate() {}

module.exports = { activate, deactivate };
