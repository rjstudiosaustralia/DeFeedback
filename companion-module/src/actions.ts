import type ModuleInstance from './main.js'

type LaneOption = { lane: number }
type ModeOption = { mode: string }

export type ActionsSchema = {
	set_strength: { options: LaneOption & { value: number } }
	step_strength: { options: LaneOption & { step: number } }
	set_bypass: { options: LaneOption & ModeOption }
	set_plugin_mute: { options: LaneOption & ModeOption }
	set_lane_enabled: { options: LaneOption & ModeOption }
	set_master_mute: { options: ModeOption }
	set_engine: { options: ModeOption }
	reset_xruns: { options: Record<string, never> }
	refresh_devices: { options: Record<string, never> }
}

const modeChoices = [
	{ id: 'toggle', label: 'Toggle' },
	{ id: 'on', label: 'On' },
	{ id: 'off', label: 'Off' },
]

function modeValue(mode: string, current: boolean): boolean {
	return mode === 'toggle' ? !current : mode === 'on'
}

export function UpdateActions(self: ModuleInstance): void {
	const lanes = self.latestState?.lanes || []
	const laneChoices = lanes.map((lane) => ({ id: lane.id, label: `${lane.id}: ${lane.name}` }))
	const defaultLane = laneChoices[0]?.id || 1

	self.setActionDefinitions({
		set_strength: {
			name: 'Lane: Set Strength / Sensitivity',
			options: [
				{ id: 'lane', type: 'dropdown', label: 'Lane', choices: laneChoices, default: defaultLane },
				{ id: 'value', type: 'number', label: 'Strength (%)', min: 0, max: 100, default: 100 },
			],
			callback: async (event) => {
				const lane = self.getLane(Number(event.options.lane))
				const next = Math.min(1, Math.max(0, Number(event.options.value) / 100))
				if (lane) {
					lane.strength = next
					self.notifyOptimisticState()
				}
				await self.runCommand({
					type: 'setLaneStrength',
					id: Number(event.options.lane),
					value: next,
					commit: true,
				})
			},
		},
		step_strength: {
			name: 'Lane: Step Strength / Sensitivity (for dial or buttons)',
			options: [
				{ id: 'lane', type: 'dropdown', label: 'Lane', choices: laneChoices, default: defaultLane },
				{
					id: 'step',
					type: 'number',
					label: 'Change (%)',
					min: -100,
					max: 100,
					default: 5,
				},
			],
			callback: async (event) => {
				const lane = self.getLane(Number(event.options.lane))
				if (!lane) return
				lane.strength = Math.min(1, Math.max(0, lane.strength + Number(event.options.step) / 100))
				self.notifyOptimisticState()
				await self.runCommand({
					type: 'setLaneStrength',
					id: lane.id,
					value: lane.strength,
					commit: true,
				})
			},
		},
		set_bypass: {
			name: 'Lane: Set or toggle bypass',
			options: [
				{ id: 'lane', type: 'dropdown', label: 'Lane', choices: laneChoices, default: defaultLane },
				{ id: 'mode', type: 'dropdown', label: 'Bypass', choices: modeChoices, default: 'toggle' },
			],
			callback: async (event) => {
				const lane = self.getLane(Number(event.options.lane))
				if (!lane) return
				lane.dry = modeValue(String(event.options.mode), lane.dry)
				self.notifyOptimisticState()
				await self.runCommand({
					type: 'updateLane',
					id: lane.id,
					name: lane.name,
					inputChannel: lane.inputChannel,
					outputChannel: lane.outputChannel,
					dry: lane.dry,
				})
			},
		},
		set_plugin_mute: {
			name: 'Lane: Set or toggle De-Feedback mute',
			options: [
				{ id: 'lane', type: 'dropdown', label: 'Lane', choices: laneChoices, default: defaultLane },
				{ id: 'mode', type: 'dropdown', label: 'Mute', choices: modeChoices, default: 'toggle' },
			],
			callback: async (event) => {
				const lane = self.getLane(Number(event.options.lane))
				if (!lane) return
				lane.pluginMuted = modeValue(String(event.options.mode), lane.pluginMuted)
				self.notifyOptimisticState()
				await self.runCommand({
					type: 'setLanePluginMuted',
					id: lane.id,
					muted: lane.pluginMuted,
				})
			},
		},
		set_lane_enabled: {
			name: 'Lane: Set or toggle plugin on/off',
			options: [
				{ id: 'lane', type: 'dropdown', label: 'Lane', choices: laneChoices, default: defaultLane },
				{ id: 'mode', type: 'dropdown', label: 'Plugin', choices: modeChoices, default: 'toggle' },
			],
			callback: async (event) => {
				const lane = self.getLane(Number(event.options.lane))
				if (!lane) return
				lane.pluginEnabled = modeValue(String(event.options.mode), lane.pluginEnabled)
				self.notifyOptimisticState()
				await self.runCommand({
					type: 'setLanePluginEnabled',
					id: lane.id,
					enabled: lane.pluginEnabled,
				})
			},
		},
		set_master_mute: {
			name: 'Master: Set or toggle all-output mute',
			options: [{ id: 'mode', type: 'dropdown', label: 'Mute all outputs', choices: modeChoices, default: 'toggle' }],
			callback: async (event) => {
				const next = modeValue(String(event.options.mode), self.latestState?.masterMuted || false)
				if (self.latestState) {
					self.latestState.masterMuted = next
					self.notifyOptimisticState()
				}
				await self.runCommand({
					type: 'setMasterMuted',
					muted: next,
				})
			},
		},
		set_engine: {
			name: 'Engine: Set or toggle running state',
			options: [{ id: 'mode', type: 'dropdown', label: 'Audio engine', choices: modeChoices, default: 'toggle' }],
			callback: async (event) => {
				const next = modeValue(String(event.options.mode), self.latestState?.engineRunning || false)
				if (self.latestState) {
					self.latestState.engineRunning = next
					self.notifyOptimisticState()
				}
				await self.runCommand({
					type: 'setEngineRunning',
					running: next,
				})
			},
		},
		reset_xruns: {
			name: 'Reset XRun counter',
			options: [],
			callback: async () => self.runCommand({ type: 'resetXRuns' }),
		},
		refresh_devices: {
			name: 'Refresh Core Audio devices',
			options: [],
			callback: async () => self.runCommand({ type: 'refreshDevices' }),
		},
	})
}
