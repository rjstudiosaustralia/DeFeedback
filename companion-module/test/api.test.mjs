import assert from 'node:assert/strict'
import http from 'node:http'
import test from 'node:test'
// The test deliberately imports the build output produced by the test script.
// eslint-disable-next-line n/no-unpublished-import
import { AuthenticationError, DeFeedbackApi } from '../dist/api.js'

function listen(server) {
	return new Promise((resolve, reject) => {
		server.once('error', reject)
		server.listen(0, '127.0.0.1', () => resolve(server.address().port))
	})
}

function close(server) {
	return new Promise((resolve, reject) => server.close((error) => (error ? reject(error) : resolve())))
}

function send(response, status, data, headers = {}) {
	response.writeHead(status, { 'Content-Type': 'application/json', Connection: 'close', ...headers })
	response.end(JSON.stringify(data))
}

test('authenticates once, retains the server cookie, and sends commands', async () => {
	let loginCount = 0
	let receivedCommand
	const server = http.createServer((request, response) => {
		const chunks = []
		request.on('data', (chunk) => chunks.push(chunk))
		request.on('end', () => {
			if (request.url === '/api/login') {
				loginCount += 1
				assert.deepEqual(JSON.parse(Buffer.concat(chunks).toString()), { code: 'custom code' })
				send(response, 200, { authenticated: true }, { 'Set-Cookie': 'DeFeedbackRemote8765=remember-me; HttpOnly' })
				return
			}

			if (request.headers.cookie !== 'DeFeedbackRemote8765=remember-me') {
				send(response, 401, { error: 'Authentication required' })
				return
			}

			if (request.url === '/api/state') {
				send(response, 200, { version: '0.7.0', lanes: [] })
				return
			}

			if (request.url === '/api/command') {
				receivedCommand = JSON.parse(Buffer.concat(chunks).toString())
				send(response, 202, { accepted: true })
				return
			}

			send(response, 404, { error: 'Not found' })
		})
	})

	const port = await listen(server)
	try {
		const api = new DeFeedbackApi({ host: '127.0.0.1', port, accessCode: 'custom code', pollInterval: 250 })
		assert.equal((await api.getState()).version, '0.7.0')
		await api.command({ type: 'setMasterMuted', muted: true })
		assert.equal(loginCount, 1)
		assert.deepEqual(receivedCommand, { type: 'setMasterMuted', muted: true })
	} finally {
		await close(server)
	}
})

test('reports a rejected access code as an authentication failure', async () => {
	const server = http.createServer((request, response) => {
		if (request.url === '/api/state') send(response, 401, { error: 'Authentication required' })
		else send(response, 401, { error: 'Incorrect access code' })
	})
	const port = await listen(server)
	try {
		const api = new DeFeedbackApi({ host: '127.0.0.1', port, accessCode: 'wrong', pollInterval: 250 })
		await assert.rejects(() => api.getState(), AuthenticationError)
	} finally {
		await close(server)
	}
})
