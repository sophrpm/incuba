import type { ExecutionStats } from "../types/analysis";

interface OutputPanelProps {
    messages: string[];
    stats: ExecutionStats | null;
}

function OutputPanel({ messages, stats }: OutputPanelProps) {
    const summary = stats ? `\n----------------------------------------\nresumen de ejecucion\ncomandos exitosos: ${stats.successfulCommands}\nerrores lexicos: ${stats.lexicalErrors}\nerrores sintacticos: ${stats.syntaxErrors}\n----------------------------------------` : "";
    return (
        <section className="output-panel-container">
            <div className="panel-header">
                <div>
                    <p className="panel-label">Salida de Comandos</p>
                    <h2 className="panel-title">Output</h2>
                </div>
            </div>
            <pre className="output-panel" aria-live="polite">{messages.length === 0 && !stats ? "Output de comandos..." : messages.join("\n") + summary}</pre>
        </section>
    );
}
export default OutputPanel;
