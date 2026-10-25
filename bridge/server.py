"""Optional LAN data bridge. Run: python server.py --config config.json."""
import argparse
import datetime as dt
import hmac
import io
import json
import logging
import threading
import time
from pathlib import Path
from zoneinfo import ZoneInfo

import feedparser
import recurring_ical_events
import requests
from flask import Flask, jsonify, request
from icalendar import Calendar

LOG = logging.getLogger("wisp")

def create_app(config):
    token = config.get("token", "")
    if len(token) < 24 or token.startswith("REPLACE"):
        raise ValueError("Set a unique random token of at least 24 characters in config.json")
    app = Flask(__name__)
    app.config["MAX_CONTENT_LENGTH"] = 1024 * 1024
    cache = {"at": 0, "data": {}, "refreshing": False}
    lock = threading.Lock()
    model_lock = threading.Lock()
    model = None

    @app.before_request
    def authenticate():
        expected = "Bearer " + token
        if not hmac.compare_digest(request.headers.get("Authorization", ""), expected):
            return jsonify(error="Unauthorized"), 401

    def fetch(url, **kwargs):
        response = requests.get(url, timeout=(5, 12), **kwargs)
        response.raise_for_status()
        return response

    def ha_headers():
        return {"Authorization": "Bearer " + config.get("home_assistant_token", "")}

    def build_feed():
        now = dt.datetime.now(ZoneInfo(config.get("timezone", "America/Edmonton")))
        result = {"updated": now.isoformat(), "weather": "Weather not configured", "calendar": "No calendar connected", "briefing": "", "home": [], "errors": []}
        if config.get("latitude") is not None and config.get("longitude") is not None:
            try:
                current = fetch("https://api.open-meteo.com/v1/forecast", params={"latitude": config["latitude"], "longitude": config["longitude"], "current": "temperature_2m,apparent_temperature,weather_code", "timezone": config.get("timezone", "America/Edmonton")}).json()["current"]
                code = current["weather_code"]
                sky = "Clear" if code == 0 else "Cloudy" if code < 4 else "Fog" if code < 50 else "Rain" if code < 70 else "Snow" if code < 80 else "Showers" if code < 95 else "Thunderstorms"
                result["weather"] = f'{sky} | {current["temperature_2m"]} C | feels {current["apparent_temperature"]} C'
            except Exception as exc:
                LOG.warning("Weather fetch failed: %s", type(exc).__name__)
                result["weather"] = "Weather temporarily unavailable"
                result["errors"].append("weather")
        if config.get("calendar_ics_url"):
            try:
                raw = fetch(config["calendar_ics_url"]).content
                if len(raw) > 5000000:
                    raise ValueError("Calendar exceeds 5 MB")
                events = recurring_ical_events.of(Calendar.from_ical(raw)).between(now, now + dt.timedelta(days=7))
                candidates = []
                for event in events:
                    start = event.decoded("dtstart")
                    end = event.get("dtend")
                    end = end.dt if end else start
                    if isinstance(start, dt.datetime):
                        if start.tzinfo is None: start = start.replace(tzinfo=now.tzinfo)
                        start = start.astimezone(now.tzinfo)
                        if isinstance(end, dt.datetime):
                            if end.tzinfo is None: end = end.replace(tzinfo=now.tzinfo)
                            if end < now: continue
                        label = start.strftime("%a %H:%M")
                    else:
                        if start < now.date(): continue
                        label = start.strftime("%a") + " all day"
                        start = dt.datetime.combine(start, dt.time(), now.tzinfo)
                    if str(event.get("status", "")).upper() == "CANCELLED": continue
                    candidates.append((start, label + " - " + str(event.get("summary", "Event"))))
                result["calendar"] = sorted(candidates, key=lambda x: x[0])[0][1] if candidates else "No events in the next 7 days"
            except Exception as exc:
                LOG.warning("Calendar fetch failed: %s", type(exc).__name__)
                result["calendar"] = "Calendar temporarily unavailable"
                result["errors"].append("calendar")
        headlines = []
        for url in config.get("rss_urls", [])[:3]:
            try:
                parsed = feedparser.parse(fetch(url).content)
                for entry in parsed.entries[:3]:
                    title = entry.get("title", "").strip()
                    if title: headlines.append(title[:160])
            except Exception as exc:
                LOG.warning("RSS fetch failed: %s", type(exc).__name__)
                result["errors"].append("rss")
        result["briefing"] = (now.strftime("%A, %B %d") + "\n" + result["weather"] + "\n\n" + "\n".join("- " + h for h in headlines[:5]) + "\n\nWhat is one small thing you can finish today?\nWeather: Open-Meteo. Headlines: configured RSS sources.")[:1800]
        base = config.get("home_assistant_url", "").rstrip("/")
        if base:
            for entity in config.get("home_entities", [])[:5]:
                try:
                    data = fetch(base + "/api/states/" + entity, headers=ha_headers()).json()
                    allowed = entity in config.get("controllable_entities", []) and entity.split(".")[0] in ("light", "switch")
                    result["home"].append({"id": entity, "name": data.get("attributes", {}).get("friendly_name", entity)[:55], "state": str(data["state"])[:40], "controllable": allowed})
                except Exception as exc:
                    LOG.warning("Home entity fetch failed: %s", type(exc).__name__)
                    result["home"].append({"id": entity, "name": entity, "state": "unavailable", "controllable": False})
        return result

    def refresh_feed():
        try:
            data = build_feed()
            with lock:
                cache["data"] = data
                cache["at"] = time.monotonic()
        finally:
            with lock:
                cache["refreshing"] = False

    @app.get("/feed")
    def feed():
        with lock:
            if (time.monotonic() - cache["at"] > 300 or not cache["data"]) and not cache["refreshing"]:
                cache["refreshing"] = True
                threading.Thread(target=refresh_feed, daemon=True).start()
            result = dict(cache["data"]) if cache["data"] else {"weather": "Fetching connected data...", "calendar": "Calendar loading", "briefing": "The bridge is fetching your first briefing. This page will update in about a minute.", "home": []}
            result["refreshing"] = cache["refreshing"]
            return jsonify(result)

    @app.post("/toggle")
    def toggle():
        data = request.get_json(silent=True) or {}
        entity = data.get("entity_id", "")
        if entity not in config.get("controllable_entities", []) or entity not in config.get("home_entities", []) or entity.split(".")[0] not in ("light", "switch"):
            return jsonify(error="Entity not allowed"), 403
        try:
            response = requests.post(config["home_assistant_url"].rstrip("/") + "/api/services/" + entity.split(".")[0] + "/toggle", headers=ha_headers(), json={"entity_id": entity}, timeout=8)
            response.raise_for_status()
            cache["at"] = 0
            return jsonify(ok=True)
        except requests.RequestException:
            return jsonify(error="Home Assistant unavailable"), 502

    @app.post("/voice")
    def voice():
        nonlocal model
        if not config.get("transcription_model"):
            return jsonify(error="Set transcription_model and install faster-whisper"), 503
        wav = request.get_data()
        if len(wav) < 44 or wav[:4] != b"RIFF" or wav[8:12] != b"WAVE":
            return jsonify(error="Expected WAV audio"), 400
        try:
            with model_lock:
                if model is None:
                    from faster_whisper import WhisperModel
                    model = WhisperModel(config["transcription_model"], device="cpu", compute_type="int8")
                segments, _ = model.transcribe(io.BytesIO(wav), beam_size=1, vad_filter=True)
                text = " ".join(segment.text.strip() for segment in segments)
            return jsonify(text=text[:4000])
        except Exception as exc:
            LOG.warning("Transcription failed: %s", type(exc).__name__)
            return jsonify(error="Transcription unavailable; WAV remains on board"), 503

    @app.get("/health")
    def health():
        return jsonify(ok=True, version="0.1.0")

    return app

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default="config.json")
    args = parser.parse_args()
    config = json.loads(Path(args.config).read_text())
    logging.basicConfig(level=logging.INFO)
    from waitress import serve
    print(f'Wisp bridge on port {config.get("port", 8787)}. Keep this terminal running.')
    serve(create_app(config), host=config.get("host", "0.0.0.0"), port=config.get("port", 8787), threads=4)
