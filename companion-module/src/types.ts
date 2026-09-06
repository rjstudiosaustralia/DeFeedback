export type RemoteLane = {
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
