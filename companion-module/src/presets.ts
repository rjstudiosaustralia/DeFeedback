import type { CompanionPresetDefinitions, CompanionPresetSection } from '@companion-module/base'
import type { ModuleSchema } from './main.js'
import type ModuleInstance from './main.js'

export function UpdatePresets(self: ModuleInstance): void {
	const presets: CompanionPresetDefinitions<ModuleSchema> = {
		master_mute: {
			type: 'simple',
			name: 'Mute all outputs',
			style: { text: 'MUTE ALL\nOUTPUTS', size: 'auto', color: 0xffffff, bgcolor: 0x672d31, show_topbar: false },
			steps: [
				{
					down: [{ actionId: 'set_master_mute', options: { mode: 'toggle' } }],
					up: [],
				},
			],
			feedbacks: [
				{
					feedbackId: 'master_muted',
					options: {},
					style: { text: 'OUTPUTS\nMUTED', color: 0xffffff, bgcolor: 0xb33f45 },
				},
			],
		},
		engine: {
			type: 'simple',
			name: 'Start or stop audio engine',
			style: { text: 'START\nENGINE', size: 'auto', color: 0xffffff, bgcolor: 0x6e4d24, show_topbar: false },
			steps: [
				{
					down: [{ actionId: 'set_engine', options: { mode: 'toggle' } }],
					up: [],
				},
			],
			feedbacks: [
				{
					feedbackId: 'engine_running',
					options: {},
					style: { text: 'STOP\nENGINE', color: 0xffffff, bgcolor: 0x236b4b },
				},
			],
		},
		reset_xruns: {
			type: 'simple',
			name: 'Reset XRun counter',
			style: {
				text: 'RESET XRUNS\n$(this:xruns)',
				size: 'auto',
				color: 0xffffff,
				bgcolor: 0x29343b,
				show_topbar: false,
			},
			steps: [
				{
					down: [{ actionId: 'reset_xruns', options: {} }],
					up: [],
				},
			],
			feedbacks: [],
		},
	}

	const laneGroups = []
	for (const lane of self.latestState?.lanes || []) {
		const strengthId = `lane_${lane.id}_strength`
		const enabledId = `lane_${lane.id}_enabled`
		const mutedId = `lane_${lane.id}_muted`
		const bypassId = `lane_${lane.id}_bypass`
		laneGroups.push({
			id: `lane_${lane.id}`,
			name: `${lane.id}: ${lane.name}`,
			type: 'simple' as const,
			presets: [strengthId, enabledId, mutedId, bypassId],
		})

		presets[strengthId] = {
			type: 'simple',
			name: `${lane.name} Strength / Sensitivity dial`,
			style: {
				text: `${lane.name}\n$(this:lane_${lane.id}_strength)%`,
				size: 'auto',
				color: 0xffffff,
				bgcolor: 0x25536a,
				show_topbar: false,
			},
			steps: [
				{
					down: [],
					up: [],
					rotate_left: [{ actionId: 'step_strength', options: { lane: lane.id, step: -1 } }],
					rotate_right: [{ actionId: 'step_strength', options: { lane: lane.id, step: 1 } }],
				},
			],
			feedbacks: [],
		}
		presets[enabledId] = {
			type: 'simple',
			name: `${lane.name} plugin on/off`,
			style: { text: `${lane.name}\nPLUGIN OFF`, size: 'auto', color: 0xffffff, bgcolor: 0x29343b, show_topbar: false },
			steps: [
				{
					down: [{ actionId: 'set_lane_enabled', options: { lane: lane.id, mode: 'toggle' } }],
					up: [],
				},
			],
			feedbacks: [
				{
					feedbackId: 'lane_enabled',
					options: { lane: lane.id },
					style: { text: `${lane.name}\nPLUGIN ON`, color: 0xffffff, bgcolor: 0x236b4b },
				},
			],
		}
		presets[mutedId] = {
			type: 'simple',
			name: `${lane.name} De-Feedback mute`,
			style: { text: `${lane.name}\nMUTE`, size: 'auto', color: 0xffffff, bgcolor: 0x29343b, show_topbar: false },
			steps: [
				{
					down: [{ actionId: 'set_plugin_mute', options: { lane: lane.id, mode: 'toggle' } }],
					up: [],
				},
			],
			feedbacks: [
				{
					feedbackId: 'lane_muted',
					options: { lane: lane.id },
					style: { color: 0xffffff, bgcolor: 0xb33f45 },
				},
			],
		}
		presets[bypassId] = {
			type: 'simple',
			name: `${lane.name} bypass`,
			style: { text: `${lane.name}\nBYPASS`, size: 'auto', color: 0xffffff, bgcolor: 0x29343b, show_topbar: false },
			steps: [
				{
					down: [{ actionId: 'set_bypass', options: { lane: lane.id, mode: 'toggle' } }],
					up: [],
				},
			],
			feedbacks: [
				{
					feedbackId: 'lane_bypassed',
					options: { lane: lane.id },
					style: { color: 0xffffff, bgcolor: 0x7b531f },
				},
			],
		}
	}

	const structure: CompanionPresetSection<ModuleSchema>[] = [
		{
			id: 'master',
			name: 'Master',
			definitions: ['master_mute', 'engine', 'reset_xruns'],
		},
	]
	if (laneGroups.length > 0) structure.push({ id: 'lanes', name: 'Lanes', definitions: laneGroups })

	self.setPresetDefinitions(structure, presets)
}
