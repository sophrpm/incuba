import { useEffect, useState } from "react";
import { apiRequest, getSession, loadReport } from "../services/commandAPI";
import type { DiskInfo, FileInfo, JournalInfo, ReportInfo, SessionInfo } from "../services/commandAPI";

interface Props { revision: number; onChange: () => void }

function Explorer({ revision, onChange }: Props) {
    const [session, setSession] = useState<SessionInfo | null>(null);
    const [disks, setDisks] = useState<DiskInfo[]>([]);
    const [diskId, setDiskId] = useState("");
    const [partitionId, setPartitionId] = useState("");
    const [user, setUser] = useState("");
    const [password, setPassword] = useState("");
    const [tab, setTab] = useState("files");
    const [path, setPath] = useState("/");
    const [entries, setEntries] = useState<FileInfo[]>([]);
    const [content, setContent] = useState<{ name: string; text: string } | null>(null);
    const [journal, setJournal] = useState<JournalInfo[]>([]);
    const [reports, setReports] = useState<ReportInfo[]>([]);
    const [reportUrl, setReportUrl] = useState("");
    const [error, setError] = useState("");
    const [busy, setBusy] = useState(false);
    const selectedDisk = disks.find(disk => disk.id === diskId);
    const mounted = disks.flatMap(disk => disk.partitions).filter(partition => partition.mounted);

    useEffect(() => {
        let cancelled = false;
        async function refresh() {
            try {
                const current = await getSession();
                const data = await apiRequest<{ disks: DiskInfo[] }>("/api/disks");
                if (cancelled) return;
                setSession(current);
                setDisks(data.disks);
                if (!current.active) { setEntries([]); setContent(null); setPath("/"); }
                else setPartitionId(current.partitionId);
            } catch (failure) { if (!cancelled) setError(String(failure)); }
        }
        void refresh();
        return () => { cancelled = true; };
    }, [revision]);

    useEffect(() => {
        let cancelled = false;
        async function refreshView() {
            setError("");
            setContent(null);
            try {
                if (tab === "files" && session?.active) {
                    const data = await apiRequest<{ entries: FileInfo[] }>(`/api/files?path=${encodeURIComponent(path)}`);
                    if (!cancelled) setEntries(data.entries);
                } else if (tab === "journal" && partitionId) {
                    setJournal([]);
                    const data = await apiRequest<{ entries: JournalInfo[] }>(`/api/journaling/${encodeURIComponent(partitionId)}`);
                    if (!cancelled) setJournal(data.entries);
                } else if (tab === "reports") {
                    const data = await apiRequest<{ reports: ReportInfo[] }>("/api/reports");
                    if (!cancelled) setReports(data.reports);
                }
            } catch (failure) { if (!cancelled) { setError(String(failure)); setEntries([]); } }
        }
        void refreshView();
        return () => { cancelled = true; };
    }, [tab, path, partitionId, session?.active, revision]);

    useEffect(() => () => { if (reportUrl) URL.revokeObjectURL(reportUrl); }, [reportUrl]);

    async function login(event: React.FormEvent) {
        event.preventDefault(); setBusy(true); setError("");
        try {
            await apiRequest("/api/login", { id: partitionId, user, password });
            setPassword(""); setPath("/"); onChange();
        } catch (failure) { setError(String(failure)); }
        finally { setBusy(false); }
    }
    async function logout() {
        setBusy(true);
        try { await apiRequest("/api/logout", {}); setContent(null); setEntries([]); onChange(); }
        catch (failure) { setError(String(failure)); }
        finally { setBusy(false); }
    }
    async function openFile(file: FileInfo) {
        if (file.type === "directory") { setPath(file.path); return; }
        try {
            const data = await apiRequest<{ content: string }>(`/api/files/content?path=${encodeURIComponent(file.path)}`);
            setContent({ name: file.path, text: data.content }); setError("");
        } catch (failure) { setContent(null); setError(String(failure)); }
    }
    async function openReport(report: ReportInfo) {
        try { setReportUrl(await loadReport(report.url)); setError(""); }
        catch (failure) { setError(String(failure)); }
    }

    return <section className="explorer panel">
        <h2>Discos y sistema de archivos</h2>
        {session?.active ? <div className="session-bar">Sesión: <strong>{session.user}</strong> · {session.partitionId} <button disabled={busy} onClick={() => void logout()}>Cerrar sesión</button></div> :
            <form className="login-form" onSubmit={login}>
                <label>Partición<select required value={partitionId} onChange={event => setPartitionId(event.target.value)}><option value="">Seleccione ID</option>{mounted.map(partition => <option key={partition.id} value={partition.id}>{partition.id} · {partition.name}</option>)}</select></label>
                <label>Usuario<input required autoComplete="username" value={user} onChange={event => setUser(event.target.value)} /></label>
                <label>Contraseña<input required type="password" autoComplete="current-password" value={password} onChange={event => setPassword(event.target.value)} /></label>
                <button disabled={busy} type="submit">Iniciar sesión</button>
            </form>}
        <div className="disk-list">{disks.length === 0 && <p>Sin discos. Cree uno mediante MKDISK.</p>}{disks.map(disk => <button className={diskId === disk.id ? "selected" : ""} key={disk.id} onClick={() => setDiskId(disk.id)}>{disk.name} · {(disk.size / 1048576).toFixed(2)} MiB · Fit {disk.fit}<small>{new Date(disk.date * 1000).toLocaleString()}</small></button>)}</div>
        {selectedDisk && <div className="table-scroll"><table><thead><tr><th>Partición</th><th>Tipo</th><th>Tamaño</th><th>Fit</th><th>Estado</th><th>Formato / ID</th></tr></thead><tbody>{selectedDisk.partitions.map(partition => <tr key={partition.name}><td><button disabled={!partition.mounted} onClick={() => setPartitionId(partition.id)}>{partition.name}</button></td><td>{partition.type}</td><td>{partition.size} B</td><td>{partition.fit}</td><td>{partition.mounted ? "Montada" : "Desmontada"}</td><td>{partition.filesystem ? `EXT${partition.filesystem}` : "Sin formato"} {partition.id}</td></tr>)}</tbody></table></div>}
        <nav className="view-tabs" aria-label="Vistas del sistema">{[["files", "Archivos"], ["journal", "Journaling"], ["reports", "Reportes"]].map(([value, name]) => <button key={value} className={tab === value ? "selected" : ""} onClick={() => setTab(value)}>{name}</button>)}<button onClick={onChange}>Actualizar</button></nav>
        {error && <p role="alert" className="error-message">{error}</p>}
        {tab === "files" && (session?.active ? <>
            <p>Partición de sesión: {session.partitionId} · Explorador de solo lectura</p>
            <div className="path-bar"><button disabled={path === "/"} onClick={() => setPath(path.slice(0, path.lastIndexOf("/")) || "/")}>Subir</button><code>{path}</code></div>
            <div className="table-scroll"><table><thead><tr><th>Nombre</th><th>Tipo</th><th>Bytes</th><th>UID</th><th>GID</th><th>Permisos</th></tr></thead><tbody>{entries.map(file => <tr key={file.path}><td><button onClick={() => void openFile(file)}>{file.name}</button></td><td>{file.type === "directory" ? "Carpeta" : "Archivo"}</td><td>{file.size}</td><td>{file.uid}</td><td>{file.gid}</td><td>{file.permissions}</td></tr>)}</tbody></table></div>
            {!entries.length && !error && <p>Carpeta vacía.</p>}
            {content && <section><h3>{content.name}</h3><pre className="file-content">{content.text || "(Archivo vacío)"}</pre></section>}
        </> : <p>Inicie sesión para navegar y leer los archivos.</p>)}
        {tab === "journal" && <><label>Partición<select value={partitionId} onChange={event => setPartitionId(event.target.value)}><option value="">Seleccione ID</option>{mounted.map(partition => <option key={partition.id} value={partition.id}>{partition.id} · {partition.name}</option>)}</select></label><p>Capacidad: 50 operaciones. Fechas y horas según la zona del navegador.</p>{journal.length ? <div className="table-scroll"><table><thead><tr><th>Operación</th><th>Ruta</th><th>Contenido</th><th>Fecha y hora</th></tr></thead><tbody>{journal.map((entry, index) => <tr key={index}><td>{entry.operation}</td><td>{entry.path}</td><td>{entry.content}</td><td>{new Date(entry.date * 1000).toLocaleString()}</td></tr>)}</tbody></table></div> : !error && <p>Sin registros. Seleccione una partición EXT3.</p>}</>}
        {tab === "reports" && <><p>Genere reportes con REP. Los bitmaps permiten comparar antes y después de LOSS.</p>{reports.map(report => <button key={report.url} onClick={() => void openReport(report)}>{report.name}</button>)}{!reports.length && <p>Sin reportes generados en este navegador.</p>}{reportUrl && <iframe title="Reporte generado" sandbox="" src={reportUrl} className="report-view" />}</>}
    </section>;
}
export default Explorer;
