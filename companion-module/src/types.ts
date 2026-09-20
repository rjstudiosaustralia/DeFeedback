export type PluginMode = 'defeedback' | 'clearvoice' | 'voiceiso'

export type RemoteLane = {
	pluginMode?: PluginMode
	pluginName?: string
	dereverb?: number
	dereverbAvailable?: boolean
	highCpu?: boolean
	lowLatency?: boolean
	cpuModeAvailable?: boolean
	latencyModeAvailable?: boolean
	pluginLatencySamples?: number
	id: number
	name: string
	inputChannel: number
	outputChannel: number
	dry: boolean
	pluginEnabled: boolean
	dryFallback: boolean
	pluginMuted: boolean
	pluginMuteAvailable: boolean
	strength: number
	strengthAvailable: boolean
	inputPeak: number
	outputPeak: number
	status: string
}

export type RemoteState = {
	pluginMode?: PluginMode | 'mixed'
	pluginName?: string
	version: string
	engineRunning: boolean
	masterMuted: boolean
	autoStart: boolean
	launchAtLogin: boolean
	sleepPrevented: boolean
	accessCodeRequired: boolean
	latencyMs: number
	cpuPercent: number
	xruns: number
	pluginDiagnostic: string
	message: string
	messageError: boolean
	capacity: number
	lanes: RemoteLane[]
}

export type RemoteCommand = Record<string, boolean | number | string>
