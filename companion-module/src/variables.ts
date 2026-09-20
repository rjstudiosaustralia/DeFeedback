import type ModuleInstance from './main.js'

export type VariablesSchema = Record<string, string | number | boolean | undefined>

export function UpdateVariableDefinitions(self: ModuleInstance): void {
	const definitions: Record<string, { name: string }> = {
		plugin_mode: { name: 'Selected plugin mode' },
		version: { name: 'DeFeedback Live version' },
		engine_running: { name: 'Audio engine running' },
		master_muted: { name: 'All outputs muted' },
		latency_ms: { name: 'Estimated round-trip latency (ms)' },
		cpu_percent: { name: 'Audio CPU (%)' },
		xruns: { name: 'XRun count' },
		lane_count: { name: 'Lane count' },
		message: { name: 'Current host message' },
	}

	for (const lane of self.latestState?.lanes || []) {
		const prefix = `lane_${lane.id}`
		definitions[`${prefix}_plugin_mode`] = { name: `Lane ${lane.id} plugin` }
		definitions[`${prefix}_voiceiso_strength`] = { name: `Lane ${lane.id} VoiceISO Strength (0–120%)` }
		definitions[`${prefix}_dereverb`] = { name: `Lane ${lane.id} Dereverb (0–10)` }
		definitions[`${prefix}_cpu_mode`] = { name: `Lane ${lane.id} Clear Voice CPU mode` }
		definitions[`${prefix}_latency_mode`] = { name: `Lane ${lane.id} Clear Voice latency mode` }
		definitions[`${prefix}_plugin_latency_samples`] = { name: `Lane ${lane.id} plugin delay (samples)` }
		definitions[`${prefix}_name`] = { name: `Lane ${lane.id} name` }
		definitions[`${prefix}_strength`] = { name: `Lane ${lane.id} Strength / Sensitivity (%)` }
		definitions[`${prefix}_enabled`] = { name: `Lane ${lane.id} plugin on` }
		definitions[`${prefix}_muted`] = { name: `Lane ${lane.id} plugin muted` }
		definitions[`${prefix}_bypassed`] = { name: `Lane ${lane.id} bypassed` }
		definitions[`${prefix}_input_peak`] = { name: `Lane ${lane.id} input meter (%)` }
		definitions[`${prefix}_output_peak`] = { name: `Lane ${lane.id} output meter (%)` }
		definitions[`${prefix}_status`] = { name: `Lane ${lane.id} status` }
	}

	self.setVariableDefinitions(definitions)
}

export function UpdateVariableValues(self: ModuleInstance): void {
	const state = self.latestState
	if (!state) return

	const values: VariablesSchema = {
		plugin_mode: state.pluginMode || 'defeedback',
		version: state.version,
		engine_running: state.engineRunning,
		master_muted: state.masterMuted,
		latency_ms: Number(state.latencyMs.toFixed(1)),
		cpu_percent: Number(state.cpuPercent.toFixed(1)),
		xruns: state.xruns,
		lane_count: state.lanes.length,
		message: state.message,
	}

	for (const lane of state.lanes) {
		const prefix = `lane_${lane.id}`
		values[`${prefix}_plugin_mode`] = lane.pluginMode || state.pluginMode || 'defeedback'
		values[`${prefix}_voiceiso_strength`] =
			(lane.pluginMode || state.pluginMode) === 'voiceiso' ? Number((lane.strength * 120).toFixed(1)) : ''
		values[`${prefix}_dereverb`] = Number(((lane.dereverb || 0) * 10).toFixed(1))
		values[`${prefix}_cpu_mode`] =
			(lane.pluginMode || state.pluginMode) === 'clearvoice' ? (lane.highCpu ? 'HIGH' : 'LOW') : ''
		values[`${prefix}_latency_mode`] =
			(lane.pluginMode || state.pluginMode) === 'clearvoice' ? (lane.lowLatency ? 'LOW' : 'ZERO') : ''
		values[`${prefix}_plugin_latency_samples`] = lane.pluginLatencySamples || 0
		values[`${prefix}_name`] = lane.name
		values[`${prefix}_strength`] = Math.round(lane.strength * 100)
		values[`${prefix}_enabled`] = lane.pluginEnabled
		values[`${prefix}_muted`] = lane.pluginMuted
		values[`${prefix}_bypassed`] = !lane.pluginEnabled || lane.dry || lane.dryFallback
		values[`${prefix}_input_peak`] = Math.round(Math.sqrt(Math.max(0, lane.inputPeak)) * 100)
		values[`${prefix}_output_peak`] = Math.round(Math.sqrt(Math.max(0, lane.outputPeak)) * 100)
		values[`${prefix}_status`] = lane.status
	}

	self.setVariableValues(values)
}
