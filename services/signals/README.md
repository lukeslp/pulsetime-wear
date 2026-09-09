# Private PulseTime readings

This optional Python service turns Drummer health checks and configured data
sources into the small JSON format consumed by the Pebble phone companion.
It binds to loopback. It does not send alerts or choose motor instructions.

`GET /v1/status` requires `Authorization: Bearer <PULSETIME_TOKEN>`. It checks
the two selected gateway services, CPU load per logical CPU, available memory,
and free disk space. Healthy services under load are busy; a partial failure
or resource pressure is degraded. Down means both selected checks failed
while the bridge itself is still reachable. It does not prove a host outage.
A missing bridge response becomes unknown on the watch.

Copy `config.example.json` to a private `config.json`. Keep a random token of
at least 32 characters in `/etc/dreamer/pulsetime-signals.env`, mode 0600,
as `PULSETIME_TOKEN=...`. The systemd unit is a deployment template. Do not
put a token in a URL, repository, or watch package. Configure the HTTPS URL
and token in PulseTime's phone settings. GET `/health` is a content-free
process check; it does not probe the configured services.

## Other hosted sources

An endpoint can already return this format directly:

```json
{
  "version": 1,
  "observed_at": 1788933600,
  "expires_at": 1788933900,
  "label": "Build",
  "state": "healthy",
  "detail": "Latest build passed"
}
```

Replace the example timestamps with the source's actual Unix seconds. States
are `healthy`, `busy`, `degraded`, `down`, and `unknown`. An optional
`values: [0, 1, 3]` supplies three ordered samples, each from 0 through 3.
The watch calls these samples 1, 2, and 3; explain their units and time range
in `detail`. Three samples use the timeline instead of the named state rhythm.

For an existing API with a different shape, configure a named adapter under
`feeds` and use `/v1/feeds/<name>`:

```json
{
  "door": {
    "url": "https://example.com/sensor",
    "label": "Door",
    "observed_path": "observed_at",
    "value_path": "door",
    "states": {"closed": "healthy", "open": "degraded"},
    "ttl": 300
  }
}
```

Alternatively replace `states` with `thresholds: [1, 2, 3]` and point
`value_path` at exactly three finite numbers. An optional `token_env` names
an environment variable containing that upstream's token. Only predefined
sources can be fetched. Redirects are rejected to avoid forwarding credentials.
Source timestamps are retained; a newly fetched old document remains unknown.

The service caches for 15 seconds, bounds response size and connections,
uses three-second upstream timeouts, and writes no request data to logs.
Run `python3 -m unittest -v` here to check classification, mappings, expiry,
authentication, and unknown routes.

## Deployment boundary

The private bridge was installed on Drummer on September 9, 2026, at
`https://api.dr.eamer.dev/pulsetime-signals/v1/status`. Unauthenticated requests
return 401. `/health` returns a content-free process check. The source is
`/home/coolhand/apps/pulsetime-signals/`; the unit uses port 5018 on loopback.
Gateway v1 and v2 are the selected checks. The deployment token is in the
protected environment file described above. No public listing was created.

The route was added directly to live Caddy, validated, adapted with the existing
HTTP-01 transformation, then reloaded. An equivalent additive route was placed
in the infrastructure source while preserving its unrelated existing changes.
Before/after checks confirmed both gateway APIs and the main site remained
available. Backups are under
`/home/coolhand/backups/pulsetime-signals-20260909T062452Z/`.

Deploy the reviewed `server.py`, `config.json`, and unit from this repository.
Add one path under an existing HTTPS host, stripping its prefix before proxying
to `127.0.0.1:5018`. Back up the live Caddy file, validate, then reload. Check
that unauthenticated requests return 401 and authenticated requests return a
fresh version-1 reading. Record the deployed source hash. Preserve existing
files and routes; rollback removes only this prefix and stops this service.
