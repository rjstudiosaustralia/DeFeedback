import { InstanceBase, InstanceStatus, type SomeCompanionConfigField } from '@companion-module/base'
import { AuthenticationError, DeFeedbackApi } from './api.js'
import { GetConfigFields, normaliseConfig, type ModuleConfig } from './config.js'
import { UpdateActions, type ActionsSchema } from './actions.js'
import { UpdateFeedbacks, type FeedbacksSchema } from './feedbacks.js'
import { UpdatePresets } from './presets.js'
import type { RemoteCommand, RemoteLane, RemoteState } from './types.js'
import { UpgradeScripts } from './upgrades.js'
import { UpdateVariableDefinitions, UpdateVariableValues, type VariablesSchema } from './variables.js'

export type ModuleSchema = {
	config: ModuleConfig
	secrets: undefined
	actions: ActionsSchema
	feedbacks: FeedbacksSchema
	variables: VariablesSchema
}

export { UpgradeScripts }

export default class ModuleInstance extends InstanceBase<ModuleSchema> {
	config!: ModuleConfig
	latestState?: RemoteState
	private api?: DeFeedbackApi
	private pollTimer?: NodeJS.Timeout
	private destroyed = false
	private laneSignature = ''

	constructor(internal: unknown) {
		super(internal)
	}

	async init(config: ModuleConfig): Promise<void> {
		this.destroyed = false
		this.configure(config)
	}

	async destroy(): Promise<void> {
		this.destroyed = true
		if (this.pollTimer) clearTimeout(this.pollTimer)
		this.pollTimer = undefined
		this.api = undefined
	}

	async configUpdated(config: ModuleConfig): Promise<void> {
		this.configure(config)
	}

	getConfigFields(): SomeCompanionConfigField[] {
		return GetConfigFields()
	}

	getLane(id: number): RemoteLane | undefined {
		return this.latestState?.lanes.find((lane) => lane.id === id)
	}

	notifyOptimisticState(): void {
		UpdateVariableValues(this)
		this.checkAllFeedbacks()
	}

	async runCommand(command: RemoteCommand): Promise<void> {
		if (!this.api) return
		try {
			await this.api.command(command)
			this.refreshSoon()
		} catch (error) {
			this.reportError(error)
		}
	}

	private configure(config: ModuleConfig): void {
		if (this.pollTimer) clearTimeout(this.pollTimer)
		this.config = normaliseConfig(config)
		this.api = new DeFeedbackApi(this.config)
		this.latestState = undefined
		this.laneSignature = ''
		this.updateStatus(InstanceStatus.Connecting)
		this.updateDefinitions()
		void this.poll()
	}

	private async poll(): Promise<void> {
		if (this.destroyed || !this.api) return
		let nextPollDelay = this.config.pollInterval
		try {
			const state = await this.api.getState()
			this.latestState = state
			this.updateStatus(InstanceStatus.Ok)

			const signature = JSON.stringify(state.lanes.map((lane) => [lane.id, lane.name]))
			if (signature !== this.laneSignature) {
				this.laneSignature = signature
				this.updateDefinitions()
			}

			UpdateVariableValues(this)
			this.checkAllFeedbacks()
		} catch (error) {
			this.reportError(error)
			nextPollDelay = error instanceof AuthenticationError ? 5000 : Math.max(1000, this.config.pollInterval)
		} finally {
			if (!this.destroyed) this.pollTimer = setTimeout(() => void this.poll(), nextPollDelay)
		}
	}

	private refreshSoon(): void {
		if (this.pollTimer) clearTimeout(this.pollTimer)
		if (!this.destroyed) this.pollTimer = setTimeout(() => void this.poll(), 50)
	}

	private reportError(error: unknown): void {
		const message = error instanceof Error ? error.message : String(error)
		if (error instanceof AuthenticationError) {
			this.updateStatus(InstanceStatus.AuthenticationFailure, message)
		} else {
			this.updateStatus(InstanceStatus.ConnectionFailure, message)
		}
	}

	private updateDefinitions(): void {
		UpdateActions(this)
		UpdateFeedbacks(this)
		UpdatePresets(this)
		UpdateVariableDefinitions(this)
	}
}
