# Meaningful custom readings

In phone settings, choose Advanced, enter your Custom HTTPS feed, and select
its meaning. These presets interpret your endpoint; they do not include a
transit subscription, discover a server, or fetch a different provider.
The endpoint must return current source timestamps in Unix seconds.

## Next three departures

Choose **Next three departures (minutes)**. Return the minutes remaining until
the next three departures in ascending order, along with the usual source
state and freshness fields:

```json
{
  "version": 1,
  "observed_at": 1788933600,
  "expires_at": 1788933660,
  "state": "healthy",
  "label": "Bus 12",
  "measurements": [2, 10, 25]
}
```

Replace those example timestamps with real source times. The watch shows
Next 2 min, 2nd 10 min, and 3rd 25 min. More taps means sooner: within five
minutes is dense, within fifteen is medium, within thirty is sparse, and
beyond thirty is a quiet field. Opening and closing cues remain present.
Negative, unordered, missing, or stale measurements produce Unknown. Zero
means due now; it is not silence. Values must be between 0 and 9999 minutes.

## Resource usage

Choose **CPU / memory / disk used (%)**. Use the same envelope and provide
`measurements: [0, 70, 95]` for CPU usage, used memory, and used disk, in that
order. Each value must be between 0 and 100. Zero is quiet; positive usage
below 50 is sparse, below 80 is medium, and 80 or above is dense. These are
glance categories, not a universal definition of service health. The source
must supply measurements in these units and preserve its observation time.

## Your own labels and units

Choose **Source-defined levels and labels**. Keep `values` as three integers
from 0 through 3, and optionally provide both arrays below:

```json
{
  "values": [1, 2, 3],
  "sample_labels": ["North", "East", "West"],
  "sample_text": ["10 kph", "20 kph", "30 kph"],
  "detail": "Wind speed. More taps means stronger wind."
}
```

These fields go inside the version-1 envelope above. Names allow eight ASCII
characters, value text twelve; provide all three names and all three values.
The source is responsible for consistent units, descriptions, and level
mapping. The watch displays the supplied text and vibrates the corresponding
level. Hold Down to read the explanation. These are ordered samples, not
automatically two-hour forecast windows. A categorical feed needs no samples.

Credentials stay on the phone. No endpoint can send executable code or choose
arbitrary motor commands. Presets preserve source freshness; retrieving an
old response does not make it current.
