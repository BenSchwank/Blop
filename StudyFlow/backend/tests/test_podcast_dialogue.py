import unittest

from podcast_dialogue import (
    breath_segments,
    gap_seconds_between,
    parse_podcast_dialogue,
    podcast_voice_pair,
    spoken_podcast_line,
    split_spoken_and_cues,
    tts_instructions_for_turn,
)


class PodcastDialogueTests(unittest.TestCase):
    def test_labeled_turns_keep_speakers(self):
        script = """
ALEX: Was bedeutet das?
SAM: Kurz gesagt: eine Regel mit einem Beispiel.
ALEX: Und warum ist das wichtig?
SAM: Weil man damit den nächsten Schritt selbst lösen kann.
"""
        turns = parse_podcast_dialogue(script)
        self.assertEqual([speaker for speaker, _ in turns], ["ALEX", "SAM", "ALEX", "SAM"])
        self.assertIn("Regel", turns[1][1])

    def test_multiline_turn_stays_with_the_same_person(self):
        script = "ALEX: Erste Frage.\nKannst du das nochmal sagen?\nSAM: Ja, gerne."
        turns = parse_podcast_dialogue(script)
        self.assertEqual(turns[0][0], "ALEX")
        self.assertIn("nochmal", turns[0][1])
        self.assertEqual(turns[1][0], "SAM")

    def test_unlabeled_text_becomes_two_speakers(self):
        script = "Das ist der erste Gedanke. Hier kommt der zweite Satz.\n\nDanach erklärt jemand den Zusammenhang. Und nennt ein Beispiel."
        turns = parse_podcast_dialogue(script)
        speakers = {speaker for speaker, _ in turns}
        self.assertEqual(speakers, {"ALEX", "SAM"})

    def test_stage_directions_are_not_spoken(self):
        spoken = spoken_podcast_line("Also [lacht] das heißt <short pause> genau das.")
        self.assertNotIn("[lacht]", spoken)
        self.assertIn("<short pause>", spoken)
        self.assertIn("genau das", spoken)

    def test_emotion_tags_become_delivery_cues(self):
        spoken, cues = split_spoken_and_cues("ALEX would say: [überrascht] wirklich?")
        self.assertNotIn("[", spoken)
        self.assertIn("surprised", cues.lower())
        instructions = tts_instructions_for_turn("ALEX", "aoede", "[lacht] Okay, check.")
        self.assertIn("laugh", instructions.lower())
        self.assertIn("Alex", instructions)

    def test_breath_segments_split_long_turns(self):
        text = "Erster Satz hier. " * 20
        parts = breath_segments(text, max_chars=80)
        self.assertGreater(len(parts), 1)
        self.assertTrue(all(len(p) <= 100 for p in parts))

    def test_gaps_are_longer_between_speakers(self):
        same = gap_seconds_between("ALEX", "ALEX", "kurz")
        swap = gap_seconds_between("ALEX", "SAM", "kurz")
        long = gap_seconds_between("ALEX", "SAM", " ".join(["wort"] * 45))
        self.assertLess(same, swap)
        self.assertGreater(long, swap)

    def test_same_voice_choice_gets_a_second_person(self):
        self.assertEqual(podcast_voice_pair("aoede", "aoede"), ("aoede", "charon"))
        self.assertEqual(podcast_voice_pair("alloy", ""), ("aoede", "charon"))


if __name__ == "__main__":
    unittest.main()
