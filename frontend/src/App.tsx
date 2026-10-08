import {
    useEffect,
    useState,
} from "react";

import Buttons from "./components/Buttons";
import CommandEditor from "./components/CommandEditor";
import FileChooser from "./components/FileChooser";
import Header from "./components/Header";
import OutputPanel from "./components/OutputPanel";
import ServerStatus from "./components/ServerStatus";

import {
    analyzeCommands,
    checkServerStatus,
} from "./services/commandAPI";

import "./styles/app.css";
import type { ExecutionStats} from "./types/analysis";

function App() {
    // conserva el nombre del archi
    const [fileName, setFileName] = useState("");

    // guarda el comandos en editr
    const [commandText, setCommandText] = useState("");

    // guarda los mensajes de outut
    const [messages, setMessages] = useState<string[]>([]);

    const [isOnline, setIsOnline] = useState(false);
    const [busy, setBusy] = useState(false);
    const [pending, setPending] = useState(false);
    const [answer, setAnswer] = useState("");
    const [stats, setStats] = useState<ExecutionStats | null>(null);

    useEffect(() => {
        const verifyServer = async (): Promise<void> => {
            const serverAvailable =
                await checkServerStatus();

            setIsOnline(serverAvailable);
        };

        verifyServer();
    }, []);

    //lee archivo y hace display en el eidto
    const handleFileSelected = async (event: React.ChangeEvent<HTMLInputElement>): Promise<void> => {
        // obtiene el inputfile
        const selectedFile =
            event.target.files?.[0];

        if (!selectedFile) {
            return;
        }

        const selectedFileName =
            selectedFile.name.toLowerCase();

        const validExtension =
            selectedFileName.endsWith(".smia");

        if (!validExtension) {
            setMessages([
                "Error: solo se permiten archivos con extensión .smia.",
            ]);

            return;
        }

        try {
            //lee archivo
            const content =
                await selectedFile.text();

            setFileName(selectedFile.name);

            //comandos en el editor
            setCommandText(content);

            //info mesage
            setMessages([
                `Archivo "${selectedFile.name}" cargado correctamente.`,
            ]);
        } catch {
            setMessages([
                "Error: no se pudo leer el archivo seleccionado.",
            ]);
        }
    };

    
    const handleAnalyze = async (): Promise<void> => {
        // evita enviar un editor vacío.
        if (busy || (pending ? answer.trim().length === 0 : commandText.length === 0)) {
            return;
        }

        
        setBusy(true);
        const serverAvailable = await checkServerStatus();

        setIsOnline(serverAvailable);

        if (!serverAvailable) {
            setBusy(false);
            setMessages([
                "Error: no se pudo conectar con el backend.",
            ]);

            return;
        }

        try {
            
            const analysisResult =
                await analyzeCommands(pending ? answer : commandText);

            
            setMessages(previous => pending ? [...previous, ...analysisResult.messages] : analysisResult.messages);
            setStats(analysisResult.stats);
            setPending(analysisResult.pendingConfirmation);
            setAnswer("");
        } catch {
            
            setIsOnline(false);

            setMessages([
                "Error: ocurrió un problema al comunicarse con el backend.",
            ]);
        } finally {
            setBusy(false);
        }
    };

    //lmpia el editor y sus resultados
    const handleClear = (): void => {
        setFileName("");
        setCommandText("");
        setMessages([]);
        setStats(null);
    };

    return (
        <div className="app-shell">
            <Header />

            <main className="app-main">
                <div className="toolbar">
                    <ServerStatus
                        isOnline={isOnline}
                    />
                </div>

                <FileChooser
                    fileName={fileName}
                    onFileSelect={handleFileSelected}
                />

                <CommandEditor
                    value={commandText}
                    onChange={setCommandText}
                />

                <Buttons
                    disabled={
                        busy || pending || commandText.length === 0
                    }
                    onAnalyze={handleAnalyze}
                    onClear={handleClear}
                />

                <OutputPanel
                    messages={messages}
                    stats={pending ? null : stats}
                />
                {pending && (
                    <form className="panel" onSubmit={event => { event.preventDefault(); void handleAnalyze(); }}>
                        <label htmlFor="confirmation">¿Desea sobrescribirlo? [y/n]: </label>
                        <input id="confirmation" value={answer} onChange={event => setAnswer(event.target.value)} disabled={busy} autoFocus autoComplete="off" />
                        <button type="submit" disabled={busy || !answer.trim()}>Responder</button>
                    </form>
                )}
            </main>

            <footer className="app-footer">
                Proyecto 1 · Analizador léxico y sintáctico EXT2
            </footer>
        </div>
    );
}

export default App;
