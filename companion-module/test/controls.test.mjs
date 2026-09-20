import assert from 'node:assert/strict'
import test from 'node:test'
// eslint-disable-next-line n/no-unpublished-import
import { UpdateActions } from '../dist/actions.js'
// eslint-disable-next-line n/no-unpublished-import
import { UpdateFeedbacks } from '../dist/feedbacks.js'
// eslint-disable-next-line n/no-unpublished-import
import { UpdateVariableValues } from '../dist/variables.js'

function host(mode) {
	const lane = {
		id: 1,
		name: 'Lead',
		strength: 100 / 120,
		strengthAvailable: true,
		pluginEnabled: true,
		inputPeak: 0.2,
		outputPeak: 0.2,
	}
	return {
		latestState: { pluginMode: mode, lanes: [lane], latencyMs: 2.67, cpuPercent: 10 },
		getLane: () => lane,
		setActionDefinitions(actions) {
			this.actions = actions
		},
		setFeedbackDefinitions(feedbacks) {
			this.feedbacks = feedbacks
		},
		setVariableValues(values) {
			this.values = values
		},
		notifyOptimisticState() {},
		async runCommand(command) {
			this.command = command
		},
	}
}

test('VoiceISO visible Strength and legacy normalized Strength use distinct scales', async () => {
	const self = host('voiceiso')
	UpdateActions(self)
	await self.actions.set_voiceiso_strength.callback({ options: { lane: 1, value: 100 } })
	assert.equal(self.command.value, 100 / 120)
	await self.actions.set_voiceiso_strength.callback({ options: { lane: 1, value: 120 } })
	assert.equal(self.command.value, 1)
	await self.actions.set_strength.callback({ options: { lane: 1, value: 30 } })
	assert.equal(self.command.value, 0.3)
	UpdateVariableValues(self)
	assert.equal(self.values.lane_1_strength, 30)
	assert.equal(self.values.lane_1_voiceiso_strength, 36)
	self.latestState.pluginMode = 'defeedback'
	self.command = undefined
	await self.actions.set_voiceiso_strength.callback({ options: { lane: 1, value: 120 } })
	assert.equal(self.command, undefined)
})

test('OFF is reflected as dry in every plugin mode', () => {
	for (const mode of ['defeedback', 'clearvoice', 'voiceiso']) {
		const self = host(mode)
		self.latestState.lanes[0].pluginEnabled = false
		UpdateFeedbacks(self)
		UpdateVariableValues(self)
		assert.equal(self.feedbacks.lane_bypassed.callback({ options: { lane: 1 } }), true)
		assert.equal(self.feedbacks.lane_processing.callback({ options: { lane: 1 } }), false)
		assert.equal(self.values.lane_1_bypassed, true)
	}
})

test('Mixed racks use each lane plugin for controls and variables', async () => {
	const self = host('mixed')
	self.latestState.lanes[0].pluginMode = 'voiceiso'
	UpdateActions(self)
	await self.actions.set_voiceiso_strength.callback({ options: { lane: 1, value: 120 } })
	assert.equal(self.command.value, 1)
	UpdateVariableValues(self)
	assert.equal(self.values.lane_1_voiceiso_strength, 100)
	assert.equal(self.values.lane_1_plugin_mode, 'voiceiso')
	self.latestState.lanes[0].pluginMode = 'clearvoice'
	self.latestState.lanes[0].highCpu = true
	self.command = undefined
	await self.actions.set_voiceiso_strength.callback({ options: { lane: 1, value: 120 } })
	assert.equal(self.command, undefined)
	UpdateVariableValues(self)
	assert.equal(self.values.lane_1_voiceiso_strength, '')
	assert.equal(self.values.lane_1_cpu_mode, 'HIGH')
	await self.actions.set_lane_plugin.callback({ options: { lane: 1, value: 'defeedback' } })
	assert.deepEqual(self.command, { type: 'setLanePluginMode', id: 1, value: 'defeedback' })
})
