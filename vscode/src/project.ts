// Which files are Baker's: a .dta file with a baker-project.dta at or above it. Kept apart
// from the extension so it can be tested without VS Code.
import * as fs from "fs";
import * as path from "path";

export const PROJECT_FILE = "baker-project.dta";

// The directory of the nearest project file at or above `file`, or undefined.
export function projectOf(file: string, exists: (p: string) => boolean = fs.existsSync):
    string | undefined {
    for (let dir = path.dirname(file); ; ) {
        if (exists(path.join(dir, PROJECT_FILE))) return dir;
        const parent = path.dirname(dir);
        if (parent === dir) return undefined;
        dir = parent;
    }
}

// Whether `file` should be shown as Baker.
export function isBaker(file: string, exists: (p: string) => boolean = fs.existsSync): boolean {
    return file.toLowerCase().endsWith(".dta") && projectOf(file, exists) !== undefined;
}
