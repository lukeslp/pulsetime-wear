# Listing update, November 3, 2026

Approved by Luke (ambient-time `docs/reviews/pulsetime-listing-20261103.md`) and applied with
ambient-time `scripts/update-pebble-listing.py`. No new release: the store download stays 1.2.0.

Before: title "PulseTime Public Beta", category Daily, no website, one screenshot per
platform (three on emery), no banners. The previous description is `description-before.txt`.

After (`dashboard-after.json`): title "PulseTime: Feel the Time", website
https://ambient-time.com/watch/pebble/, the description in `../description.txt`, and per
platform five screenshots and a banner from `../listing-media/`. The screenshots were captured
in the emulator from the published 1.2.0 package (SHA-256 11450dc5...f711) at 3:32: the clock,
HOURS 3, QUARTERS 2, the Learn by touch page, and Practice. The banners follow the house
template (name and ambient-time.com beside the framed HOURS 3 capture, ground #8A5A2B).

`dashboard-before.json` was written by the final media run, so it shows the new text with
the old media, not the original listing.

Category: still Daily. The dashboard update endpoint returns success but ignores every
category field tried (category with key, slug or id; categoryId; category_id; categoryKey;
categorySlug; category_name; categoryName), and the dashboard's Edit Listing page has
no category field. Changing it would need the store's operators (appstore@repebble.com).
