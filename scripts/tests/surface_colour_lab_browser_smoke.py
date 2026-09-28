"""Optional browser check: install playwright, start the lab, then run this script."""

import argparse
import json
from pathlib import Path

from playwright.sync_api import sync_playwright


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8765")
    parser.add_argument("--screenshot", default="/tmp/surface-colour-lab-ui.png")
    parser.add_argument("--chrome", default="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")
    args = parser.parse_args()
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(executable_path=args.chrome)
        page = browser.new_page(viewport={"width": 1600, "height": 1100})
        errors = []
        page.on("pageerror", lambda error: errors.append(str(error)))
        page.goto(args.url)
        page.wait_for_function("lastRender && !busy")
        baseline = page.locator("#result").get_attribute("src")
        assert baseline == page.locator("#reference").get_attribute("src")
        page.locator("#add").click()
        page.wait_for_function("!busy && lastRender.layers.length === 2")
        assert baseline != page.locator("#result").get_attribute("src")
        page.locator("#layer-filter").select_option("highpass")
        page.locator("#palette").select_option("Inferno-like")
        page.locator("#applyPalette").click()
        page.wait_for_timeout(1000)
        page.wait_for_function("!busy")
        page.screenshot(path=args.screenshot, full_page=True)
        page.get_by_label("Enable Detail", exact=True).uncheck()
        page.wait_for_function("!busy && lastRender.layers[1] === null")
        assert baseline == page.locator("#result").get_attribute("src")
        with page.expect_download() as pending:
            page.locator("#saveRecipe").click()
        saved = json.loads(Path(pending.value.path()).read_text())
        assert saved["layers"][1]["filter"] == "highpass"
        page.locator("#recipeFile").set_input_files({
            "name": "recipe.json", "mimeType": "application/json",
            "buffer": json.dumps(saved).encode(),
        })
        page.wait_for_timeout(1000)
        page.wait_for_function("!busy")
        assert baseline == page.locator("#result").get_attribute("src")
        with page.expect_download() as pending:
            page.locator("#export").click()
        assert Path(pending.value.path()).read_bytes().startswith(b"\x89PNG\r\n\x1a\n")
        page.set_viewport_size({"width": 700, "height": 900})
        assert page.evaluate("document.documentElement.scrollWidth <= innerWidth")
        assert not errors, errors
        browser.close()
    print("Browser editing, layer identity, recipe reload, PNG export and responsive layout passed.")


if __name__ == "__main__":
    main()
