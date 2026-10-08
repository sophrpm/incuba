import type { AnalysisResponse, HealthResponse } from "../types/analysis";

export const BACKEND_URL = import.meta.env.VITE_API_URL || (import.meta.env.DEV ? "http://localhost:2611" : "");
let token = sessionStorage.getItem("mia-token") || "";
let initialization: Promise<SessionInfo> | null = null;

export interface SessionInfo { success: boolean; token: string; active: boolean; user: string; partitionId: string; uid: number; gid: number }
export interface PartitionInfo { name: string; size: number; fit: string; type: string; id: string; mounted: boolean; filesystem: number }
export interface DiskInfo { id: string; name: string; size: number; fit: string; date: number; partitions: PartitionInfo[] }
export interface FileInfo { name: string; path: string; type: string; size: number; uid: number; gid: number; permissions: string }
export interface JournalInfo { operation: string; path: string; content: string; date: number }
export interface ReportInfo { name: string; url: string }

//crea el estado del navegador antes de enviar comandos
export async function getSession(): Promise<SessionInfo> {
    const response = await fetch(`${BACKEND_URL}/api/session`, { headers: { "X-MIA-Session": token } });
    if (!response.ok) throw new Error("No se pudo obtener la sesión.");
    const data: SessionInfo = await response.json();
    token = data.token;
    sessionStorage.setItem("mia-token", token);
    return data;
}

export async function apiRequest<T>(path: string, body?: object): Promise<T> {
    if (!token) {
        initialization ||= getSession();
        await initialization;
        initialization = null;
    }
    const response = await fetch(`${BACKEND_URL}${path}`, {
        method: body ? "POST" : "GET",
        headers: { "Content-Type": "application/json", "X-MIA-Session": token },
        body: body ? JSON.stringify(body) : undefined,
    });
    const data = await response.json();
    if (!response.ok) throw new Error(data.message || data.messages?.join("\n") || "Error en la solicitud.");
    return data as T;
}

export async function checkServerStatus(): Promise<boolean> {
    try {
        const response = await fetch(`${BACKEND_URL}/api/health`);
        const data: HealthResponse = await response.json();
        return response.ok && data.success;
    } catch { return false; }
}

//los errores de comando conservan los mensajes y los contadores
export async function analyzeCommands(input: string): Promise<AnalysisResponse> {
    if (!token) await getSession();
    const response = await fetch(`${BACKEND_URL}/api/analyze`, {
        method: "POST", headers: { "Content-Type": "application/json", "X-MIA-Session": token }, body: JSON.stringify({ input }),
    });
    const data = await response.json();
    if (!data.stats) throw new Error(data.message || "No se pudieron ejecutar los comandos.");
    return data;
}

export async function loadReport(path: string): Promise<string> {
    const response = await fetch(`${BACKEND_URL}${path}`, { headers: { "X-MIA-Session": token } });
    if (!response.ok) throw new Error("No se pudo abrir el reporte.");
    return URL.createObjectURL(await response.blob());
}
