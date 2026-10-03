// Just enough for stage2 build to pass

export interface PluginContext extends Record<string, any> {
	emitFile: (a1: any) => any;
}

export interface FunctionPluginHooks extends Record<string, any> {
	generateBundle: (this: PluginContext, a1: any, a2: any, a3: any) => void;
	load: (this: PluginContext, a1: any) => any;
	renderDynamicImport: (this: PluginContext, a1: any) => void;
	resolveId: (this: PluginContext, a1: any, a2: any) => any;
	writeBundle: (this: PluginContext, a1: any) => void;
}

export type Plugin = Partial<FunctionPluginHooks>;

export type LogHandler = (a1: any, a2: any) => any;
export type PluginImpl = () => any;
export type RollupOptions = any;
export type WarningHandlerWithDefault = (a1: any) => any;
