import type { AnalysisJob, AnalysisOptions } from './model.js';
export type AnalysisRequest = { type: 'list' } | { type: 'get' | 'cancel'; id: string }
  | { type: 'start'; revision: number; options: AnalysisOptions }
  | { type: 'replay'; id: string; trace: number; steps: number }
  | { type: 'export'; id: string; trace: number; revision: number; scenarioId: string };
export interface AnalysisReply { job?: AnalysisJob; jobs?: AnalysisJob[]; error?: string; notice?: string }
export interface AnalysisBridge { request(input: AnalysisRequest): Promise<AnalysisReply> }
