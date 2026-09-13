export interface ExecutionStats {
    successfulCommands: number;
    lexicalErrors: number;
    syntaxErrors: number;
}

export interface GeneratedReport {
    name: string;
    url: string;
}

export interface AnalysisResponse {
    success: boolean;
    messages: string[];
    stats: ExecutionStats;
    pendingConfirmation: boolean;
    reports: GeneratedReport[];
}

export interface HealthResponse {
    success: boolean;
    message: string;
}
