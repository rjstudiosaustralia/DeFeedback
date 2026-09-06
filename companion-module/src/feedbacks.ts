import type ModuleInstance from './main.js'

type LaneOption = { lane: number }

export type FeedbacksSchema = {
	engine_running: { type: 'boolean'; options: Record<string, never> }
	master_muted: { type: 'boolean'; options: Record<string, never> }
	lane_enabled: { type: 'boolean'; options: LaneOption }
	lane_muted: { type: 'boolean'; options: LaneOption }
	lane_bypassed: { type: 'boolean'; options: LaneOption }
	lane_processing: { type: 'boolean'; options: LaneOption }
}

export function UpdateFeedbacks(self: ModuleInstance): void {
	const laneChoices = (self.latestState?.lanes || []).map((lane) => ({
		id: lane.id,
		label: `${lane.id}: ${lane.name}`,
	}))
	const defaultLane = laneChoices[0]?.id || 1
	const laneOption = () => [
		{ id: 'lane' as const, type: 'dropdown' as const, label: 'Lane', choices: laneChoices, default: defaultLane },
	]

	self.setFeedbackDefinitions({
		engine_running: {
			name: 'Audio engine is running',
			type: 'boolean',
			defaultStyle: { color: 0xffffff, bgcolor: 0x236b4b },
			options: [],
			callback: () => self.latestState?.engineRunning === true,
		},
		master_muted: {
			name: 'All outputs are muted',
			type: 'boolean',
			defaultStyle: { color: 0xffffff, bgcolor: 0xb33f45 },
			options: [],
			callback: () => self.latestState?.masterMuted === true,
		},
		lane_enabled: {
			name: 'Lane plugin is on',
			type: 'boolean',
			defaultStyle: { color: 0xffffff, bgcolor: 0x236b4b },
			options: laneOption(),
			callback: (feedback) => self.getLane(Number(feedback.options.lane))?.pluginEnabled === true,
		},
		lane_muted: {
			name: 'Lane De-Feedback mute is on',
			type: 'boolean',
			defaultStyle: { color: 0xffffff, bgcolor: 0xb33f45 },
			options: laneOption(),
			callback: (feedback) => self.getLane(Number(feedback.options.lane))?.pluginMuted === true,
		},
		lane_bypassed: {
			name: 'Lane is bypassed / dry',
			type: 'boolean',
			defaultStyle: { color: 0xffffff, bgcolor: 0x7b531f },
			options: laneOption(),
			callback: (feedback) => {
				const lane = self.getLane(Number(feedback.options.lane))
				return lane?.dry === true || lane?.dryFallback === true
			},
		},
		lane_processing: {
			name: 'Lane is actively processing',
			type: 'boolean',
			defaultStyle: { color: 0xffffff, bgcolor: 0x236b4b },
			options: laneOption(),
			callback: (feedback) => {
				const lane = self.getLane(Number(feedback.options.lane))
				return Boolean(
					self.latestState?.engineRunning &&
					!self.latestState.masterMuted &&
					lane?.pluginEnabled &&
					!lane.pluginMuted &&
					!lane.dry &&
					!lane.dryFallback,
				)
			},
		},
	})
}
