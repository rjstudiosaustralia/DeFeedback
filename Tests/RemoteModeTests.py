"""Opt-in HTTP tests for the simulated QA app on port 8766, never the live host."""
import http.cookiejar
import json
import time
import urllib.request

BASE = 'http://127.0.0.1:8766'
client = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))


def request(path, body=None):
    data = None if body is None else json.dumps(body).encode()
    return json.load(client.open(urllib.request.Request(
        BASE + path, data=data, headers={'Content-Type': 'application/json'}), timeout=3))


def state():
    return request('/api/state')


def command(kind, **values):
    request('/api/command', {'type': kind, **values})
    # Commands are queued on the GUI thread; the state publisher runs at 10 Hz.
    time.sleep(0.25)
    return state()


request('/api/login', {'code': '12345678'})
assert 'Isolated UI preview' in state()['pluginDiagnostic'], 'Refusing to test a non-QA host'
for parallel in (False, True):
    command('setMasterMuted', muted=False)
    command('setEngineRunning', running=True)
    changed = command('setParallelProcessing', enabled=parallel)
    assert changed['parallelProcessing'] == parallel
    assert not changed['engineRunning'] and changed['masterMuted']
    assert changed['processingThreads'] >= 1
    command('setMasterMuted', muted=False)
    command('setEngineRunning', running=True)
    unchanged = command('setParallelProcessing', enabled=parallel)
    assert unchanged['engineRunning'] and not unchanged['masterMuted']
for limit in (4, 6, 8, 0):
    changed = command('setParallelProcessing', enabled=True, maxThreads=limit)
    assert changed['maxProcessingThreads'] == limit
    assert not changed['engineRunning'] and changed['masterMuted']
    assert 'callbackPeakMs' in changed and 'deviceXruns' in changed
rejected = command('setParallelProcessing', enabled=True, maxThreads=-1)
assert rejected['maxProcessingThreads'] == 0 and rejected['messageError']
rejected = command('setParallelProcessing', enabled=True, maxThreads=65)
assert rejected['maxProcessingThreads'] == 0 and rejected['messageError']
rejected = command('setParallelProcessing', enabled=True, maxThreads=4294967296)
assert rejected['maxProcessingThreads'] == 0 and rejected['messageError']
for mode in ('voiceiso', 'clearvoice', 'defeedback'):
    start = command('setPluginMode', value=mode)
    for lane in start['lanes']:
        command('setLanePluginEnabled', id=lane['id'], pluginMode=mode, enabled=True)
original = [(l['id'], l['name'], l['inputChannel'], l['outputChannel']) for l in start['lanes']]
first, second, third = [l['id'] for l in start['lanes'][:3]]
command('setMasterMuted', muted=False)
command('setEngineRunning', running=True)
changed = command('setLanePluginMode', id=second, pluginMode='defeedback', value='clearvoice')
assert changed['pluginMode'] == 'mixed' and not changed['engineRunning'] and changed['masterMuted']
assert [l['pluginMode'] for l in changed['lanes'][:3]] == ['defeedback', 'clearvoice', 'defeedback']
changed = command('setLanePluginMode', id=third, pluginMode='defeedback', value='voiceiso')
assert [l['pluginMode'] for l in changed['lanes'][:3]] == ['defeedback', 'clearvoice', 'voiceiso']
assert changed['lanes'][0]['pluginMuteAvailable'] and not changed['lanes'][0]['dereverbAvailable']
assert changed['lanes'][1]['dereverbAvailable'] and not changed['lanes'][1]['pluginMuteAvailable']
assert not changed['lanes'][2]['dereverbAvailable'] and not changed['lanes'][2]['pluginMuteAvailable']
assert [(l['id'], l['name'], l['inputChannel'], l['outputChannel']) for l in changed['lanes']] == original
rejected = command('setLanePluginEnabled', id=second, enabled=False, pluginMode='defeedback')
assert rejected['lanes'][1]['pluginEnabled'] and 'Stale remote command rejected' in rejected['message']
rejected = command('setLanePluginMode', id=first, pluginMode='defeedback', value='unknown')
assert rejected['lanes'][0]['pluginMode'] == 'defeedback'
rejected = command('updateLane', id=first, inputChannel=changed['lanes'][1]['inputChannel'], pluginMode='defeedback')
assert rejected['lanes'][0]['inputChannel'] == original[0][2] and rejected['messageError']
# Unchanged selections leave playback and the master gate alone.
command('setEngineRunning', running=True)
command('setMasterMuted', muted=False)
unchanged = command('setLanePluginMode', id=second, pluginMode='clearvoice', value='clearvoice')
assert unchanged['engineRunning'] and not unchanged['masterMuted']
# OFF remains a dry feed in the mixed rack; master mute wins on all lanes.
for lane in changed['lanes'][:3]:
    off = command('setLanePluginEnabled', id=lane['id'], enabled=False, pluginMode=lane['pluginMode'])
    actual = next(l for l in off['lanes'] if l['id'] == lane['id'])
    assert actual['dryFallback'] and actual['status'] == 'PLUGIN OFF - DRY' and actual['outputPeak'] > 0
muted = command('setMasterMuted', muted=True, pluginMode='outdated')
assert all(l['status'] == 'OUTPUT MUTED' and l['outputPeak'] == 0 for l in muted['lanes'])
for lane in changed['lanes']:
    command('setLanePluginEnabled', id=lane['id'], enabled=True, pluginMode=lane['pluginMode'])
# Changing one patch preserves every plugin choice.
repatched = command('updateLane', id=first, pluginMode='defeedback', inputChannel=15, outputChannel=15)
assert not repatched['engineRunning'] and repatched['masterMuted']
assert [l['pluginMode'] for l in repatched['lanes']] == [l['pluginMode'] for l in changed['lanes']]
for mode in ('voiceiso', 'clearvoice', 'defeedback'):
    selected = command('setPluginMode', value=mode)
    assert selected['pluginMode'] == mode and all(l['pluginMode'] == mode for l in selected['lanes'])
    assert selected['lanes'][0]['inputChannel'] == 15 and selected['lanes'][0]['outputChannel'] == 15
    assert not selected['engineRunning'] and selected['masterMuted']
command('updateLane', id=first, pluginMode='defeedback', inputChannel=original[0][2], outputChannel=original[0][3])
assert state()['sleepPrevented'], 'QA host must hold system and display sleep assertions'
print('All mixed-plugin remote, routing, stale-command, OFF and master-mute tests passed.')
