// The server's read-only views of a project (baker/merged, baker/layerEffect): their text, and
// for each line that comes from a file, where. No vscode here, so the tests can load it.

export interface ViewSource {
    line: number;      // 0-based, in the view
    uri: string;       // the file the line's value is written in
    fileLine: number;  // 1-based
}

export interface View {
    text: string;
    sources: ViewSource[];  // ascending by line
}

// The source of the view's 0-based `line`, if it has one.
export function sourceAt(view: View, line: number): ViewSource | undefined {
    let lo = 0;
    let hi = view.sources.length;
    while (lo < hi) {
        const mid = (lo + hi) >> 1;
        if (view.sources[mid].line < line) lo = mid + 1;
        else hi = mid;
    }
    return lo < view.sources.length && view.sources[lo].line === line ? view.sources[lo] : undefined;
}

// The view document's path for a project: merged.dta, or <layer>.changes for a layer's view.
export function viewPath(projectDir: string, layer?: string): string {
    const name = projectDir.split(/[\\/]/).filter((p) => p).pop() ?? "project";
    return layer === undefined ? `/${name}/merged.dta` : `/${name}/${layer}.changes`;
}
