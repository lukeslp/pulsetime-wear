#!/usr/bin/env python3
"""Luke Steuber's private, read-only PulseTime data bridge. Python stdlib only."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hmac
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import os
from pathlib import Path
import shutil
import threading
import time
from urllib.parse import urlsplit
from urllib.request import Request, build_opener, HTTPRedirectHandler

STATES = {"unknown", "healthy", "busy", "degraded", "down"}


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None  # Do not forward a configured upstream credential elsewhere.


def reading(label, state, detail, *, now=None, ttl=60, values=None):
    now = int(time.time() if now is None else now)
    result = dict(version=1, observed_at=now, expires_at=now + ttl,
                  label=label[:24], state=state, detail=detail[:96])
    if values is not None:
        result["values"] = values
    return result


def get_json(url, token="", timeout=3):
    request = Request(url, headers={"Accept": "application/json"})
    if token:
        request.add_header("Authorization", "Bearer " + token)
    with build_opener(NoRedirect).open(request, timeout=timeout) as response:
        content = response.read(65537)
    if len(content) > 65536:
        raise ValueError("response too large")
    return json.loads(content)


def probe(url):
    started = time.monotonic()
    try:
        data = get_json(url)
        ok = isinstance(data, dict) and (data.get("status") in ("ok", "healthy", "operational") or data.get("ok") is True)
        return ok, time.monotonic() - started
    except (OSError, ValueError):
        return False, time.monotonic() - started


def classify_host(checks, load_ratio, available_memory, free_disk):
    """Down describes selected services; receipt itself proves host reachability."""
    if not checks:
        return "unknown", "No service checks configured."
    passed = sum(ok for ok, _ in checks)
    if not passed:
        return "down", "Service checks failed; host agent is reachable."
    if passed != len(checks) or available_memory < .10 or free_disk < .10:
        return "degraded", "Some checks failed or memory/disk pressure is high."
    if load_ratio >= .85 or max(elapsed for _, elapsed in checks) >= 1.5:
        return "busy", "Serving requests under load."
    return "healthy", "Service checks passed; resources have headroom."


def host_reading(config):
    urls = config.get("checks", [])
    with ThreadPoolExecutor(max_workers=max(1, min(4, len(urls)))) as pool:
        checks = list(pool.map(probe, urls))
    mem = {}
    for line in Path("/proc/meminfo").read_text().splitlines():
        key, value = line.split(":", 1)
        mem[key] = int(value.split()[0])
    disk = shutil.disk_usage(config.get("disk", "/"))
    state, detail = classify_host(checks, os.getloadavg()[0] / (os.cpu_count() or 1),
                                  mem["MemAvailable"] / mem["MemTotal"], disk.free / disk.total)
    return reading("Drummer services", state, detail)


def path_value(data, path):
    for key in path.split("."):
        if isinstance(data, list) and key.isdigit():
            data = data[int(key)]
        elif isinstance(data, dict):
            data = data[key]
        else:
            raise ValueError("missing data path")
    return data


def adapt_feed(raw, cfg, now=None):
    """Map a source field to named states or three numeric bands.

    Require upstream observed time so fetching a cached stale document does not
    make it fresh. Configure the timestamp field in Unix seconds.
    """
    now = int(time.time() if now is None else now)
    observed = path_value(raw, cfg["observed_path"])
    ttl = cfg.get("ttl", 300)
    if isinstance(observed, bool) or not isinstance(observed, int) or not 1 <= observed <= now or observed + ttl <= now:
        raise ValueError("old or future upstream timestamp")
    if "states" in cfg:
        value = path_value(raw, cfg["value_path"])
        state = cfg["states"].get(str(value), "unknown")
        if state not in STATES:
            raise ValueError("unsupported mapped state")
        result = reading(cfg["label"], state, cfg.get("detail", ""), now=observed, ttl=ttl)
    else:
        values = path_value(raw, cfg["value_path"])
        thresholds = cfg["thresholds"]
        if not isinstance(values, list) or len(values) != 3 or len(thresholds) != 3 or thresholds != sorted(thresholds):
            raise ValueError("three samples and three ascending thresholds required")
        if any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in values + thresholds):
            raise ValueError("invalid numeric samples")
        levels = [sum(v >= threshold for threshold in thresholds) for v in values]
        result = reading(cfg["label"], "healthy", cfg.get("detail", ""), now=observed, ttl=ttl, values=levels)
    return result


class Bridge:
    def __init__(self, config):
        self.config = config
        self.cache = {}
        self.lock = threading.Lock()

    def fetch(self, path):
        with self.lock:
            cached = self.cache.get(path)
            if cached and time.time() - cached[0] < 15:
                return cached[1]
            if path == "/v1/status":
                try:
                    result = host_reading(self.config)
                except (OSError, ValueError, KeyError, ZeroDivisionError):
                    result = reading("Drummer services", "unknown", "Host metrics unavailable.")
            elif path.startswith("/v1/feeds/"):
                name = path.removeprefix("/v1/feeds/")
                cfg = self.config.get("feeds", {}).get(name)
                if cfg is None:
                    return None
                try:
                    token = os.environ.get(cfg.get("token_env", ""), "")
                    result = adapt_feed(get_json(cfg["url"], token), cfg)
                except (OSError, ValueError, KeyError, TypeError, IndexError):
                    result = reading(cfg["label"], "unknown", "Upstream data unavailable or old.")
            else:
                return None
            self.cache[path] = (time.time(), result)
            return result


def validate_config(config):
    checks = config.get("checks", [])
    feeds = config.get("feeds", {})
    if not isinstance(checks, list) or len(checks) > 4 or not isinstance(feeds, dict) or len(feeds) > 16:
        raise ValueError("too many or invalid sources")
    for url in checks:
        parsed = urlsplit(url)
        if parsed.scheme not in ("http", "https") or not parsed.hostname or parsed.username or parsed.password:
            raise ValueError("invalid probe URL")
    for name, cfg in feeds.items():
        if not name or len(name) > 40 or not all(c.isalnum() or c == "-" for c in name):
            raise ValueError("invalid feed name")
        parsed = urlsplit(cfg["url"])
        if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password:
            raise ValueError("feed URLs must use HTTPS without embedded credentials")
        if not isinstance(cfg.get("label"), str) or not cfg["label"] or not 15 <= cfg.get("ttl", 300) <= 3600:
            raise ValueError("invalid label or TTL")
    return config


def handler(bridge, token):
    class Handler(BaseHTTPRequestHandler):
        timeout = 5

        def do_GET(self):
            if self.path == "/health":
                self.reply(200, {"ok": True})
                return
            if not hmac.compare_digest(self.headers.get("Authorization", "").encode(), ("Bearer " + token).encode()):
                self.reply(401, {"error": "unauthorized"})
                return
            result = bridge.fetch(self.path)
            self.reply(404, {"error": "not found"}) if result is None else self.reply(200, result)

        def reply(self, status, data):
            body = json.dumps(data, separators=(",", ":")).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Robots-Tag", "noindex, nofollow")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *_):
            pass  # never log headers, feed paths, URLs or source contents
    return Handler


class BoundedServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, *args, **kwargs):
        self.slots = threading.BoundedSemaphore(8)
        super().__init__(*args, **kwargs)

    def process_request(self, request, client_address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.slots.release()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True)
    parser.add_argument("--port", type=int, default=5018)
    args = parser.parse_args()
    token = os.environ.get("PULSETIME_TOKEN", "")
    if len(token) < 32:
        parser.error("PULSETIME_TOKEN must contain at least 32 characters")
    config = validate_config(json.loads(Path(args.config).read_text()))
    BoundedServer(("127.0.0.1", args.port), handler(Bridge(config), token)).serve_forever()


if __name__ == "__main__":
    main()
