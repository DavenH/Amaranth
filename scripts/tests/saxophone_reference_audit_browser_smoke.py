"""Check the generated local reference listening page with Playwright and Chrome."""

import argparse
import json
from pathlib import Path

from playwright.sync_api import sync_playwright


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--page", type=Path, required=True)
    parser.add_argument("--screenshot", type=Path, required=True)
    parser.add_argument("--chrome", default="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")
    args = parser.parse_args()
    errors = []
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(executable_path=args.chrome)
        page = browser.new_page(viewport={"width": 1440, "height": 1000}, accept_downloads=True)
        page.on("pageerror", lambda error: errors.append(str(error)))
        media_requests = []
        page.on("request", lambda request: media_requests.append(request.url)
                if "/media/" in request.url else None)
        page.goto(args.page.resolve().as_uri())
        catalog = json.loads(page.locator("#catalog-data").text_content())
        playable = [track for track in catalog["tracks"] if track["available"]]
        comparisons = {pair["referenceId"]: pair for pair in catalog.get("comparisons", [])}
        bridge = catalog.get("bridge", {})
        bridge_clips = bridge.get("clips", [])

        def check_audio(clip, identifier):
            page.wait_for_function("""() => {
                const audio = document.querySelector('audio');
                return !audio.paused && Number.isFinite(audio.duration) && audio.currentTime > .03;
            }""")
            actual = page.locator("audio").evaluate("audio => ({duration:audio.duration,rate:audio.playbackRate})")
            assert abs(actual["duration"] - clip["durationSeconds"]) < .15, (identifier, actual)
            assert actual["rate"] == 1
            assert page.locator("audio").evaluate(
                "(audio, url) => audio.currentSrc === new URL(url, location.href).href", clip["url"]), identifier
            assert page.locator("audio").count() == 1

        page.wait_for_function("document.querySelectorAll('.track').length > 0")
        assert page.locator(".track").count() == len(playable)
        assert page.locator("audio").count() == 1
        assert page.locator("#audio-player").get_attribute("src") is None
        assert media_requests == [], media_requests
        compact_rows = page.locator("#track-list .track").evaluate_all("""rows => rows.map(row => {
            const play = row.querySelector('.play-track').getBoundingClientRect();
            const notes = row.querySelector('td.track-details .notes-toggle').getBoundingClientRect();
            const synth = row.querySelector('.synth-track')?.getBoundingClientRect();
            return {id: row.dataset.trackId, tag: row.tagName, height: row.getBoundingClientRect().height,
                    column: row.lastElementChild.classList.contains('track-details'),
                    centerDifference: Math.abs((play.top + play.bottom - notes.top - notes.bottom) / 2),
                    synthCenterDifference: synth ? Math.abs((play.top + play.bottom - synth.top - synth.bottom) / 2) : 0};
        })""")
        for row in compact_rows:
            assert row["tag"] == "TR" and row["column"], row
            assert row["height"] <= 48, row
            assert row["centerDifference"] <= 1, row
            assert row["synthCenterDifference"] <= 1, row
        for track in playable:
            row = page.locator('.track[data-track-id="' + track["id"] + '"]')
            if track["reserved"]:
                assert row.locator(".availability").text_content() == "Reserved reference"
            pair = comparisons.get(track["id"])
            clip = pair["source"] if pair else track
            row.locator(".play-track").click()
            check_audio(clip, track["id"])
            if track["source"] in ("Iowa", "Solo"):
                target = min(20, clip["durationSeconds"] / 2)
                page.locator("audio").evaluate("(audio,time) => {audio.currentTime=time}", target)
                page.wait_for_function("document.querySelector('audio').currentTime >= " + str(target), timeout=3000)
            page.locator("audio").evaluate("audio => audio.pause()")
            if pair:
                row.locator(".synth-track").click()
                check_audio(pair["synth"], pair["renderId"])
                assert row.locator(".synth-track").get_attribute("aria-current") == "true"
                assert row.locator(".play-track").get_attribute("aria-current") is None
                page.locator("audio").evaluate("audio => audio.pause()")
                row.locator(".notes-toggle").click()
                dialog = page.locator("#recording-details")
                content = dialog.text_content()
                assert pair["renderId"] in content
                assert pair["source"]["sha256"] in content
                assert pair["synth"]["sha256"] in content
                if pair.get("note"):
                    assert pair["note"] in content
                if pair.get("rawSynth"):
                    dialog.get_by_role("button", name="Raw synth level", exact=True).click()
                    check_audio(pair["rawSynth"], pair["renderId"] + ":raw")
                    page.locator("audio").evaluate("audio => audio.pause()")
                else:
                    page.locator("#close-details").click()
        assert page.locator("#bridge-auditions").is_visible() == bool(bridge_clips)
        for clip in bridge_clips:
            button = page.locator('#bridge-clips [data-clip-id="' + clip["id"] + '"]')
            button.click()
            check_audio(clip, clip["renderId"])
            target = clip["durationSeconds"] * .65
            page.locator("audio").evaluate("(audio,time) => {audio.currentTime=time}", target)
            page.wait_for_function("""time => {
                const audio = document.querySelector('audio');
                return !audio.seeking && !audio.paused && audio.currentTime >= time;
            }""", arg=target)
            assert button.get_attribute("aria-current") == "true"
            assert page.locator('#bridge-clips [aria-current="true"]').count() == 1
            assert page.locator('.track.active').count() == 0
            page.locator("audio").evaluate("audio => audio.pause()")
        if bridge_clips:
            page.locator("#bridge-clips").get_by_role("button", name="Notes", exact=True).click()
            dialog = page.locator("#recording-details")
            assert dialog.evaluate("dialog => dialog.open")
            content = dialog.text_content()
            if bridge.get("note"):
                assert bridge["note"] in content
            for clip in bridge_clips:
                assert clip["renderId"] in content
                assert clip["sha256"] in content
            dialog.locator(".audit-note textarea").fill("Bridge movement is continuous; compare the midpoint.")
            page.locator("#close-details").click()
        page.locator("#source-filter").select_option("VCSL")
        assert page.locator(".track").count() == 12
        page.locator("#technique-filter").select_option("Staccato")
        assert page.locator(".track").count() == 3
        page.locator("#search").fill("C3")
        assert page.locator(".track").count() == 2
        first = page.locator(".track").first
        identifier = first.get_attribute("data-track-id")
        first.locator('.vote[data-choice="keep"]').click()
        first.locator(".notes-toggle").click()
        dialog = page.locator("#recording-details")
        assert dialog.evaluate("dialog => dialog.open")
        dialog.locator(".audit-note textarea").fill("Clear short attack; useful reference.")
        page.locator("#close-details").click()
        assert not dialog.evaluate("dialog => dialog.open")
        page.reload()
        first = page.locator('.track[data-track-id="' + identifier + '"]')
        assert first.locator('.vote[data-choice="keep"]').get_attribute("aria-pressed") == "true"
        first.locator(".notes-toggle").click()
        assert dialog.evaluate("dialog => dialog.open")
        assert dialog.locator(".audit-note textarea").input_value() == "Clear short attack; useful reference."
        original = next(track for track in playable if track["id"] == identifier)
        assert original["sha256"] in dialog.text_content()
        page.keyboard.press("Escape")
        assert not dialog.evaluate("dialog => dialog.open")
        first.locator(".play-track").click()
        page.locator("#next").click()
        page.wait_for_function("document.querySelector('.track.active').dataset.trackId !== " + json.dumps(identifier))
        page.locator("#repeat").click()
        assert page.locator("audio").evaluate("audio => audio.loop")
        page.locator("audio").evaluate("audio => audio.pause()")
        page.locator("#available-only").uncheck()
        for track in catalog["tracks"]:
            if not track["available"]:
                assert page.locator('.track[data-track-id="' + track["id"] + '"] .play-track').is_disabled()
        with page.expect_download() as downloaded:
            page.locator("#export-audit").click()
        exported = json.loads(Path(downloaded.value.path()).read_text())
        assert any(track["id"] == identifier and track.get("choice") == "keep"
                   and track.get("note") == "Clear short attack; useful reference."
                   for track in exported["recordings"])
        if comparisons:
            assert exported["comparisons"] == catalog["comparisons"]
        if bridge_clips:
            assert exported["bridge"]["audit"]["note"] == "Bridge movement is continuous; compare the midpoint."
            assert [clip["renderId"] for clip in exported["bridge"]["clips"]] == [clip["renderId"] for clip in bridge_clips]
            assert [clip["sha256"] for clip in exported["bridge"]["clips"]] == [clip["sha256"] for clip in bridge_clips]
        page.evaluate("localStorage.clear()")
        page.reload()
        page.locator(".track .play-track").first.click()
        page.wait_for_function("document.querySelector('audio').currentTime > 0.05")
        page.locator("audio").evaluate("audio => audio.pause()")
        page.evaluate("scrollTo(0, 0)")
        page.wait_for_timeout(200)
        page.screenshot(path=str(args.screenshot), full_page=False)
        page.set_viewport_size({"width": 740, "height": 900})
        page.evaluate("scrollTo(0, 0)")
        page.wait_for_timeout(300)
        assert page.evaluate("document.documentElement.scrollWidth <= innerWidth")
        page.screenshot(path=str(args.screenshot.with_name("saxophone-reference-audit-narrow.png")))
        assert not errors, errors
        browser.close()
    print(f"PASS: {len(playable)} recordings, {len(comparisons)} synth comparisons and {len(bridge_clips)} bridge clips played; seeking, filters, compact rows, notes dialog, export and responsive layout checked")


if __name__ == "__main__":
    main()
