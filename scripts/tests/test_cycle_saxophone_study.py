import copy
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from cycle_reference_study import file_digest
from cycle_saxophone_study import (DEFAULT_PLAN, RECIPE, assert_peak_only, assert_unused_cc118,
                                   economy, load_plan, named, peak_edit)


class SaxophoneStudyTest(unittest.TestCase):
    def setUp(self):
        self.plan = load_plan(DEFAULT_PLAN)
        self.graph = json.loads(Path(self.plan["baselinePath"]).read_text())
        self.controls = {"nodeId": "volumeEnvelope1", "model": named(self.graph["nodes"], "volumeEnvelope1")["model"]}

    def fixture(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        content = Path(temporary.name) / "content"
        directory = content / "studies/expressive-sax"
        directory.mkdir(parents=True)
        for name in ("plan.json", "references.json", "objective-suite.json", "withheld-suite.json"):
            shutil.copyfile(DEFAULT_PLAN.parent / name, directory / name)
        for name in ("presets", "references"):
            (content / name).symlink_to(DEFAULT_PLAN.parents[2] / name, target_is_directory=True)
        return directory / "plan.json"

    def change(self, plan, name, mutate):
        path = plan.parent / name
        document = json.loads(path.read_text())
        mutate(document)
        path.write_text(json.dumps(document))

    def candidate(self):
        changed = copy.deepcopy(self.graph)
        named(changed["nodes"], "volumeEnvelope1")["model"] = peak_edit(self.controls, RECIPE)["model"]
        return changed

    def test_plan_freezes_both_splits_and_every_declared_source_identity(self):
        self.assertEqual((len(self.plan["objectiveSuite"]["cases"]), len(self.plan["withheldSuite"]["cases"])), (6, 4))
        self.assertEqual((self.plan["objectiveSuite"]["renderCount"], self.plan["withheldSuite"]["renderCount"]), (12, 8))
        self.assertEqual(2 * (self.plan["objectiveSuite"]["audioSeconds"] + self.plan["withheldSuite"]["audioSeconds"]), 80)
        self.assertEqual(len(self.plan["identities"]), 6)
        for path, digest in self.plan["identities"].items():
            self.assertEqual(file_digest(path), digest)
        json.dumps(self.plan, allow_nan=False)
        self.assertTrue(any("no independent withheld" in item for item in self.plan["document"]["missingCoverage"]))

    def test_peak_edit_preserves_inspection_and_changes_only_reviewed_peak_coordinates(self):
        original = copy.deepcopy(self.controls)
        edit = peak_edit(self.controls, RECIPE)
        self.assertEqual(self.controls, original)
        self.assertEqual(edit["expectedModelRevision"], original["model"]["revision"])
        self.assertEqual(edit["model"]["revision"], original["model"]["revision"] + 1)
        changed = self.candidate()
        assert_peak_only(self.graph, changed, RECIPE)
        vertices = named(changed["nodes"], "volumeEnvelope1")["model"]["state"]["mesh"]["mainMesh"]["vertices"]
        before = original["model"]["state"]["mesh"]["mainMesh"]["vertices"]
        for vertex in vertices:
            source = named(before, vertex["id"])
            self.assertEqual(vertex["phase"], source["phase"] * (2 if vertex["id"] in RECIPE["vertexIds"] else 1))

    def test_peak_proof_rejects_topology_markers_gain_other_models_and_extra_revisions(self):
        for mutate in (
                lambda graph: graph["edges"].pop(),
                lambda graph: graph["guides"][0].update(noise=.7),
                lambda graph: named(graph["nodes"], "output")["parameters"].update(gain=.7),
                lambda graph: named(graph["nodes"], "volumeEnvelope1")["model"]["state"]["mesh"].update(sustainIndices=[]),
                lambda graph: named(graph["nodes"], "volumeEnvelope1")["model"].update(revision=99),
                lambda graph: named(graph["nodes"], "volumeEnvelope1")["model"]["state"]["mesh"]["mainMesh"]["vertices"][8].update(amp=.9)):
            changed = self.candidate()
            mutate(changed)
            with self.assertRaises(ValueError):
                assert_peak_only(self.graph, changed, RECIPE)

    def test_recipe_and_source_geometry_cannot_silently_expand_the_intervention(self):
        for field, value in (("phaseFactor", 3), ("cubeIndex", True), ("vertexIds", [8, 9]), ("nodeId", "pitchEnvelope1")):
            with self.subTest(field=field), self.assertRaises(ValueError):
                peak_edit(self.controls, {**RECIPE, field: value})
        for field, value in (("amp", .9), ("phase", float("nan")), ("phase", .4)):
            altered = copy.deepcopy(self.controls)
            altered["model"]["state"]["mesh"]["mainMesh"]["vertices"][8][field] = value
            with self.assertRaises(ValueError):
                peak_edit(altered, RECIPE)
        altered = copy.deepcopy(self.controls)
        altered["model"]["state"]["revision"] += 1
        with self.assertRaises(ValueError):
            peak_edit(altered, RECIPE)

    def test_declared_source_change_and_missing_coverage_fail_before_authoring(self):
        for mutate in (lambda value: value.update(sourceSha256="0" * 64),
                       lambda value: value.update(missingCoverage=[]),
                       lambda value: value.update(baseline="../../presets/alto-sax.cyclegraph"),
                       lambda value: value.update(candidate={**RECIPE, "phaseFactor": 1.5})):
            plan = self.fixture()
            self.change(plan, "plan.json", mutate)
            with self.assertRaises(ValueError):
                load_plan(plan)

    def test_wrong_missing_and_duplicate_case_ids_cannot_cross_the_frozen_split(self):
        for name, mutate in (("objective-suite.json", lambda value: value["cases"].pop()),
                             ("objective-suite.json", lambda value: value["cases"].append(copy.deepcopy(value["cases"][0]))),
                             ("withheld-suite.json", lambda value: value["cases"][0].update(id="anchor"))):
            plan = self.fixture()
            self.change(plan, name, mutate)
            with self.assertRaises(ValueError):
                load_plan(plan)

    def test_native_rate_seed_repetitions_and_explicit_initial_controls_are_required(self):
        for mutate in (lambda value: value.update(repetitions=1),
                       lambda value: value["render"].update(sampleRate=48000),
                       lambda value: value["render"].update(randomSeed=42),
                       lambda value: value["cases"][0]["events"].pop(0),
                       lambda value: value["cases"][0]["events"][2].update(value=1)):
            plan = self.fixture()
            self.change(plan, "objective-suite.json", mutate)
            with self.assertRaises(ValueError):
                load_plan(plan)

    def test_schedule_control_retains_same_notes_state_times_and_only_unused_target(self):
        for mutate in (lambda value: value["cases"][-1]["events"][4].update(controller=1),
                       lambda value: value["cases"][-1]["events"][4].update(controller=2),
                       lambda value: value["cases"][-1]["events"][4].update(timeMs=201),
                       lambda value: value["cases"][-1]["events"][0].update(value=1),
                       lambda value: value["cases"][-2]["events"][4].update(value=63),
                       lambda value: value["cases"][3]["events"][3].update(velocity=.6)):
            plan = self.fixture()
            self.change(plan, "objective-suite.json", mutate)
            with self.assertRaises(ValueError):
                load_plan(plan)
        altered = copy.deepcopy(self.graph)
        named(altered["nodes"], "modulationOverride1")["parameters"]["blueController"] = 118
        with self.assertRaises(ValueError):
            assert_unused_cc118(altered)

    def test_unreleased_notes_late_releases_and_cc3_motion_are_rejected(self):
        for mutate in (lambda value: value["cases"][0]["events"].pop(),
                       lambda value: value["cases"][0]["events"][-1].update(timeMs=1500),
                       lambda value: value["cases"][1]["events"][4].update(controller=3)):
            plan = self.fixture()
            self.change(plan, "withheld-suite.json", mutate)
            with self.assertRaises(ValueError):
                load_plan(plan)

    def test_reference_remains_one_existing_recording_instead_of_a_fabricated_holdout(self):
        for mutate in (lambda value: value["references"].append({**value["references"][0], "id": "withheld"}),
                       lambda value: value["references"][0].pop("source"),
                       lambda value: value["references"][0].update(referenceId="factory:different-recording"),
                       lambda value: value["references"][0]["labels"]["articulation"].update(origin="observed")):
            plan = self.fixture()
            self.change(plan, "references.json", mutate)
            with self.assertRaises(ValueError):
                load_plan(plan)

    def test_economy_exposes_large_pitch_geometry_and_is_unchanged_by_peak_candidate(self):
        counts = economy(self.graph)
        self.assertEqual((counts["nodeCount"], counts["edgeCount"], counts["guideCount"]), (23, 28, 5))
        pitch = next(item for item in counts["envelopes"] if item["nodeId"] == "pitchEnvelope1")
        self.assertEqual((pitch["vertexCount"], pitch["cubeCount"]), (840, 104))
        self.assertEqual((counts["meshVertexCount"], counts["meshCubeCount"]), (1176, 147))
        self.assertEqual(counts, economy(self.candidate()))


if __name__ == "__main__":
    unittest.main()
