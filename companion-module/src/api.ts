import http, { type IncomingHttpHeaders, type RequestOptions } from 'node:http'
import type { ModuleConfig } from './config.js'
import type { RemoteCommand, RemoteState } from './types.js'

export class AuthenticationError extends Error {}

type HttpResult<T> = {
	status: number
	headers: IncomingHttpHeaders
	data: T
}

export class DeFeedbackApi {
	private cookie = ''

	constructor(private readonly config: ModuleConfig) {}

	async getState(): Promise<RemoteState> {
		let result = await this.request<RemoteState>('GET', '/api/state')
		if (result.status === 401) {
			await this.login()
			result = await this.request<RemoteState>('GET', '/api/state')
		}
		if (result.status === 401) throw new AuthenticationError('Access code was rejected')
		if (result.status !== 200) throw new Error(`State request failed (HTTP ${result.status})`)
		return result.data
	}

	async command(command: RemoteCommand): Promise<void> {
		let result = await this.request<{ accepted?: boolean }>('POST', '/api/command', command)
		if (result.status === 401) {
			await this.login()
			result = await this.request<{ accepted?: boolean }>('POST', '/api/command', command)
		}
		if (result.status === 401) throw new AuthenticationError('Access code was rejected')
		if (result.status !== 202) throw new Error(`Command failed (HTTP ${result.status})`)
	}

	private async login(): Promise<void> {
		const result = await this.request<{ authenticated?: boolean }>('POST', '/api/login', {
			code: this.config.accessCode,
		})
		if (result.status === 401) throw new AuthenticationError('Access code was rejected')
		if (result.status !== 200 || !result.data.authenticated) throw new Error(`Login failed (HTTP ${result.status})`)

		const setCookie = result.headers['set-cookie']
		const firstCookie = Array.isArray(setCookie) ? setCookie[0] : setCookie
		if (firstCookie) this.cookie = firstCookie.split(';', 1)[0] || ''
	}

	private async request<T>(method: 'GET' | 'POST', path: string, body?: object): Promise<HttpResult<T>> {
		return new Promise((resolve, reject) => {
			const json = body === undefined ? '' : JSON.stringify(body)
			const headers: Record<string, string | number> = {
				Accept: 'application/json',
				Connection: 'close',
			}
			if (json) {
				headers['Content-Type'] = 'application/json'
				headers['Content-Length'] = Buffer.byteLength(json)
			}
			if (this.cookie) headers.Cookie = this.cookie

			const options: RequestOptions = {
				hostname: this.config.host,
				port: this.config.port,
				path,
				method,
				headers,
				timeout: 2000,
			}

			const request = http.request(options, (response) => {
				const chunks: Buffer[] = []
				response.on('data', (chunk: Buffer | string) => chunks.push(Buffer.from(chunk)))
				response.on('end', () => {
					const raw = Buffer.concat(chunks).toString('utf8')
					let data: T
					try {
						data = (raw ? JSON.parse(raw) : {}) as T
					} catch {
						reject(new Error('DeFeedback Live returned invalid JSON'))
						return
					}
					resolve({ status: response.statusCode || 0, headers: response.headers, data })
				})
			})
			request.on('timeout', () => request.destroy(new Error('Connection timed out')))
			request.on('error', reject)
			if (json) request.write(json)
			request.end()
		})
	}
}
