import type {
    AnalysisResponse,
    HealthResponse,
} from "../types/analysis";

export const BACKEND_URL = import.meta.env.VITE_API_URL || "http://localhost:2611";


//backnd disponible?
export async function checkServerStatus(): Promise<boolean> {
    try {
        const response = await fetch(`${BACKEND_URL}/api/health`);

        if (!response.ok) {
            return false;
        }

        const data: HealthResponse = await response.json();

        return data.success;
    } catch {
        return false;
    }
}


//evia comandos al back
export async function analyzeCommands(commandText: string): Promise<AnalysisResponse> {
    const response = await fetch(`${BACKEND_URL}/api/analyze`, {
        method: "POST",

        headers: {
            "Content-Type": "application/json",
        },

        body: JSON.stringify({
            input: commandText,
        }),
    });

    //error
    if (!response.ok) {
        throw new Error("El backend no pudo analizar los comandos.");
    }

    const result: AnalysisResponse = await response.json();

    return result;
}