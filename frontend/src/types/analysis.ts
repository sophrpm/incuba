export interface ExecutionStats {
    successfulCommands: number;
    lexicalErrors: number;
    syntaxErrors: number;
}


export interface AnalysisResponse {
    success: boolean;
    messages: string[];
    stats: ExecutionStats;
    pendingConfirmation: boolean;
}

export interface HealthResponse {
    success: boolean;
    message: string;
}
