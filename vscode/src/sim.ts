// A project's scenarios as tests: what the server lists (baker/scenarios), how the game's
// binary is asked to run them (`sim --json`), and what its results (baker.sim/1) say about
// each scenario. No vscode here, so the tests can load it.

export interface Declared {
    name: string;
    uri: string;
    line: number;  // 0-based
    desc: string;
    objectives: string[];
    median: boolean;
}

export interface Suite {
    project: string;    // the project file's URI
    directory: string;  // its directory, where the game runs
    tool: string;       // the binary the project file names; empty if none
    scenarios: Declared[];
    objectives: Declared[];
}

export interface Message {
    text: string;
    file?: string;  // a path, where the problem is declared or happened
    line?: number;  // 1-based
}

export interface Outcome {
    passed: boolean;
    messages: Message[];
}

// The binary to run: the project file's, else the setting's, in which ${project} is the
// project directory's name (stacks_${project} for Baker's stacks). Relative to the project.
export function toolFor(suite: Suite, setting: string, join: (a: string, b: string) => string,
                        isAbsolute: (p: string) => boolean): string {
    const name = suite.directory.split(/[\\/]/).filter((p) => p).pop() ?? "";
    const tool = suite.tool || setting.split("${project}").join(name);
    if (!tool) return "";
    return isAbsolute(tool) ? tool : join(suite.directory, tool);
}

export function simArgs(scenarios: string[]): string[] {
    const args = ["sim"];
    for (const s of scenarios) args.push("--scenario", s);
    args.push("--json");
    return args;
}

// baker.sim/1, as far as the test view reads it.
export interface SimProblem {
    message: string;
    file?: string;
    line?: number;
}
export interface SimRun {
    scenario: string;
    seed: number;
    passed: boolean;
    errors?: SimProblem[];
    objectives?: {name: string; file?: string; line?: number; value: number | null;
                  pass: boolean; over: string; error: SimProblem | null}[];
}
export interface SimResults {
    schema: string;
    status: string;
    runs?: SimRun[];
    medians?: {scenario: string; objective: string; file?: string; line?: number;
               value: number | null; seeds: number; pass: boolean; error: SimProblem | null}[];
}

const num = (v: unknown) => (typeof v === "number" ? String(v) : "no number");

// Each scenario's verdict in `results` (baker.sim/1): it passes when every run passed and
// every median the scenario is judged by passed. A scenario the results do not mention is
// left out.
export function outcomes(results: SimResults): Map<string, Outcome> {
    const out = new Map<string, Outcome>();
    const of = (s: string) => {
        let o = out.get(s);
        if (!o) out.set(s, (o = {passed: true, messages: []}));
        return o;
    };
    for (const run of results?.runs ?? []) {
        const o = of(run.scenario);
        const seed = `seed ${run.seed}`;
        for (const e of run.errors ?? []) {
            o.passed = false;
            o.messages.push({text: `${seed}: ${e.message}`, file: e.file, line: e.line});
        }
        for (const obj of run.objectives ?? []) {
            if (obj.over !== "seed" || obj.pass) continue;
            o.passed = false;
            const why = obj.error?.message ?? `value ${num(obj.value)}`;
            o.messages.push({text: `${obj.name} failed (${seed}): ${why}`, file: obj.file,
                             line: obj.line});
        }
        if (run.passed === false) o.passed = false;
    }
    for (const m of results?.medians ?? []) {
        const o = of(m.scenario);
        if (m.pass) continue;
        o.passed = false;
        const why = m.error?.message ?? `median ${num(m.value)} over ${m.seeds} seeds`;
        o.messages.push({text: `${m.objective} failed: ${why}`, file: m.file, line: m.line});
    }
    return out;
}
