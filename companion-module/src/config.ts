import type { SomeCompanionConfigField } from '@companion-module/base'

export type ModuleConfig = {
	host: string
	port: number
	accessCode: string
	pollInterval: number
}

export function GetConfigFields(): SomeCompanionConfigField[] {
	return [
		{
			type: 'textinput',
			id: 'host',
			label: 'DeFeedback Live Mac address',
			width: 8,
			default: '127.0.0.1',
			tooltip: 'IP address or hostname only; do not include http:// or the port.',
		},
		{
			type: 'number',
			id: 'port',
			label: 'LAN remote port',
			width: 4,
			min: 1024,
			max: 65535,
			default: 8765,
		},
		{
			type: 'textinput',
			id: 'accessCode',
			label: 'Access code',
			width: 8,
			default: '',
			tooltip: 'Enter the custom code shown in the Mac app. Leave blank only when access-code protection is off.',
		},
		{
			type: 'number',
			id: 'pollInterval',
			label: 'State refresh (ms)',
			width: 4,
			min: 100,
			max: 2000,
			default: 250,
		},
	]
}

export function normaliseConfig(config: ModuleConfig): ModuleConfig {
	return {
		host: String(config.host || '127.0.0.1')
			.trim()
			.replace(/^https?:\/\//i, '')
			.split('/')[0]
			.split(':')[0],
		port: Math.min(65535, Math.max(1024, Number(config.port) || 8765)),
		accessCode: String(config.accessCode || ''),
		pollInterval: Math.min(2000, Math.max(100, Number(config.pollInterval) || 250)),
	}
}
